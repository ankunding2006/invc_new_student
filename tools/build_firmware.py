"""Build both STM32F103C8 images with GNU Arm Embedded; no flashing.
Requires arm-none-eabi-gcc, objcopy and size on PATH. Python 3 standard library only.
"""
from pathlib import Path
import subprocess,xml.etree.ElementTree as ET,argparse
root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser();parser.add_argument('--out',type=Path,default=root/'build/firmware');args=parser.parse_args()
for side in ('sender','receiver'):
    p=root/side;out=args.out.resolve()/side;out.mkdir(parents=True,exist_ok=True)
    includes=[p/f for f in ('App','BSP','Service','Core/Inc','Drivers/STM32F1xx_HAL_Driver/Inc','Drivers/STM32F1xx_HAL_Driver/Inc/Legacy','Drivers/CMSIS/Device/ST/STM32F1xx/Include','Drivers/CMSIS/Include')]
    xml=ET.parse(p/'MDK-ARM'/f'{side}.uvprojx')
    sources={(p/'MDK-ARM'/n.text).resolve() for n in xml.findall('.//FilePath') if n.text.endswith('.c')}
    sources.update(f.resolve() for folder in ('App','BSP','Service') for f in (p/folder).glob('*.c'))
    sources.add((root/'tools/gcc_runtime.c').resolve())
    sources.add((p/'Drivers/CMSIS/Device/ST/STM32F1xx/Source/Templates/gcc/startup_stm32f103xb.s').resolve())
    objects=[]
    for i,source in enumerate(sorted(sources)):
        obj=out/(f'{i:02}_'+source.stem+'.o');objects.append(obj)
        cmd=['arm-none-eabi-gcc','-mcpu=cortex-m3','-mthumb','-mfloat-abi=soft','-Os','-g3','-std=c99','-ffunction-sections','-fdata-sections','-fstack-usage','-Wall','-Wextra','-Werror','-DUSE_HAL_DRIVER','-DSTM32F103xB']
        cmd += [arg for inc in includes for arg in ('-I',str(inc))]
        subprocess.run(cmd+['-c',str(source),'-o',str(obj)],check=True)
    elf=out/(side+'.elf')
    cmd=['arm-none-eabi-gcc','-mcpu=cortex-m3','-mthumb','-mfloat-abi=soft','-nostartfiles','--specs=nano.specs','--specs=nosys.specs','-T',str(root/'tools/STM32F103C8_FLASH.ld')]
    cmd += [str(o) for o in objects]
    cmd += ['-Wl,--gc-sections','-Wl,--print-memory-usage','-Wl,-Map='+str(out/(side+'.map')),'-Wl,--start-group','-lc','-lm','-lnosys','-Wl,--end-group','-o',str(elf)]
    print(side,flush=True);subprocess.run(cmd,check=True)
    subprocess.run(['arm-none-eabi-size',str(elf)],check=True)
    for fmt,ext in (('ihex','hex'),('binary','bin')):subprocess.run(['arm-none-eabi-objcopy','-O',fmt,str(elf),str(out/(side+'.'+ext))],check=True)
