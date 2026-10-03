"""Build both STM32F103C8 images with CMake and GNU Arm Embedded; no flashing.
Requires cmake, ninja, arm-none-eabi-gcc on PATH. Python 3 standard library only.
"""
from pathlib import Path
import subprocess, shutil, argparse

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description="Build sender and receiver firmware via CMake")
parser.add_argument('--out', type=Path, default=root / 'build/firmware', help="Output directory for firmware")
parser.add_argument('--config', type=str, default='Release', choices=['Debug', 'Release'], help="Build configuration preset")
args = parser.parse_args()

for side in ('sender', 'receiver'):
    p = root / side
    out = args.out.resolve() / side
    out.mkdir(parents=True, exist_ok=True)
    
    print(f"=== Configuring {side} ({args.config}) ===", flush=True)
    subprocess.run(['cmake', '--preset', args.config], cwd=str(p), check=True)
    
    print(f"=== Building {side} ({args.config}) ===", flush=True)
    subprocess.run(['cmake', '--build', '--preset', args.config], cwd=str(p), check=True)
    
    bld_dir = p / 'build' / args.config
    for ext in ('elf', 'hex', 'bin', 'map'):
        src_file = bld_dir / f"{side}.{ext}"
        if src_file.exists():
            shutil.copy2(src_file, out / f"{side}.{ext}")
            print(f"Copied {src_file.name} -> {out}", flush=True)

print("\nFirmware build completed successfully.", flush=True)
