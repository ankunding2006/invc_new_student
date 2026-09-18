"""Both real application/driver stacks, mocked HAL only; no physical radio."""
import ctypes as C,heapq,struct,json
from build_simulations import build
out=build();buf=(C.c_uint8*256)()
def load(name):
    lib=C.CDLL(str(out/(name+'_sim.dll')))
    lib.sim_init.argtypes=[C.c_uint32];lib.sim_step.argtypes=[C.c_uint32]
    lib.sim_value.argtypes=[C.c_uint];lib.sim_value.restype=C.c_uint32
    lib.sim_fault.argtypes=[C.c_uint,C.c_bool]
    lib.sim_set_joystick.argtypes=[C.c_uint16,C.c_uint16]
    return lib
s=load('sender');r=load('receiver')
def take(lib,port):
    n=lib.sim_take_tx(port,buf,256);return bytes(buf[:n])
def receive(lib,data):
    block=(C.c_uint8*len(data)).from_buffer_copy(data);lib.sim_receive(0,block,len(data))
def run(duration,*,start=0,loss=False,outage=None,restart=None):
    s.sim_init(start);r.sim_init(start)
    queue=[];serial=0;counts=[0,0];pc=0;online_pc=0;offline_seen=False
    for elapsed in range(duration):
        now=(start+elapsed)&0xffffffff
        if restart is not None and elapsed==restart:s.sim_init(now)
        if elapsed==2800 or (restart is not None and elapsed==restart+100):s.sim_set_joystick(0,4095)
        blocked=outage and outage[0]<=elapsed<outage[1]
        while queue and queue[0][0]<=elapsed:
            _,_,target,data=heapq.heappop(queue)
            if not blocked:receive(s if target==0 else r,data)
        for index,lib in enumerate((s,r)):
            lib.sim_step(now)
            data=take(lib,0)
            if data:
                counts[index]+=1
                # Corrupt selected data attempts and lose some ACKs independently.
                drop=blocked or (loss and ((index==0 and counts[index]%13==0) or (index==1 and counts[index]%17==0)))
                if not drop:
                    if loss and index==0 and counts[index]%29==0:data=data[:10]+bytes([data[10]^1])+data[11:]
                    serial+=1;heapq.heappush(queue,(elapsed+4,serial,1-index,data))
            data=take(lib,1)
            if index==1 and data:
                assert len(data)==60 and data[-4:]==b'\0\0\x80\x7f'
                values=struct.unpack('<14f',data[:56]);assert all(abs(v)<1e7 for v in values)
                if values[13]==0:offline_seen=True
                else:online_pc+=1
                pc+=1
        if outage and elapsed==outage[1]-1:assert r.sim_value(0)==2
    assert s.sim_value(0)==2 and r.sim_value(0)==1
    assert C.c_int32(r.sim_value(5)).value==-1000
    if loss:assert s.sim_value(2)>0 and r.sim_value(2)>0
    if outage:assert offline_seen
    assert online_pc==r.sim_value(1)-r.sim_value(4)
    result={'duration_ms':duration,'data_attempts':counts[0],'acks_sent':counts[1],
        'unique_received':r.sim_value(1),'duplicates':r.sim_value(2),'inferred_lost':r.sim_value(3),
        'sender_retries':s.sim_value(2),'sender_expired':s.sim_value(3),'pc_frames':pc,
        'pc_drops':r.sim_value(4),'last_unique_hz':r.sim_value(6)/100}
    print(result);return result
results={}
results['clean']=run(7000)
assert results['clean']['last_unique_hz']==50
assert results['clean']['unique_received']>256 # exercises seq wrap end-to-end
results['loss']=run(9000,loss=True)
results['outage']=run(10000,outage=(4000,5600))
results['sender_restart']=run(11000,restart=4200)
assert results['sender_restart']['inferred_lost']==0
results['tick_wrap']=run(7000,start=0xfffff000)
(out/'integration_results.json').write_text(json.dumps(results,indent=2))
print('Step 14: both stacks clean/loss/ACK loss/outage/reboot/sequence wrap/tick wrap PASS')
