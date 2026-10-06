"""Linux Pulse/PipeWire integration: private null sink, real AAC/TS over localhost.
Requires an accessible audio session, pactl, paplay, ffmpeg, ffprobe and TSDuck.
Does not route sound to speakers, alter default devices or contact a transmitter.
Captures/logs are retained in build/audio-check/ for independent review.
"""
import array
import argparse
import json
import math
import os
from pathlib import Path
import shutil
import socket
import statistics
import subprocess
import time

OUT = Path('build/audio-check')


def run(args, **kwargs):
    return subprocess.run(args, check=True, capture_output=True, **kwargs)


def inspect(data, bitrate, audio_rate, buffer_ms, name, fps):
    counters, pcrs, audio, video_pts = {}, [], [], []
    current, start, pts = bytearray(), 0, 0
    pending_end = None
    tables = {}
    for i in range(len(data)//188):
        p = data[i*188:(i+1)*188]
        assert p[0] == 0x47 and not p[1]&128
        pid = (p[1]&31)*256+p[2]
        mode, cc = (p[3]>>4)&3, p[3]&15
        assert pid in (0,17,18,20,256,257,258,4096,8191) and mode
        if pid != 8191:
            if pid in counters:
                assert cc == (counters[pid]+bool(mode&1))%16, (i,pid)
            counters[pid] = cc
        pos = 4
        if mode&2:
            pos += 1+p[4]
            assert pos<=188
            if p[4] and p[5]&16:
                ticks = ((p[6]<<25)|(p[7]<<17)|(p[8]<<9)|(p[9]<<1)|(p[10]>>7))*300+((p[10]&1)<<8)+p[11]
                assert abs(ticks-(i*188+11)*8*27000000/bitrate)<1.01
                pcrs.append(ticks)
        if not mode&1:
            continue
        payload = p[pos:]
        if pid == 4096 and p[1]&64:
            section = payload[1+payload[0]:]
            tables[pid] = section[:3+((section[1]&15)<<8)+section[2]]
        if pid == 256 and p[1]&64:
            b=payload[9:14]
            video_pts.append(((b[0]&14)<<29)|(b[1]<<22)|((b[2]&254)<<14)|(b[3]<<7)|(b[4]>>1))
        if pid != 258:
            continue
        if p[1]&64:
            assert not current, 'A new PES started before previous PES completed'
            current=bytearray(); start=i
        current.extend(payload)
        if len(current)>=6:
            length=6+int.from_bytes(current[4:6],'big')
            assert length>14 and len(current)<=length
            if len(current)==length:
                assert current[:4]==b'\0\0\1\xc0' and current[6:9]==b'\x80\x80\x05'
                b=current[9:14]
                assert b[0]&1 and b[2]&1 and b[4]&1
                pts=((b[0]&14)<<29)|(b[1]<<22)|((b[2]&254)<<14)|(b[3]<<7)|(b[4]>>1)
                assert current[14]==0x56 and current[15]&0xe0==0xe0
                assert ((current[15]&31)<<8|current[16])+17==len(current)
                lead=pts-(i+1)*1504*90000/bitrate
                assert 0<lead<=9000, (name,i,lead)
                audio.append((pts,start,i,len(current),lead))
                pending_end=i+1
                current=bytearray()
    required={0,17,256,258,4096}
    if not name.endswith('plain'): required|={18,20,257}
    assert required<=counters.keys()
    assert b'\x11\xe1\x02\xf0\x03\x7c\x01\x51' in tables[4096]
    assert len(audio)>80, (name,len(audio))
    assert [a[0] for a in audio]==[buffer_ms*90+1920*i for i in range(len(audio))]
    assert all(b-a==90000//fps for a,b in zip(video_pts,video_pts[1:]))
    assert max(b-a for a,b in zip(pcrs,pcrs[1:]))<=2700000
    # Clip at a complete audio PES; incomplete trailing video is not decoded here.
    path=OUT/(name+'.ts'); path.write_bytes(data[:pending_end*188])
    probe=json.loads(run(['ffprobe','-v','error','-analyzeduration','15000000','-probesize','10000000','-show_programs','-of','json',str(path)]).stdout)
    stream=next(s for s in probe['programs'][0]['streams'] if s.get('codec_type')=='audio')
    assert stream['codec_name']=='aac_latm' and stream['sample_rate']=='48000'
    channels=1 if audio_rate==48000 else 2
    assert stream['channels']==channels
    decoded=run(['ffmpeg','-v','error','-xerror','-analyzeduration','15000000','-probesize','10000000','-i',str(path),'-map','0:a:0','-f','f32le','-']).stdout
    samples=array.array('f'); samples.frombytes(decoded)
    assert len(samples)>=len(audio)*1024*channels, (len(samples),len(audio))
    assert all(math.isfinite(s) for s in samples)
    rms=math.sqrt(sum(s*s for s in samples)/len(samples))
    assert 0.01<rms<0.5, rms
    result=run(['tsp','-I','file',str(path),'-P','continuity','-P','pcrverify','--bitrate',str(bitrate),'--jitter-max','1','-O','drop'])
    assert b'0 with jitter' in result.stderr and b'error' not in result.stderr.lower(),result.stderr
    (OUT/(name+'-tsduck.txt')).write_bytes(result.stderr)
    print(f'{name}: {len(audio)} AAC frames, {channels} channels, RMS {rms:.3f}, minimum PTS lead {min(a[4] for a in audio)/90:.2f} ms',flush=True)


def capture(source, bitrate, audio_rate, buffer_ms, duration, name, playback=None, remove=None):
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as receiver:
        receiver.bind(('127.0.0.1',0)); receiver.settimeout(0.1)
        fps=4 if name=='333-s2-mono-long' else 10
        command=['build/udp-sender',str(receiver.getsockname()[1]),str(duration),str(bitrate),str(fps),'2',
                 'audio-plain' if name.endswith('plain') else 'audio',str(audio_rate),source,str(buffer_ms)]
        with (OUT/(name+'.out')).open('wb') as out, (OUT/(name+'.err')).open('wb') as err:
            proc=subprocess.Popen(command,stdout=out,stderr=err)
            packets,times=[],[]; started=time.monotonic()
            try:
                while time.monotonic()-started<duration+15:
                    try:
                        packet,_=receiver.recvfrom(65535)
                        assert len(packet)==1316
                        packets.append(packet); times.append(time.monotonic())
                        if playback is not None:
                            playback(); playback=None
                        if remove and len(packets)>40:
                            run(['pactl','unload-module',remove]); remove=None
                    except socket.timeout:
                        if proc.poll() is not None: break
                proc.wait(timeout=3)
            finally:
                if proc.poll() is None: proc.kill(); proc.wait()
            if name in ('missing-source','disconnect','capacity'):
                assert proc.returncode!=0, name
                if name!='disconnect': assert not packets, 'Invalid input sent UDP data'
                assert time.monotonic()-started<8
                print(name+': failed promptly as expected',flush=True)
                return
            assert proc.returncode==0,(name,(OUT/(name+'.err')).read_text())
        status=json.loads((OUT/(name+'.out')).read_text())
        assert status['packets']==len(packets) and status['stop_ms']<500,status
        deltas=[b-a for a,b in zip(times,times[1:])]; interval=10528/bitrate
        assert abs(statistics.mean(deltas)-interval)<interval*0.03
        assert min(deltas)>interval*0.2 and max(deltas)<interval*2+0.03
        inspect(b''.join(packets),bitrate,audio_rate,buffer_ms,name,fps)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case', choices=['333-mono','333-stereo','333-stereo-plain',
        '333-s2-mono-long','500-mono','500-stereo-buffer10'])
    selected=parser.parse_args().case
    for tool in ('pactl','paplay','ffmpeg','ffprobe','tsp'):
        assert shutil.which(tool),f'Required tool missing: {tool}'
    OUT.mkdir(exist_ok=True)
    sink='atv_audio_test_'+str(os.getpid())
    module=run(['pactl','load-module','module-null-sink','sink_name='+sink,'rate=48000','channels=2']).stdout.decode().strip()
    noise=OUT/'stimulus.f32'
    run(['ffmpeg','-v','error','-y','-f','lavfi','-i',
         r'aevalsrc=if(lt(t\,2)\,0\,if(lt(t\,4)\,0.2*sin(2*PI*997*t)\,0.25*(2*random(0)-1))):s=48000:d=60',
         '-ac','2','-f','f32le',str(noise)])
    try:
        for rate,audio,buffer,duration,name in (
                (306882,48000,1000,8,'333-mono'),(306882,96000,1000,8,'333-stereo'),
                (306882,96000,1000,8,'333-stereo-plain'),
                (213395,48000,1000,40,'333-s2-mono-long'),
                (460784,48000,1000,8,'500-mono'),(460784,96000,10000,14,'500-stereo-buffer10')):
            if selected and selected!=name: continue
            play=subprocess.Popen(['paplay','--raw','--format=float32le','--rate=48000','--channels=2','--device='+sink,str(noise)])
            try: capture(sink+'.monitor',rate,audio,buffer,duration,name)
            finally: play.terminate(); play.wait(timeout=3)
        if selected: return
        capture('atv_nonexistent_source',306882,48000,1000,1,'missing-source')
        capture(sink+'.monitor',160000,96000,1000,1,'capacity')
        # Removing only our own sink must fail capture; no silent fallback to a mic.
        capture(sink+'.monitor',306882,48000,1000,8,'disconnect',remove=module)
        module=None
    finally:
        if module: run(['pactl','unload-module',module])


if __name__=='__main__': main()
