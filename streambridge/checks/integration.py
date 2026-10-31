#!/usr/bin/env python3
"""Native encode and real SRT/UDP loopback checks; FFmpeg/TSDuck are analyzers only."""
import json
import os
from pathlib import Path
import shutil
import signal
import socket
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / 'bin/datv-streambridge'
OUT = ROOT / 'build' / ('checks-' + str(time.time_ns()))
OUT.mkdir(parents=True)

def run(args, **kw):
    p = subprocess.run(list(map(str, args)), capture_output=True, text=True, timeout=50, **kw)
    assert p.returncode == 0, (args, p.stdout, p.stderr)
    return p

def crc(data):
    c = 0xffffffff
    for b in data:
        c ^= b << 24
        for _ in range(8):
            c = ((c << 1) ^ (0x04c11db7 if c & 0x80000000 else 0)) & 0xffffffff
    return c

def pts(p):
    assert p[0] & 1 and p[2] & 1 and p[4] & 1
    return ((p[0] & 14) << 29) | (p[1] << 22) | ((p[2] & 254) << 14) | (p[3] << 7) | (p[4] >> 1)

def check(path, rate, audio=True, callsign="NOCALL", service_type=0x16):
    data = path.read_bytes()
    assert len(data) % 1316 == 0 and len(data) > rate // 8 * 2
    counters, tables, clocks, timestamps, deadlines = {}, {}, [], {}, {}
    allowed = {0, 16, 17, 18, 20, 256, 4096, 8191} | ({257} if audio else set())
    for n in range(len(data)//188):
        p = data[n*188:(n+1)*188]
        assert p[0] == 0x47 and not p[1] & 128 and not p[3] & 192
        pid = ((p[1] & 31) << 8) | p[2]
        assert pid in allowed
        afc, cc = (p[3] >> 4) & 3, p[3] & 15
        assert afc in (1, 2, 3)
        pos, disc = 4, False
        if afc & 2:
            pos = 5+p[4]
            assert pos <= 188 and (afc != 2 or p[4] == 183)
            if p[4]:
                disc = bool(p[5] & 128)
                if p[5] & 16:
                    assert pid == 256 and p[4] >= 7 and p[10] & 126 == 126
                    base = (p[6]<<25)|(p[7]<<17)|(p[8]<<9)|(p[9]<<1)|(p[10]>>7)
                    ext = ((p[10]&1)<<8)|p[11]
                    assert ext < 300
                    t = base*300+ext
                    assert t == ((n*188+11)*8*27000000//rate) % ((1<<33)*300)
                    clocks.append((n, t))
        if pid != 8191 and pid in counters and not disc:
            assert cc == (counters[pid]+bool(afc&1)) % 16, (pid, n)
        counters[pid] = cc
        if not afc & 1:
            continue
        payload = p[pos:]
        if pid in (0, 16, 17, 18, 20, 4096):
            assert p[1] & 64
            sec = payload[1+payload[0]:]
            size = 3+((sec[1]&15)<<8)+sec[2]
            sec = sec[:size]
            assert len(sec) == size
            if pid != 20:
                assert crc(sec) == 0, (pid, sec.hex())
            tables.setdefault(pid, []).append(n)
            if pid == 4096:
                assert sec[8:12] == bytes([0xe1, 0, 0xf0, 0])
                assert sec[12:17] == bytes([0x1b, 0xe1, 0, 0xf0, 0])
                if audio:
                    assert sec[17:22] == bytes([0x0f, 0xe1, 1, 0xf0, 0])
            if pid == 17:
                assert sec[16]==0x48 and sec[18]==service_type, ('SDT service type',sec.hex())
            if pid == 16:
                assert sec[24:28]==bytes([0x41,3,0,1]) and sec[28]==service_type, ('NIT service type',sec.hex())
            if pid == 18:
                assert sec[0] == 0x4e and sec[6] in (0, 1) and sec[7] == 1
        if pid in (256, 257):
            if p[1] & 64:
                assert payload[:3] == b'\0\0\1'
                assert payload[6:9] == bytes([128,128,5])
                t = pts(payload[9:14])
                ts = timestamps.setdefault(pid, [])
                assert not ts or t > ts[-1]
                ts.append(t); deadlines[pid] = t
                if pid == 256:
                    assert payload[14:18] == b'\0\0\0\1'  # Annex B
            assert (n+1)*1504*90000//rate <= deadlines[pid], (pid,n)
    assert set(tables) == {0, 16, 17, 18, 20, 4096}
    assert len(clocks) > 20
    assert max((b-a)/27000000 for (_,a),(_,b) in zip(clocks,clocks[1:])) <= .1
    for pid, limit in ((0,.5),(4096,.5),(17,2),(16,10),(18,2),(20,30)):
        positions = tables[pid]
        assert all((b-a)*1504/rate <= limit for a,b in zip(positions,positions[1:]))
    assert 256 in timestamps and (257 in timestamps) == audio
    if shutil.which('ffprobe'):
        info = json.loads(run(['ffprobe','-v','error','-show_programs','-of','json',path]).stdout)
        program, = info['programs']
        assert program['tags']['service_name'] == callsign and program['tags']['service_provider'] == callsign
        streams = program['streams']
        assert streams[0]['codec_name']=='h264' and streams[0]['has_b_frames']==0
        if audio:
            assert streams[1]['codec_name']=='aac' and streams[1]['sample_rate']=='48000'
    else:
        raise AssertionError('ffprobe ontbreekt; decodercontrole is verplicht voor deze test')
    decoded = run(['ffmpeg','-v','error','-i',path,'-f','null','-'])
    assert not decoded.stderr, decoded.stderr
    if shutil.which('tsanalyze'):
        report = run(['tsanalyze',path]).stdout
        path.with_suffix('.analysis.txt').write_text(report)
    print(f'OK {path.name}: {rate} bit/s, PCR/PTS/CC/PSI/SI en H.264'+(' + AAC' if audio else ''))

def config(name, values):
    p = OUT / (name+'.ini')
    p.write_text('[streambridge]\n'+'\n'.join(f'{k}={v}' for k,v in values.items())+'\n')
    return p

def test_local():
    for name, values, rate, audio in [
        ('default', {}, 440310, True),
        ('audio48', {'system':0,'symbol_rate_id':3,'fec_id':1},184313,True),
        ('audio64', {'system':1,'symbol_rate_id':2,'fec_id':9},223576,True),
        ('low', {'system':0,'symbol_rate_id':2,'fec_id':0,'audio_kbps':0,'fps':5,'gop':5},115196,False),
        ('high', {'system':1,'symbol_rate_id':5,'fec_id':9},894305,True),
        ('audio512', {'system':1,'symbol_rate_id':5,'fec_id':9,'audio_kbps':512},894305,True),
    ]:
        if '--audio-budget' in sys.argv and name not in ('audio48','audio64'):continue
        path=OUT/(name+'.ts')
        p=run([EXE,'--headless','--config',config(name,values),'--test-source','--seconds','4','--capture',path])
        (OUT/(name+'.log')).write_text(p.stderr)
        check(path,rate,audio)
    if '--audio-budget' in sys.argv:return
    bad = config('overbudget', {'video_kbps':2000})
    p = subprocess.run([str(EXE),'--headless','--config',str(bad)],capture_output=True,text=True)
    assert p.returncode and 'Onvoldoende TS-capaciteit' in p.stderr
    lowaudio = config('audio32', {'audio_kbps':32})
    p = subprocess.run([str(EXE),'--headless','--config',str(lowaudio)],capture_output=True,text=True)
    assert p.returncode and '48–512' in p.stderr
    print('OK ongeldig bitratebudget en audio lager dan 48 kbit/s geweigerd')
    path=OUT/'long.ts'
    p=run([EXE,'--headless','--config',config('long',{}),'--test-source','--seconds','25','--capture',path])
    (OUT/'long.log').write_text(p.stderr);check(path,440310)

def free_port(kind):
    with socket.socket(socket.AF_INET,kind) as s:
        s.bind(('127.0.0.1',0));return s.getsockname()[1]

def srt_sender(port, mode):
    env=os.environ.copy();env['GST_PLUGIN_PATH']=str(ROOT/'bin/plugins')+os.pathsep+env.get('GST_PLUGIN_PATH','')
    args=['gst-launch-1.0','-q','mpegtsmux','name=mux','alignment=7','!','srtsink',f'uri=srt://127.0.0.1:{port}?mode={mode}',
          'videotestsrc','is-live=true','pattern=snow','!','video/x-raw,format=I420,width=640,height=480,framerate=25/1','!',
          'x264enc','tune=zerolatency','bitrate=1800','key-int-max=25','!','h264parse','!','queue','!','mux.',
          'audiotestsrc','is-live=true','!','audioconvert','!','audio/x-raw,rate=48000,channels=2','!',
          'fdkaacenc','bitrate=64000','!','aacparse','!','queue','!','mux.']
    return subprocess.Popen(args,env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE)

def stop(p):
    if p.poll() is None:
        p.terminate()
        try:return p.communicate(timeout=5)
        except subprocess.TimeoutExpired:p.kill();return p.communicate()
    return p.communicate()

def test_srt(mode):
    srt_port=free_port(socket.SOCK_DGRAM)
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as receiver:
        receiver.bind(('127.0.0.1',0));receiver.settimeout(.2)
        conf=config('srt-'+mode,{'srt':f'srt://127.0.0.1:{srt_port}?mode={mode}', 'port':receiver.getsockname()[1]})
        sender=None;bridge=None;packets=[];times=[]
        try:
            if mode=='caller':sender=srt_sender(srt_port,'listener');time.sleep(.5)
            bridge=subprocess.Popen([str(EXE),'--headless','--config',str(conf),'--seconds','5'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,env={**os.environ,'GST_DEBUG':'2,decodebin:5' if '--debug' in sys.argv else '0','GST_DEBUG_FILE':str(OUT/'gst-debug.log')})
            if mode=='listener':time.sleep(.5);sender=srt_sender(srt_port,'caller')
            deadline=time.monotonic()+35
            while time.monotonic()<deadline:
                try:
                    data,_=receiver.recvfrom(65536);assert len(data)==1316
                    packets.append(data);times.append(time.monotonic())
                except socket.timeout:
                    if bridge.poll() is not None:break
            stdout,stderr=bridge.communicate(timeout=5)
            (OUT/f'srt-{mode}.log').write_bytes(stderr)
            assert bridge.returncode==0,stderr.decode()
            assert len(times)>100
            rate=(len(times)-1)*1316*8/(times[-1]-times[0])
            assert abs(rate/440310-1)<.025,rate
            gaps=[b-a for a,b in zip(times,times[1:])]
            (OUT/f'srt-{mode}-timing.json').write_text(json.dumps({'bitrate':rate,'datagrams':len(times),'min_gap_ms':min(gaps)*1000,'max_gap_ms':max(gaps)*1000},indent=2))
            path=OUT/f'srt-{mode}.ts';path.write_bytes(b''.join(packets));check(path,440310)
            print(f'OK SRT {mode} → hercodering → UDP: {rate:.0f} bit/s, max gap {max(gaps)*1000:.2f} ms')
        finally:
            if bridge:
                _,err=stop(bridge);(OUT/f"srt-{mode}.log").write_bytes(err or b"")
            if sender:
                _,err=stop(sender);(OUT/f"sender-{mode}.log").write_bytes(err or b"")

def test_stop_waiting():
    conf=config('waiting',{'srt':f'srt://127.0.0.1:{free_port(socket.SOCK_DGRAM)}?mode=listener'})
    p=subprocess.Popen([str(EXE),'--headless','--config',str(conf),'--capture',str(OUT/'waiting.ts')],stderr=subprocess.PIPE)
    time.sleep(.5);start=time.monotonic();p.send_signal(signal.SIGINT)
    _,err=p.communicate(timeout=3)
    assert p.returncode==0,err
    assert time.monotonic()-start<2
    print('OK stoppen tijdens wachten op SRT')

def test_fallback(mode="listener"):
    import array
    import math
    port=free_port(socket.SOCK_DGRAM)
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as rx:
        rx.bind(('127.0.0.1',0));rx.settimeout(.04)
        conf=config('fallback-'+mode,{'srt':f'srt://127.0.0.1:{port}?mode={mode}','port':rx.getsockname()[1],'callsign':'PE1TEST'})
        source_mode='caller' if mode=='listener' else 'listener'
        sender=srt_sender(port,source_mode) if mode=='caller' else None
        if sender:time.sleep(.5)
        bridge=subprocess.Popen([str(EXE),'--headless','--config',str(conf),'--seconds','17'],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        retired=None;stage=1 if sender else 0;packets=[];times=[];started=time.monotonic()
        try:
            while time.monotonic()-started<25:
                elapsed=time.monotonic()-started
                if stage==0 and elapsed>=1:
                    sender=srt_sender(port,source_mode);stage=1
                elif stage==1 and elapsed>=5:
                    sender.terminate();retired=sender;sender=None;stage=2
                elif stage==2 and elapsed>=10:
                    sender=srt_sender(port,source_mode);stage=3
                try:
                    data,_=rx.recvfrom(65536);assert len(data)==1316
                    packets.append(data);times.append(time.monotonic())
                except socket.timeout:
                    if bridge.poll() is not None:break
            _,err=bridge.communicate(timeout=5)
            (OUT/f'fallback-{mode}.log').write_bytes(err)
            assert bridge.returncode==0,err.decode()
            assert b'No Signal' in err and b'SRT-beeld actief' in err
            path=OUT/f'fallback-{mode}.ts';path.write_bytes(b''.join(packets));check(path,440310,callsign='PE1TEST')
            # A source failure must not reset CC/PCR or mark a new output discontinuity.
            seen=set()
            data=path.read_bytes()
            for i in range(0,len(data),188):
                p=data[i:i+188];pid=((p[1]&31)<<8)|p[2]
                disc=bool(p[3]&32 and p[4] and p[5]&128)
                if disc:assert pid not in seen,('output discontinuity',pid,i)
                seen.add(pid)
            gaps=[b-a for a,b in zip(times,times[1:])]
            assert max(gaps)<.1,max(gaps)
            rate=(len(times)-1)*1316*8/(times[-1]-times[0]);assert abs(rate/440310-1)<.025
            frames=subprocess.run(['ffmpeg','-v','error','-i',str(path),'-vf','fps=1','-pix_fmt','rgb24','-f','rawvideo','-'],capture_output=True,check=True).stdout
            size=320*240*3;blue=[]
            for i in range(len(frames)//size):
                frame=frames[i*size:(i+1)*size]
                blue.append(sum(frame[n]<60 and frame[n+1]<60 and frame[n+2]>170 for n in range(0,size,3))/(size//3))
            assert blue[0]>.7 and max(blue[7:10])>.7,blue
            assert min(blue[3:5])<.2 and min(blue[13:16])<.2,blue
            pcm=subprocess.run(['ffmpeg','-v','error','-i',str(path),'-vn','-ar','48000','-ac','2','-f','s16le','-'],capture_output=True,check=True).stdout
            samples=array.array('h');samples.frombytes(pcm)
            def rms(second):
                part=samples[second*96000:(second+1)*96000]
                return math.sqrt(sum(v*v for v in part)/len(part))
            assert rms(8)<10 and rms(13)>1000,(rms(8),rms(13))
            run(['ffmpeg','-v','error','-ss','8','-i',path,'-frames:v','1',OUT/f'no-signal-{mode}.png'])
            (OUT/f'fallback-{mode}-timing.json').write_text(json.dumps({'bitrate':rate,'max_gap_ms':max(gaps)*1000,'blue_fractions':blue,'silent_rms':rms(8),'live_rms':rms(13)},indent=2))
            print(f'OK SRT {mode} loss/reconnect: continuous TS, blue No Signal/callsign, silent AAC, automatic live recovery')
        finally:
            stop(bridge)
            if sender:stop(sender)
            if retired:stop(retired)

if __name__=='__main__':
    run([EXE,'--probe'])
    if '--network' not in sys.argv and '--fallback-only' not in sys.argv:test_local()
    if '--audio-budget' not in sys.argv:
        if '--fallback-only' not in sys.argv:
            if '--caller' not in sys.argv:test_srt('listener')
            test_srt('caller');test_stop_waiting()
        test_fallback('listener');test_fallback('caller')
    print('Captures en rapporten:',OUT)
