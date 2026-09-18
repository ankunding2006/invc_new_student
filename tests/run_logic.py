from pathlib import Path
import subprocess,os
root=Path(__file__).resolve().parents[1]
build=Path(os.environ.get('INVC_BUILD_DIR',str(root/'build/tests'))) ;build.mkdir(parents=True,exist_ok=True)
sender=root/'sender/Service';receiver=root/'receiver/Service'
exe=build/'logic.exe'
cmd=['gcc','-std=c99','-O2','-Wall','-Wextra','-Werror','-I',str(sender),'-I',str(receiver),str(root/'tests/test_logic.c')]
cmd += [str(sender/(n+'.c')) for n in ('srv_protocol','srv_link','srv_input','srv_imu_filter')]
cmd += [str(receiver/'srv_stats.c'),'-lm','-o',str(exe)]
subprocess.run(cmd,check=True);subprocess.run([str(exe)],check=True)
