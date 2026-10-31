#!/usr/bin/env python3
"""Real camera-path encodes, MPEG-TS validation and loopback pacing. No RF output."""
import importlib.util
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import time

ROOT=Path(__file__).resolve().parents[2]
EXE=ROOT/'camera/bin/datv-camera'
OUT=ROOT/'camera/build'/('checks-'+str(time.time_ns()))
OUT.mkdir(parents=True)
spec=importlib.util.spec_from_file_location('bridge_checks', ROOT/'streambridge/checks/integration.py')
bridge=importlib.util.module_from_spec(spec)
spec.loader.exec_module(bridge)

def run(args, timeout=35):
    p=subprocess.run(list(map(str,args)),capture_output=True,text=True,timeout=timeout)
    assert p.returncode==0,(args,p.stdout,p.stderr)
    return p

def config(name, **values):
    p=OUT/(name+'.ini')
    p.write_text('[camera]\ncallsign=PE1ITR\n'+'\n'.join(f'{k}={v}' for k,v in values.items())+'\n')
    return p

def check(path,rate):
    bridge.check(path,rate,False,'PE1ITR')
    sections={0:[],1:[]}
    data=path.read_bytes()
    for n in range(len(data)//188):
        p=data[n*188:(n+1)*188]
        if ((p[1]&31)<<8)|p[2]!=18 or not p[3]&16: continue
        pos=5+p[4] if p[3]&32 else 4
        sec=p[pos+1+p[pos]:]
        assert sec[0]==0x4e
        sections[sec[6]].append(n)
    for num,positions in sections.items():
        assert positions,('EIT section missing',num)
        assert all((b-a)*1504/rate<=2 for a,b in zip(positions,positions[1:])),(num,positions)
    streams=json.loads(run(['ffprobe','-v','error','-show_streams','-of','json',path]).stdout)['streams']
    media=[s for s in streams if s['codec_type']!='data']
    assert len(media)==1 and media[0]['codec_type']=='video'
    assert all(s.get('codec_name')=='epg' for s in streams if s['codec_type']=='data')
    path.with_suffix('.probe.json').write_text(json.dumps(streams,indent=2))

def captures():
    cases=[
        ('s33',dict(system=0,symbol_rate_id=8,fec_id=0),30411),
        ('s35',dict(system=0,symbol_rate_id=0,fec_id=0),32254),
        ('s2_33',dict(system=1,symbol_rate_id=8,fec_id=0,pilots=1),31855),
        ('s2_35',dict(system=1,symbol_rate_id=0,fec_id=0,pilots=0),34610),
        ('t35',dict(system=2,bandwidth_khz=35,fec_id=1,guard_id=0),32254),
        ('t40',dict(system=2,bandwidth_khz=40,fec_id=0,guard_id=2),30160),
    ]
    for name,values,rate in cases:
        cfg=config(name,**values)
        for preset in range(3):
            path=OUT/f'{name}-{preset}.ts'
            p=run([EXE,'--headless','--config',cfg,'--test-source','--pattern','1','--preset',preset,'--seconds','7','--capture',path])
            path.with_suffix('.log').write_text(p.stderr)
            check(path,rate)
    # Scene complexity, several IDRs, periodic NIT/TDT collisions, lowest supported RF rate.
    path=OUT/'long-noise.ts'
    p=run([EXE,'--headless','--config',config('long',system=2,bandwidth_khz=40,fec_id=0,guard_id=2),
           '--test-source','--pattern','2','--preset','2','--seconds','60','--capture',path],timeout=75)
    path.with_suffix('.log').write_text(p.stderr);check(path,30160)
    # Keep independently decoded images for visual inspection of the permanent callsign.
    for preset in range(3):
        run(['ffmpeg','-v','error','-i',OUT/f's33-{preset}.ts','-frames:v','1',OUT/f'preview-{preset}.png'])

def network():
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sock:
        sock.bind(('127.0.0.1',0));sock.settimeout(.2)
        cfg=config('udp',port=sock.getsockname()[1])
        for restart in range(2):
            proc=subprocess.Popen([str(EXE),'--headless','--config',str(cfg),'--test-source','--seconds','8'],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            packets=[];times=[];end=time.monotonic()+20
            while time.monotonic()<end:
                try:
                    data,_=sock.recvfrom(8192);packets.append(data);times.append(time.monotonic())
                except socket.timeout:
                    if proc.poll() is not None:break
            if proc.poll() is None:proc.kill()
            _,err=proc.communicate(timeout=5);assert proc.returncode==0,err
            assert len(packets)>15 and all(len(p)==1316 for p in packets)
            bitrate=(len(times)-1)*1316*8/(times[-1]-times[0]);assert abs(bitrate/30411-1)<.02,bitrate
            gaps=[b-a for a,b in zip(times,times[1:])]
            assert min(gaps)>.25 and max(gaps)<.45,gaps
            path=OUT/f'udp-{restart}.ts';path.write_bytes(b''.join(packets));check(path,30411)
            path.with_suffix('.timing.json').write_text(json.dumps(dict(bitrate=bitrate,min_gap=min(gaps),max_gap=max(gaps)),indent=2))
        # Local preview must never open/send UDP, even when an address/port is configured.
        run([EXE,'--headless','--config',cfg,'--test-source','--preview','--seconds','3'])
        try:sock.recvfrom(8192);raise AssertionError('Preview sent UDP')
        except socket.timeout:pass
    proc=subprocess.Popen([str(EXE),'--headless','--test-source','--preview'],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    time.sleep(2);start=time.monotonic();proc.send_signal(signal.SIGTERM)
    _,err=proc.communicate(timeout=3);assert proc.returncode==0,err
    assert time.monotonic()-start<1.5
    print('OK loopback rate/pacing, restart, preview without UDP and stop')

def boundaries():
    for name,values,rate in [
        ('low-12fps',dict(system=2,bandwidth_khz=40,fec_id=0,guard_id=2,width=160,fps=12,gop=60),30160),
        ('high-wide-mono',dict(system=1,symbol_rate_id=5,fec_id=9,pilots=0,width=640,aspect=169,fps=12,gop=180,monochrome=1,denoise=2),894305),
    ]:
        path=OUT/(name+'.ts')
        p=run([EXE,'--headless','--config',config(name,**values),'--test-source','--pattern','2','--seconds','18','--capture',path])
        path.with_suffix('.log').write_text(p.stderr);check(path,rate)

def failures():
    for name,values in [('budget',dict(video_kbps=1000)),('width',dict(width=161)),('fec',dict(system=2,bandwidth_khz=35,fec_id=0)),('fps',dict(fps=13)),('call',dict(callsign='bad call'))]:
        # callsign is deliberately not duplicated in the INI.
        cfg=OUT/(name+'-bad.ini');cfg.write_text('[camera]\n'+'\n'.join(f'{k}={v}' for k,v in values.items()))
        p=subprocess.run([str(EXE),'--config',str(cfg),'--headless','--test-source','--preview'],capture_output=True,timeout=5)
        assert p.returncode!=0,(name,p.stderr)
    path=OUT/'existing.ts';path.write_bytes(b'keep-me')
    p=subprocess.run([str(EXE),'--headless','--test-source','--seconds','1','--capture',str(path)],capture_output=True,timeout=8)
    assert p.returncode!=0 and path.read_bytes()==b'keep-me'
    p=subprocess.run([str(EXE),'--headless','--device','/dev/video9999','--preview'],capture_output=True,timeout=10)
    assert p.returncode!=0
    print('OK invalid budget/RF/raster/callsign, protected capture, absent camera')

if __name__=='__main__':
    run([EXE,'--probe']);captures();boundaries();network();failures();print('Captures and logs:',OUT)
