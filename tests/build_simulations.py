import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
def build():
    out=Path(os.environ.get('INVC_BUILD_DIR',str(root/'build/tests'))) ;out.mkdir(parents=True,exist_ok=True)
    for side in ('sender','receiver'):
        cmd=['gcc','-shared','-std=c99','-O2','-Wall','-Wextra','-Werror']
        if side=='sender':cmd+=['-DTEST_SENDER']
        for include in (root/'tests/mocks',root/side/'App',root/side/'BSP',root/side/'Service'):cmd+=['-I',str(include)]
        cmd += [str(root/'tests/sim_api.c'),str(root/'tests/mocks/mock_hal.c')]
        cmd += [str(p) for folder in ('App','BSP','Service') for p in (root/side/folder).glob('*.c')]
        subprocess.run(cmd+['-lm','-o',str(out/(side+'_sim.dll'))],check=True)
    return out
if __name__=='__main__':print(build())
