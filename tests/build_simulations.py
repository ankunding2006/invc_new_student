"""Compile C services/mock HAL as C99 and App as C++17, testing the real ABI."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[1]


def build():
    out = Path(os.environ.get('INVC_BUILD_DIR', str(root / 'build/tests')))
    out.mkdir(parents=True, exist_ok=True)
    for side in ('sender', 'receiver'):
        objdir = out / (side + '_objects')
        objdir.mkdir(exist_ok=True)
        flags = ['-O2', '-Wall', '-Wextra', '-Werror']
        if side == 'sender':
            flags += ['-DTEST_SENDER']
        for include in (root / 'tests/mocks', root / side / 'App',
                        root / side / 'BSP', root / side / 'Service'):
            flags += ['-I', str(include)]
        sources = [root / 'tests/sim_api.c', root / 'tests/mocks/mock_hal.c']
        for folder in ('App', 'BSP', 'Service'):
            sources += sorted((root / side / folder).glob('*.c'))
            sources += sorted((root / side / folder).glob('*.cpp'))
        objects = []
        for index, source in enumerate(sources):
            obj = objdir / f'{index:02}_{source.stem}.o'
            cpp = source.suffix == '.cpp'
            command = (['g++', '-std=c++17', '-fno-exceptions', '-fno-rtti',
                        '-fno-threadsafe-statics'] if cpp else ['gcc', '-std=c99'])
            subprocess.run(command + flags + ['-c', str(source), '-o', str(obj)], check=True)
            objects.append(str(obj))
        subprocess.run(['g++', '-shared', *objects, '-lm', '-o',
                        str(out / (side + '_sim.dll'))], check=True)
    return out


if __name__ == '__main__':
    print(build())
