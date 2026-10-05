import subprocess
import re
import sys
from pathlib import Path

def get_tokens(code_str):
    # Remove block comments
    s = re.sub(r'/\*.*?\*/', ' ', code_str, flags=re.DOTALL)
    # Remove line comments
    s = re.sub(r'//.*', ' ', s)
    # Match identifiers/keywords, numeric literals (hex, float, int with suffixes), operators, punctuation
    pattern = r'[a-zA-Z_][a-zA-Z0-9_]*|0x[0-9a-fA-F]+[uUlL]*|\d+\.\d+(?:[eE][+-]?\d+)?[fFlL]?|\d+(?:[eE][+-]?\d+)?[uUlLfF]*|[^\s\w]'
    return re.findall(pattern, s)

def verify_file(filepath):
    # Get HEAD version from git
    git_cmd = ['git', 'show', f'HEAD:{filepath}']
    res = subprocess.run(git_cmd, capture_output=True, text=True, encoding='utf-8', errors='ignore')
    if res.returncode != 0:
        print(f"Skipping {filepath} (not in git HEAD)")
        return True
    
    orig_text = res.stdout
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        curr_text = f.read()
    
    orig_tokens = get_tokens(orig_text)
    curr_tokens = get_tokens(curr_text)
    
    if orig_tokens != curr_tokens:
        print(f"[FAIL] Tokens mismatch in {filepath}!")
        print(f"  Orig tokens: {len(orig_tokens)}, Curr tokens: {len(curr_tokens)}")
        for idx, (t1, t2) in enumerate(zip(orig_tokens, curr_tokens)):
            if t1 != t2:
                print(f"  Diff at index {idx}: HEAD='{t1}' vs DISK='{t2}'")
                context_start = max(0, idx - 5)
                context_end = min(len(orig_tokens), idx + 6)
                print(f"  HEAD context: {' '.join(orig_tokens[context_start:context_end])}")
                print(f"  DISK context: {' '.join(curr_tokens[context_start:context_end])}")
                break
        return False
    return True

def main():
    root = Path(__file__).resolve().parents[1]
    all_ok = True
    tracked_files = subprocess.run(['git', 'ls-files', 'sender', 'receiver'], capture_output=True, text=True, cwd=root).stdout.splitlines()
    
    source_files = [f for f in tracked_files if f.endswith(('.c', '.cpp', '.h', '.hpp')) and 'Drivers' not in f and 'MDK-ARM' not in f and 'build' not in f]
    
    checked = 0
    for rel_path in source_files:
        full_path = root / rel_path
        if full_path.exists():
            if not verify_file(rel_path.replace('\\', '/')):
                all_ok = False
            checked += 1
            
    print(f"Checked {checked} source files.")
    if all_ok:
        print("ALL CHECKED FILES MATCH HEAD TOKENS EXACTLY! Zero code modification detected.")
    else:
        print("ERROR: Code modification detected in one or more files!")
        sys.exit(1)

if __name__ == '__main__':
    main()
