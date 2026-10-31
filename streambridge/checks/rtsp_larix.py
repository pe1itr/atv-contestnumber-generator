#!/usr/bin/env python3
"""Reproduce Larix Android SDP and validate handshake plus real A/V failover."""
import json
import socket
import subprocess
import time
from integration import EXE, OUT, config, free_port, stop, check
from rtsp_smoke import case

def control_test(idle=False):
    port=free_port(socket.SOCK_STREAM);uri=f'rtsp://127.0.0.1:{port}/live'
    name='larix-idle' if idle else 'larix-control'
    conf=config(name,{'input':1,'rtsp':uri})
    args=[str(EXE),'--headless','--config',str(conf),'--capture',str(OUT/(name+'.ts'))]
    if idle:args+=['--seconds','24']
    bridge=subprocess.Popen(args,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    result=[]
    try:
        time.sleep(.8)
        for proto,expected in [('RTP/AVP',200),('RTP/AVP/TCP',200),('RTP/SAVP/TCP',415),('RTP/OTHER',415)]:
            with socket.create_connection(('127.0.0.1',port),timeout=3) as sock:
                cseq=0;session='';buffer=b''
                def request(method,target,body='',transport=''):
                    nonlocal cseq,session,buffer
                    cseq+=1
                    headers=f'{method} {target} RTSP/1.0\r\nCSeq: {cseq}\r\nUser-Agent: Larix-fixture\r\n'
                    if session:headers+='Session: '+session+'\r\n'
                    if transport:headers+='Transport: '+transport+'\r\n'
                    if body:headers+=f'Content-Type: application/sdp\r\nContent-Length: {len(body)}\r\n'
                    sock.sendall((headers+'\r\n'+body).encode())
                    while b'\r\n\r\n' not in buffer:buffer+=sock.recv(65536)
                    header,buffer=buffer.split(b'\r\n\r\n',1)
                    lines=header.decode().splitlines();code=int(lines[0].split()[1]);size=0
                    for line in lines[1:]:
                        key,_,value=line.partition(':')
                        if key.lower()=='session':session=value.strip().split(';')[0]
                        if key.lower()=='content-length':size=int(value)
                    while len(buffer)<size:buffer+=sock.recv(65536)
                    buffer=buffer[size:];result.append((proto,method,code));return code
                assert request('OPTIONS',uri)==200
                # Codec/track parameters captured from Larix 1.5.13 on Android 16.
                sdp=f'v=0\r\nm=video 0 {proto} 96\r\na=rtpmap:96 H264/90000\r\na=fmtp:96 packetization-mode=1; sprop-parameter-sets=Z2QAH6y0AoAt01AQYBB4sXU=,aO4Ngw==; profile-level-id=64001f\r\na=control:streamid=0\r\nm=audio 0 {proto} 97\r\na=rtpmap:97 mpeg4-generic/44100/1\r\na=fmtp:97 profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1208\r\na=control:streamid=1\r\n'
                assert request('ANNOUNCE',uri,sdp)==expected,result
                if expected==200:
                    for n in range(2):assert request('SETUP',uri+f'/streamid={n}',transport=f'RTP/AVP/TCP;unicast;interleaved={n*2}-{n*2+1};mode=record')==200,result
                    assert request('RECORD',uri)==200,result
                    if idle:
                        # Keep TCP open without RTP: force the receiver's own
                        # disconnect path, outside a client/source callback.
                        sock.settimeout(13)
                        assert sock.recv(65536)==b'', 'idle publication was not closed'
                        time.sleep(.2)
                        assert bridge.poll() is None, 'bridge crashed closing idle client'
                    else:
                        assert request('TEARDOWN',uri)==200,result
            time.sleep(.1)
        (OUT/(name+'.json')).write_text(json.dumps(result,indent=2))
        # Let the bounded capture drain its final PES before decode validation.
        if idle:bridge.wait(timeout=8)
        print('OK',name,'Larix/native SDP and reconnect; secure/unknown SDP rejected',flush=True)
    finally:
        _,err=stop(bridge);(OUT/(name+'.log')).write_bytes(err)
        assert bridge.returncode==0, (bridge.returncode,err.decode(errors='replace'))
    if idle:check(OUT/(name+'.ts'),440310)

if __name__=='__main__':
    print('Captures:',OUT,flush=True)
    control_test()
    control_test(idle=True)
    case('larix-sdp',larix=True)
    # A 2s RTP startup backlog used to trip the raw source-clock guard.
    # Also cover a source at 25 fps feeding output at 30 fps.
    case('larix-buffer2000-fps30',larix=True,latency=2000,fps=30)
    case('rtsp-native-after-larix-fix')
