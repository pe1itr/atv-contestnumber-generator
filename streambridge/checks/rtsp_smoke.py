#!/usr/bin/env python3
"""RTSP RECORD/TCP integration: real publisher -> bridge -> UDP, loss and recovery."""
import array
import json
import math
import os
import signal
import socket
import subprocess
import time
from integration import EXE, ROOT, OUT, free_port, stop, check, config, run


def publisher(port, name, audio=True, generic=True, reversed_tracks=False, larix=False):
    env=os.environ.copy()
    env['GST_PLUGIN_PATH']=str(ROOT/'bin/plugins')+':'+str(ROOT/'build/plugins')
    env['LD_LIBRARY_PATH']=str(ROOT/'bin/lib')+':'+env.get('LD_LIBRARY_PATH','')
    env['GST_PLUGIN_FEATURE_RANK']='rtpmp4gpay:3000,rtpmp4apay:0' if generic else 'rtpmp4apay:3000,rtpmp4gpay:0'
    video=['videotestsrc','is-live=true','pattern=snow','!','video/x-raw,format=I420,width=640,height=480,framerate=25/1','!',
           'x264enc','tune=zerolatency','bitrate=1800','key-int-max=25','!','h264parse','!','queue','!',f'sink.sink_{1 if reversed_tracks and audio else 0}']
    sound=['audiotestsrc','is-live=true','!','audio/x-raw,rate=48000,channels=2','!','fdkaacenc','bitrate=64000','!','aacparse','!','queue','!',f'sink.sink_{0 if reversed_tracks else 1}'] if audio else []
    args=['gst-launch-1.0','-e','rtspclientsink','name=sink',f'location=rtsp://127.0.0.1:{port}/live','protocols=tcp']
    args+=(sound+video if reversed_tracks else video+sound)
    if larix:args=[str(ROOT/'build/larix-publisher'),f'rtsp://127.0.0.1:{port}/live']
    log=(OUT/(name+'-publisher.log')).open('wb')
    p=subprocess.Popen(args,env=env,stdout=log,stderr=subprocess.STDOUT);log.close()
    return p


def case(name, send_audio=True, output_audio=True, generic=True, graceful=False, reversed_tracks=False, competitor=False, larix=False, latency=200, fps=25):
    port=free_port(socket.SOCK_STREAM)
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as rx:
        rx.bind(('127.0.0.1',0));rx.settimeout(.04)
        conf=config(name,{'input':1,'rtsp':f'rtsp://127.0.0.1:{port}/live','port':rx.getsockname()[1],
                          'callsign':'PE1TEST','audio_kbps':-1 if output_audio else 0,'latency':latency,'fps':fps})
        bridge=subprocess.Popen([str(EXE),'--headless','--config',str(conf),'--seconds','18'],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        children=[];sender=None;other=None;stage=0;packets=[];times=[];started=time.monotonic()
        try:
            while time.monotonic()-started<28:
                elapsed=time.monotonic()-started
                if stage==0 and elapsed>=1:
                    sender=publisher(port,name,send_audio,generic,reversed_tracks,larix);children.append(sender);stage=1
                if competitor and other is None and elapsed>=3:
                    other=publisher(port,name+'-second',send_audio,generic);children.append(other)
                if stage==1 and elapsed>=6:
                    assert sender.poll() is None,(name,'publisher exited early')
                    sender.send_signal(signal.SIGINT) if graceful else sender.kill();stage=2
                if stage==2 and elapsed>=11:
                    assert sender.poll() is not None,(name,'publisher did not close')
                    sender=publisher(port,name+'-restart',send_audio,generic,reversed_tracks,larix);children.append(sender);stage=3
                try:
                    data,_=rx.recvfrom(65536);assert len(data)==1316
                    packets.append(data);times.append(time.monotonic())
                except socket.timeout:
                    if bridge.poll() is not None:break
            _,err=bridge.communicate(timeout=5);(OUT/(name+'.log')).write_bytes(err)
            assert bridge.returncode==0,err.decode()
            assert err.count(b'RTSP-beeld actief')>=2,err.decode()
            if competitor:
                assert other.poll() is not None,'second publisher was not rejected'
            path=OUT/(name+'.ts');path.write_bytes(b''.join(packets));check(path,440310,output_audio,'PE1TEST')
            probe=json.loads(run(['ffprobe','-v','error','-select_streams','v:0',
                                  '-show_entries','stream=r_frame_rate:packet=pts','-of','json',path]).stdout)
            assert probe['streams'][0]['r_frame_rate']==f'{fps}/1',probe['streams']
            pts=[int(p['pts']) for p in probe['packets']]
            # Nanoseconds -> 90 kHz may round a frame timestamp by one tick;
            # bound the whole timeline, not merely each adjacent difference.
            assert all(abs((p-pts[0])*fps-i*90000)<=fps for i,p in enumerate(pts)), 'output video PTS cadence changed'
            seen=set();data=path.read_bytes()
            for i in range(0,len(data),188):
                p=data[i:i+188];pid=((p[1]&31)<<8)|p[2]
                if p[3]&32 and p[4] and p[5]&128:assert pid not in seen,('new discontinuity',pid,i)
                seen.add(pid)
            gaps=[b-a for a,b in zip(times,times[1:])];assert max(gaps)<.1,max(gaps)
            rate=(len(times)-1)*1316*8/(times[-1]-times[0]);assert abs(rate/440310-1)<.025,rate
            frames=subprocess.run(['ffmpeg','-v','error','-i',str(path),'-vf','fps=1','-pix_fmt','rgb24','-f','rawvideo','-'],capture_output=True,check=True).stdout
            size=320*240*3;blue=[]
            for i in range(len(frames)//size):
                frame=frames[i*size:(i+1)*size]
                blue.append(sum(frame[n]<60 and frame[n+1]<60 and frame[n+2]>170 for n in range(0,size,3))/(size//3))
            assert blue[0]>.7 and min(blue[8:10])>.7,blue
            live_start=4 if latency==2000 else 3
            assert max(blue[live_start:6])<.2 and max(blue[14:17])<.2,blue
            report={'bitrate':rate,'max_gap_ms':max(gaps)*1000,'blue_fractions':blue}
            if output_audio:
                pcm=subprocess.run(['ffmpeg','-v','error','-i',str(path),'-vn','-ar','48000','-ac','2','-f','s16le','-'],capture_output=True,check=True).stdout
                samples=array.array('h');samples.frombytes(pcm)
                def rms(second):
                    part=samples[second*96000:(second+1)*96000];return math.sqrt(sum(v*v for v in part)/len(part))
                assert rms(9)<10,rms(9)
                if send_audio:assert rms(4)>1000 and rms(15)>1000,(rms(4),rms(15))
                else:assert rms(4)<10 and rms(15)<10
                report.update(silent_rms=rms(9),live_rms=rms(15))
            run(['ffmpeg','-v','error','-ss','9','-i',path,'-frames:v','1',OUT/(name+'.png')])
            (OUT/(name+'-timing.json')).write_text(json.dumps(report,indent=2))
            print('OK',name,'RECORD/TCP, continuous UDP/PCR/CC, No Signal/silence, reconnect',flush=True)
        finally:
            stop(bridge)
            for child in children:stop(child)


def waiting_and_port_reuse():
    port=free_port(socket.SOCK_STREAM);conf=config('rtsp-wait',{'input':1,'rtsp':f'rtsp://127.0.0.1:{port}/live'})
    for n in range(2):
        p=subprocess.Popen([str(EXE),'--headless','--config',str(conf),'--capture',str(OUT/f'waiting-{n}.ts')],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        time.sleep(.7)
        # Bad/unsupported SDP must be rejected without breaking the listener.
        bad_sdps=['this is not SDP', 'v=0\r\no=- 1 1 IN IP4 127.0.0.1\r\ns=test\r\nt=0 0\r\nm=video 0 RTP/AVP 99999999999999999999999999\r\na=rtpmap:99999999999999999999999999 H264/90000\r\n']
        for body in bad_sdps:
            with socket.create_connection(('127.0.0.1',port),timeout=2) as client:
                request=f'ANNOUNCE rtsp://127.0.0.1:{port}/live RTSP/1.0\r\nCSeq: 1\r\nContent-Type: application/sdp\r\nContent-Length: {len(body)}\r\n\r\n{body}'
                client.sendall(request.encode());response=client.recv(4096)
                assert response.startswith(b'RTSP/1.0 4'),response
        with socket.create_connection(('127.0.0.1',port),timeout=2) as client:
            client.sendall(f'OPTIONS rtsp://127.0.0.1:{port}/live RTSP/1.0\r\nCSeq: 2\r\n\r\n'.encode())
            assert client.recv(4096).startswith(b'RTSP/1.0 200')
        started=time.monotonic();p.terminate();_,err=p.communicate(timeout=3)
        assert p.returncode==0 and time.monotonic()-started<2,err.decode()
        assert b'Address already in use' not in err
    print('OK RTSP stoppen tijdens wachten en hergebruik luisterpoort',flush=True)

if __name__=='__main__':
    print('Captures:',OUT,flush=True)
    case('rtsp-generic-abrupt')
    case('rtsp-latm-teardown',generic=False,graceful=True,reversed_tracks=True,competitor=True)
    case('rtsp-video-only',send_audio=False,output_audio=False)
    case('rtsp-audio-muted',output_audio=False)
    case('rtsp-audio-missing',send_audio=False)
    waiting_and_port_reuse()
