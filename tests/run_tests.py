"""Run the software-only suite: ring buffers, pure logic, mocked drivers/apps/link.
Usage: python tests/run_tests.py [--build-dir PATH]
Requirements: Python 3, host GCC. Windows DLL simulations use ctypes.
No board is flashed; no serial port is opened.
"""
from pathlib import Path
import subprocess,sys,os,argparse
root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser();parser.add_argument('--build-dir',type=Path);args=parser.parse_args()
build=(args.build_dir or root/'build/tests').resolve();build.mkdir(parents=True,exist_ok=True)
if args.build_dir:os.environ['INVC_BUILD_DIR']=str(build)
for side in ('sender','receiver'):
    service=root/side/'Service';exe=build/(side+'_ring.exe')
    cmd=['gcc','-std=c99','-O2','-Wall','-Wextra','-Werror','-I',str(service),str(root/'tests/test_services.c'),str(service/'srv_ring_buffer.c'),'-o',str(exe)]
    subprocess.run(cmd,check=True);subprocess.run([str(exe)],check=True)
for script in ('run_logic.py','run_drivers.py','test_applications.py','test_integration.py'):
    subprocess.run([sys.executable,str(root/'tests'/script)],check=True)
print('ALL SOFTWARE CHECKS PASSED (HAL is mocked; physical hardware is untested).')
