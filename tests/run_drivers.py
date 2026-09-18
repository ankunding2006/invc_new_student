import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1];mocks=root/'tests/mocks'
build=Path(os.environ.get('INVC_BUILD_DIR',str(root/'build/tests'))) ;build.mkdir(parents=True,exist_ok=True);exe=build/'drivers.exe'
cmd=['gcc','-std=c99','-O2','-Wall','-Wextra','-Werror']
for inc in (mocks,root/'sender/BSP',root/'sender/Service'):cmd+=['-I',str(inc)]
cmd += [str(root/'tests/test_drivers.c'),str(mocks/'mock_hal.c')]
cmd += [str(p) for p in (root/'sender/BSP').glob('*.c')]
cmd += [str(root/'sender/Service'/(name+'.c')) for name in ('srv_ring_buffer','srv_input')]
subprocess.run(cmd+['-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
