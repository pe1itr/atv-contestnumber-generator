"""Capture real loopback datagrams and independently verify pacing/TS/decoding.
Use --windows to run the same sender via Wine. No RF destination is contacted.
"""
import json
import os
import re
from pathlib import Path
import shutil
import socket
import statistics
import subprocess
import sys
import tempfile
import time


def verify(data, bitrate, fps, gop, path):
    hashes = None
    cc, pcrs, pts, starts = {}, [], [], []
    for i in range(len(data)//188):
        p = data[i*188:(i+1)*188]
        assert p[0] == 0x47 and not p[1]&128
        pid, mode, counter = ((p[1]&31)<<8)|p[2], (p[3]>>4)&3, p[3]&15
        assert pid in (0, 17, 256, 4096, 8191) and mode in (1, 2, 3)
        if pid != 8191:
            if pid in cc:
                assert counter == (cc[pid]+bool(mode&1)) % 16, (i, pid)
            cc[pid] = counter
        pos = 4
        if mode&2:
            pos += 1+p[4]
            if p[4] and p[5]&16:
                base = (p[6]<<25)|(p[7]<<17)|(p[8]<<9)|(p[9]<<1)|(p[10]>>7)
                ticks = base*300 + ((p[10]&1)<<8)+p[11]
                pcrs.append((i,ticks))
        if pid==256 and mode&1 and p[1]&64:
            pes = p[pos:]
            assert pes[:9] == bytes([0,0,1,0xE0,0,0,0x80,0x80,5])
            b = pes[9:14]
            pts.append(((b[0]&14)<<29)|(b[1]<<22)|((b[2]&254)<<14)|(b[3]<<7)|(b[4]>>1))
            starts.append(i)
    assert set(cc) == {0,17,256,4096}
    assert pts == [90000+i*90000//fps for i in range(len(pts))]
    assert len(pts)>fps*3
    for (ia,a),(ib,b) in zip(pcrs,pcrs[1:]):
        assert b>a and abs((b-a)-(ib-ia)*1504*27000000/bitrate)<1.1
        assert (b-a)/27000000<0.1
    # Stop may cut the final PES. Remove that incomplete access unit for decoding.
    path.write_bytes(data[:starts[-1]*188])
    if shutil.which("ffprobe"):
        result = subprocess.run(["ffprobe","-v","error","-count_frames","-show_programs","-of","json",str(path)],capture_output=True,text=True,check=True)
        program, = json.loads(result.stdout)["programs"]
        assert program["program_id"]==1 and program["tags"]["service_name"]=="PE1ITR"
        video, = program["streams"]
        assert video["codec_name"]=="h264" and video["has_b_frames"]==0
        assert int(video["nb_read_frames"])==len(pts)-1
    if shutil.which("ffmpeg"):
        result = subprocess.run(["ffmpeg","-v","error","-xerror","-i",str(path),"-f","framemd5","-"],capture_output=True,text=True)
        assert result.returncode==0, result.stderr
        hashes = [line.rsplit(",",1)[-1] for line in result.stdout.splitlines() if line and not line.startswith("#")]
        assert len(hashes)==len(pts)-1
        period=max(2,gop)
        assert all(h==hashes[n%period] for n,h in enumerate(hashes))
        if gop==1:
            headers = subprocess.run(["ffmpeg","-v","info","-i",str(path),"-c:v","copy","-bsf:v","trace_headers","-f","null","-"],capture_output=True,text=True,check=True)
            ids = re.findall(r"idr_pic_id\s+\S+\s+= (\d+)",headers.stderr)
            assert len(ids)==len(pts)-1 and all(a!=b for a,b in zip(ids,ids[1:])),ids
    if shutil.which("tsp"):
        result = subprocess.run(["tsp","-I","file",str(path),"-P","continuity","-P","pcrverify","--bitrate",str(bitrate),"--jitter-max","1","-O","drop"],capture_output=True,text=True,check=True)
        assert "0 with jitter" in result.stderr and "error" not in result.stderr.lower(),result.stderr
    return set(hashes) if hashes is not None else None


def capture(windows, duration, bitrate, fps, gop):
    receiver = socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
    receiver.bind(("127.0.0.1",0)); receiver.settimeout(0.1)
    command = ["wine","build/udp-sender.exe"] if windows else ["build/udp-sender"]
    command += [str(receiver.getsockname()[1]),str(duration),str(bitrate),str(fps),str(gop)]
    env = dict(os.environ, WINEPREFIX="/tmp/atv-contest-wine", WINEDEBUG="-all")
    # Wine server processes can retain inherited pipe handles after the sender
    # exits. Files let us inspect the completed sender without waiting for EOF.
    output_file, error_file = tempfile.TemporaryFile(), tempfile.TemporaryFile()
    process = subprocess.Popen(command,stdout=output_file,stderr=error_file,env=env)
    packets, times = [], []
    deadline = time.monotonic()+duration+35
    try:
        while time.monotonic()<deadline:
            try:
                packet,_ = receiver.recvfrom(65535)
                assert len(packet)==1316
                packets.append(packet); times.append(time.perf_counter())
            except socket.timeout:
                if process.poll() is not None:
                    break
        else:
            raise AssertionError("UDP sender timed out")
        process.wait(timeout=2)
        output_file.seek(0); error_file.seek(0)
        stdout,stderr = output_file.read().decode(),error_file.read().decode()
        assert process.returncode==0,(stdout,stderr)
        status = json.loads(stdout.strip())
        assert status["packets"]==len(packets),status
        assert status["refusals"]==0,status
        assert status["stop_ms"]<500,status
        # No delayed queued stream remains after Stop/destruction.
        receiver.settimeout(0.25)
        try:
            receiver.recvfrom(65535)
            raise AssertionError("Packet received after sender shutdown")
        except socket.timeout:
            pass
        deltas = [b-a for a,b in zip(times,times[1:])]
        interval = 1316*8/bitrate
        assert abs(statistics.mean(deltas)-interval)<interval*0.03
        assert min(deltas)>interval*0.2 and max(deltas)<interval*2+0.03,(min(deltas),max(deltas))
        with tempfile.TemporaryDirectory(prefix="atv-udp-check-") as temp:
            hashes = verify(b"".join(packets),bitrate,fps,gop,Path(temp)/"capture.ts")
        print(f"{'Windows/Wine' if windows else 'Linux'} OK: {bitrate} bit/s, {fps} fps, GOP {gop}, {len(packets)} datagrams; interval mean/min/max {statistics.mean(deltas)*1000:.2f}/{min(deltas)*1000:.2f}/{max(deltas)*1000:.2f} ms; stop {status['stop_ms']:.1f} ms",flush=True)
        return hashes
    finally:
        if process.poll() is None:
            process.kill(); process.wait()
        receiver.close()
        output_file.close(); error_file.close()


def recovery(windows, reference_hashes):
    """Start RX late, close its port during TX, then recover without restarting TX."""
    receiver = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    receiver.bind(("127.0.0.1", 0))
    port = receiver.getsockname()[1]
    receiver.close()
    command = ["wine", "build/udp-sender.exe"] if windows else ["build/udp-sender"]
    command += [str(port), "9", "120000", "10", "2"]
    env = dict(os.environ, WINEPREFIX="/tmp/atv-contest-wine", WINEDEBUG="-all")
    with tempfile.TemporaryFile() as output, tempfile.TemporaryFile() as errors:
        process = subprocess.Popen(command, stdout=output, stderr=errors, env=env)
        try:
            time.sleep(1)
            receiver = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            receiver.bind(("127.0.0.1", port))
            receiver.settimeout(25)
            first, _ = receiver.recvfrom(65535)
            assert len(first) == 1316
            receiver.close()
            time.sleep(1)
            assert process.poll() is None, "TX stopped when RX closed its UDP port"
            receiver = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            receiver.bind(("127.0.0.1", port))
            receiver.settimeout(0.1)
            packets, times = [], []
            deadline = time.monotonic()+30
            while time.monotonic() < deadline:
                try:
                    packet, _ = receiver.recvfrom(65535)
                    assert len(packet) == 1316
                    assert all(packet[i] == 0x47 for i in range(0, 1316, 188))
                    packets.append(packet)
                    times.append(time.perf_counter())
                except socket.timeout:
                    if process.poll() is not None:
                        break
            else:
                raise AssertionError("Recovery sender timed out")
            process.wait(timeout=2)
            output.seek(0); errors.seek(0)
            stdout, stderr = output.read().decode(), errors.read().decode()
            assert process.returncode == 0, (stdout, stderr)
            status = json.loads(stdout)
            if windows and status["refusals"] == 0:
                print("Windows/Wine ICMP error delivery: NOT VERIFIED (no refusal reported); "
                      "requires the separate test-udp-errors.exe injection check.", flush=True)
            else:
                assert status["refusals"] > 0, status
            assert status["packets"] > len(packets), status
            assert status["stop_ms"] < 500, status
            assert len(packets) > 40
            deltas = [b-a for a,b in zip(times,times[1:])]
            interval = 1316*8/120000
            assert abs(statistics.mean(deltas)-interval) < interval*0.03
            assert min(deltas) > interval*0.2, "Backlog burst after recovery"
            # Joining a live stream may initially cut a PES/non-IDR picture.
            # Check actual decoder recovery, without claiming no initial loss.
            if shutil.which("ffmpeg"):
                with tempfile.TemporaryDirectory(prefix="atv-udp-recovery-") as temp:
                    path = Path(temp)/"recovery.ts"
                    data = b"".join(packets)
                    # Stop may cut the final PES, just like the healthy capture.
                    starts = [i for i in range(0, len(data), 188)
                              if data[i+1]&64 and ((data[i+1]&31)<<8 | data[i+2]) == 256]
                    assert starts
                    path.write_bytes(data[:starts[-1]])
                    decoded = subprocess.run(["ffmpeg", "-v", "error", "-i", str(path),
                        "-f", "framemd5", "-"], capture_output=True, text=True)
                    assert decoded.returncode == 0, decoded.stderr
                    hashes = [line.rsplit(",", 1)[-1] for line in decoded.stdout.splitlines()
                              if line and not line.startswith("#")]
                    assert len(hashes) > 20, (len(hashes), decoded.stderr)
                    assert reference_hashes and set(hashes[-20:]) <= reference_hashes
                    assert all(a == b for a,b in zip(hashes[-20:-2], hashes[-18:]))
            print(f"{'Windows/Wine' if windows else 'Linux'} RX late/restart OK: "
                  f"{status['refusals']} refusals, {len(packets)} recovered datagrams, "
                  f"stop {status['stop_ms']:.1f} ms", flush=True)
        finally:
            if process.poll() is None:
                process.kill(); process.wait()
            receiver.close()


if __name__=="__main__":
    windows="--windows" in sys.argv
    if windows:
        subprocess.run(["wine", "build/test-udp-errors.exe"], check=True, timeout=30,
                       env=dict(os.environ, WINEPREFIX="/tmp/atv-contest-wine", WINEDEBUG="-all"))
    reference_hashes = capture(windows,13,120000,10,2)
    capture(windows,5,60000,2,1)
    capture(windows,5,240000,10,1)
    recovery(windows, reference_hashes)
