"""Decode real motion exports and UDP on both platforms; no inferred FPS.

Run with --windows to include the Windows binaries under Wine.
"""
import hashlib
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time

from check_ts import check, run


def decoded(path):
    result = subprocess.run([
        "ffmpeg", "-v", "error", "-xerror", "-i", str(path),
        "-map", "0:v:0", "-fps_mode", "passthrough", "-pix_fmt", "gray",
        "-f", "rawvideo", "-"], capture_output=True, check=True)
    assert not result.stderr, result.stderr
    assert len(result.stdout) % (160*120) == 0
    return [result.stdout[i:i+160*120] for i in range(0, len(result.stdout), 160*120)]


def verify_motion(frames):
    assert len(frames) >= 40
    # Check the moving object's decoded position in every frame, including
    # both sides of each wrap. A stream of repeated static pictures fails.
    for n, frame in enumerate(frames):
        phase = n % 40
        x = 8 + 6*(phase if phase < 20 else 39-phase)
        y = 82 if phase < 20 else 94
        coords = [(i % 160, 80+i//160) for i, v in enumerate(frame[80*160:]) if v > 128]
        assert coords, n
        cx = sum(p[0] for p in coords)/len(coords)
        cy = sum(p[1] for p in coords)/len(coords)
        assert abs(cx-(x+11.5)) < 2 and abs(cy-(y+11.5)) < 2, (n, cx, cy)
    hashes = [hashlib.sha256(f[80*160:]).digest() for f in frames]
    assert len(set(hashes[:40])) == 40
    assert all(a != b for a, b in zip(hashes, hashes[1:]))
    # Verify that the surprise survives compression: first a white block,
    # then two dark eyes and a mouth, then a visibly shorter right eye.
    def mean(phase, x0, y0, w, h):
        x = 8 + 6*phase
        f = frames[phase]
        return sum(f[(82+y0+dy)*160+x+x0+dx] for dy in range(h) for dx in range(w))/(w*h)
    assert mean(0, 5, 6, 4, 4) > 180
    assert mean(10, 5, 6, 4, 4) < 80
    assert mean(10, 7, 17, 10, 2) < 100
    assert mean(15, 15, 8, 4, 2) > mean(10, 15, 8, 4, 2)+80


def udp_capture(directory, windows, bitrate=30160, gop=20):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("127.0.0.1", 0)); sock.settimeout(0.3)
    command = (["wine", "build/udp-sender.exe"] if windows else ["build/udp-sender"])
    command += [str(sock.getsockname()[1]), "13", str(bitrate), "10", str(gop), "motion"]
    proc = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    packets, arrivals = [], []
    deadline = time.monotonic()+60
    try:
        while time.monotonic() < deadline:
            try:
                p = sock.recv(65536)
                assert len(p) == 1316
                packets.append(p); arrivals.append(time.monotonic())
            except socket.timeout:
                if proc.poll() is not None:
                    break
        stdout, stderr = proc.communicate(timeout=5)
        assert proc.returncode == 0, (stdout, stderr)
    finally:
        if proc.poll() is None:
            proc.kill(); proc.wait()
        sock.close()
    assert len(packets) > 30
    mean_interval = (arrivals[-1]-arrivals[0])/(len(arrivals)-1)
    assert abs(mean_interval-1316*8/bitrate) < 0.03, mean_interval
    name=("windows" if windows else "linux")+f"-udp-{bitrate}"
    path = directory/(name+".ts")
    path.write_bytes(b"".join(packets))
    # An abrupt live stop may end inside the last PES. Validate all complete
    # access units by demuxing, and require 100+ error-free decoded frames.
    result = run(["ffmpeg", "-v", "error", "-i", str(path), "-f", "framemd5", "-"], check=False)
    lines = [line for line in result.stdout.splitlines() if line and not line.startswith("#")]
    assert len(lines) >= 100, result.stderr
    clean = directory/(name+".h264")
    run(["ffmpeg", "-v", "error", "-i", str(path), "-frames:v", "100", "-c:v", "copy", str(clean)])
    verify_motion(decoded(clean))
    result = run(["tsp", "-I", "file", str(path), "-P", "continuity", "-P", "pcrverify", "--bitrate", str(bitrate), "--jitter-max", "1", "-O", "drop"])
    assert "error" not in result.stderr.lower() and "0 with jitter" in result.stderr, result.stderr
    print(f"Motion UDP {'Wine' if windows else 'Linux'} {bitrate} bit/s GOP{gop}: {len(packets)} datagrams, mean interval {mean_interval:.6f}s, 100 moving frames checked", flush=True)


def main():
    os.environ.setdefault("WINEDEBUG", "-all")
    windows = "--windows" in sys.argv
    with tempfile.TemporaryDirectory(prefix="atv-motion-") as temp:
        directory = Path(temp)
        references = {}
        for platform in ([False, True] if windows else [False]):
            prefix = ["wine", "dist/atv-contestnummer.exe"] if platform else ["dist/atv-contestnummer"]
            for seconds in (4, 12, 60):
                path = directory/f"{'windows' if platform else 'linux'}-{seconds}.ts"
                report = run(prefix+["--motion-ts", str(path), "PE1ITR", "1957", "JO21QK", str(seconds)])
                print(report.stdout.strip(), flush=True)
                check(path, 30160, 10, 20, 160, 120, seconds, motion=True)
                frames = decoded(path)
                assert len(frames) == seconds*10
                verify_motion(frames)
                digest = hashlib.sha256(b"".join(frames)).digest()
                if platform:
                    assert digest == references[seconds], "Platform pixels differ"
                else:
                    references[seconds] = digest
            protected = directory/"protected.ts"; protected.write_bytes(b"keep")
            for args in (("PE1ITR", "1957", "JO21QK"), ("INVALID", "1957", "JO21QK"), ("PE1ITR", "1957", "JO21QK", "3")):
                assert run(prefix+["--motion-ts", str(protected), *args], check=False).returncode != 0
                assert protected.read_bytes() == b"keep"
            # A short IDR interval exceeds the default transport budget;
            # failure must leave no apparently usable output behind.
            impossible = directory/f"impossible-{platform}.ts"
            assert run(prefix+["--motion-ts", str(impossible), "PE1ITR", "1957", "JO21QK", "4", "5"], check=False).returncode != 0
            assert not impossible.exists()
            udp_capture(directory, platform)
            profiles=directory/("windows-profiles" if platform else "linux-profiles")
            profiles.mkdir()
            command=["wine","build/motion-profiles.exe"] if platform else ["build/motion-profiles"]
            print(run(command+[str(profiles)]).stdout.strip(),flush=True)
            for name, bitrate in (("s125",115196),("s2-125-pilots",120665),("s2-125-no-pilots",123607)):
                path=profiles/(name+".ts")
                check(path,bitrate,10,5,160,120,12,motion=True)
                frames=decoded(path); assert len(frames)==120
                verify_motion(frames)
                digest=hashlib.sha256(b"".join(frames)).digest()
                if platform: assert digest==references[name],name
                else: references[name]=digest
                udp_capture(directory,platform,bitrate,5)
    print("Motion: export, 40 distinct phases, visible face/wink, continuous loops and rejection paths OK.")


if __name__ == "__main__":
    main()
