"""Optional visual QA; Pillow required. Renders actual mocked-app framebuffers."""
from pathlib import Path
import ctypes as C,struct
from PIL import Image,ImageDraw
from build_simulations import build
out=build();buf=(C.c_uint8*1024)()
s=C.CDLL(str(out/'sender_sim.dll'));r=C.CDLL(str(out/'receiver_sim.dll'))
s.sim_init(0);r.sim_init(0);now=0
def steps(n):
 global now
 for _ in range(n):
  s.sim_step(now);r.sim_step(now)
  for lib in (s,r):
   for port in (0,1):lib.sim_take_tx(port,buf,1024)
  now+=1
def capture(lib):
 lib.sim_screen(buf);data=bytes(buf);im=Image.new('RGB',(128,64),'black')
 for y in range(64):
  for x in range(128):
   if data[(y//8)*128+x]&(1<<(y%8)):im.putpixel((x,y),(220,245,255))
 return im.resize((512,256),Image.Resampling.NEAREST)
def press(mask):
 s.sim_set_keys(mask);steps(50);s.sim_set_keys(0);steps(160)
steps(3000)
images=[('Sender / menu',capture(s))]
press(4);images.append(('Sender / joystick',capture(s)))
for label in ('keys','attitude','communication'):
 press(2);images.append(('Sender / '+label,capture(s)))
payload=struct.pack('<hhHHBBhhh',-500,500,1000,2000,3,2,-451,301,1800)
body=bytes([1,16,1])+payload;data=b'\xaa\x55'+body+bytes([sum(body)&255,13]);wire=(C.c_uint8*len(data)).from_buffer_copy(data)
r.sim_receive(0,wire,len(data));steps(160);images.append(('Receiver / live',capture(r)))
sheet=Image.new('RGB',(1064,3*294),(35,40,46));draw=ImageDraw.Draw(sheet)
for i,(label,im) in enumerate(images):
 x=12+(i%2)*528;y=8+(i//2)*294;draw.text((x,y),label,fill='white');sheet.paste(im,(x,y+22))
sheet.save(out/'oled_screens.png');print(out/'oled_screens.png')
