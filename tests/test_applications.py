import ctypes as C,struct
from build_simulations import build
out=build()
def module(side):
    lib=C.CDLL(str(out/(side+'_sim.dll')))
    lib.sim_init.argtypes=[C.c_uint32];lib.sim_step.argtypes=[C.c_uint32]
    lib.sim_value.argtypes=[C.c_uint];lib.sim_value.restype=C.c_uint32
    lib.sim_fault.argtypes=[C.c_uint,C.c_bool]
    lib.sim_set_joystick.argtypes=[C.c_uint16,C.c_uint16]
    lib.sim_init(0);return lib
s=module('sender');r=module('receiver');buf=(C.c_uint8*256)()
def take(lib,port):
    n=lib.sim_take_tx(port,buf,256);return bytes(buf[:n])
def receive(lib,port,data):
    block=(C.c_uint8*len(data)).from_buffer_copy(data);lib.sim_receive(port,block,len(data))
# Test each app independently before end-to-end integration.
for now in range(3100):
    s.sim_step(now);take(s,0);take(s,1)
assert s.sim_value(0)==2 and s.sim_value(6)==100
assert s.sim_value(3)>0 # absent ACKs expire; sensor tasks still run.
now=3100
key_logs=[]
def key(mask):
    global now
    s.sim_set_keys(mask)
    for _ in range(40):
        s.sim_step(now);take(s,0);key_logs.append(take(s,1));now+=1
key(4);key(0);assert s.sim_value(8)==1
key(8);key(0);assert s.sim_value(8)==0
assert b'K3 PRESS' in b''.join(key_logs) and b'K3 RELEASE' in b''.join(key_logs)
key(2);key(0);assert s.sim_value(7)==1
s.sim_set_joystick(0,4095)
for _ in range(30):s.sim_step(now);take(s,0);take(s,1);now+=1
assert C.c_int32(s.sim_value(4)).value==-1000
s.sim_fault(0,True)
for _ in range(50):s.sim_step(now);take(s,0);take(s,1);now+=1
assert s.sim_value(0)==3 and s.sim_value(9)&0x10
s.sim_fault(0,False)
for _ in range(4000):s.sim_step(now);take(s,0);take(s,1);now+=1
assert s.sim_value(0)==2
payload=struct.pack('<hhHHBBhhh',-500,500,1000,2000,0x0F,3,100,-100,1800)
def frame(seq):
    body=bytes([seq,16,1])+payload;return b'\xaa\x55'+body+bytes([sum(body)&255,13])
receive(r,0,frame(0));r.sim_step(1)
assert r.sim_value(0)==1 and r.sim_value(1)==1
assert len(take(r,0))==9
pc=take(r,1);assert len(pc)==60 and pc[-4:]==b'\0\0\x80\x7f'
vals=struct.unpack('<14f',pc[:-4]);assert vals[:2]==(-500,500) and vals[6:9]==(10,-10,180)
receive(r,0,frame(0));r.sim_step(2);assert r.sim_value(1)==1 and r.sim_value(2)==1 and take(r,1)==b''
take(r,0);r.sim_step(1003);assert r.sim_value(0)==2;take(r,1)
receive(r,1,b'T');receive(r,0,frame(1));r.sim_step(1004);take(r,0);assert take(r,1).startswith(b'R id=1')
# PC backpressure drops only a forwarding attempt; radio parsing/ACK remain live.
receive(r,0,frame(2));r.sim_step(1010);take(r,0)
receive(r,0,frame(3));r.sim_step(1011);take(r,0)
assert r.sim_value(1)==4 and r.sim_value(4)==1
assert take(r,1).startswith(b'R id=2')
receive(r,0,frame(4));r.sim_step(1012);take(r,0)
assert take(r,1).startswith(b'R id=4')
# A receiver reboot accepts a sender already partway through its sequence.
r.sim_init(2000);receive(r,0,frame(123));r.sim_step(2001)
assert r.sim_value(1)==1 and r.sim_value(3)==0 and len(take(r,0))==9
assert len(take(r,1))==60
print('Step 13: app startup/calibration/degraded/recovery, hierarchical menu, receiver duplicate/offline and PC formats PASS')
