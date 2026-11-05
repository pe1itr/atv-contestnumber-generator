"""Activity cards: portable rendering, decoded motion and real loopback on both OSes."""
import os
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import time

from check_ts import check
from check_udp import verify


def decode(path, w, h):
    result = subprocess.run(["ffmpeg", "-v", "error", "-xerror", "-i", str(path),
                             "-fps_mode", "passthrough", "-f", "rawvideo", "-pix_fmt", "gray", "-"], capture_output=True, check=True)
    size = w*h
    assert len(result.stdout) % size == 0
    return [result.stdout[i:i+size] for i in range(0, len(result.stdout), size)]


def capture(directory, windows, gop):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock, tempfile.TemporaryFile() as log:
        sock.bind(("127.0.0.1", 0)); sock.settimeout(0.2)
        command = ["wine", "build/udp-sender.exe"] if windows else ["build/udp-sender"]
        command += [str(sock.getsockname()[1]), "9", "120000", "4", str(gop), "activity"]
        proc = subprocess.Popen(command, stdout=log, stderr=log)
        packets, arrivals = [], []
        deadline = time.monotonic()+45
        try:
            while time.monotonic() < deadline:
                try:
                    packet = sock.recv(65536)
                    assert len(packet) == 1316
                    packets.append(packet); arrivals.append(time.monotonic())
                except socket.timeout:
                    if proc.poll() is not None:
                        break
            proc.wait(timeout=5)
            log.seek(0)
            assert proc.returncode == 0, log.read().decode(errors="replace")
        finally:
            if proc.poll() is None:
                proc.kill(); proc.wait()
        assert len(packets) > 50
        mean = (arrivals[-1]-arrivals[0])/(len(arrivals)-1)
        assert abs(mean-1316*8/120000) < 0.02
        path = directory/f"udp-gop{gop}.ts"
        verify(b"".join(packets), 120000, 4, gop, path, activity=True)
        print(f"Activity UDP {'Wine' if windows else 'Linux'} GOP {gop}: loop, continuity, PCR, PTS, decode and pacing OK", flush=True)


def main():
    for tool in ("ffmpeg", "ffprobe", "tsp"):
        assert shutil.which(tool), f"Required verification tool missing: {tool}"
    os.environ.setdefault("WINEPREFIX", "/tmp/atv-contest-wine")
    os.environ.setdefault("WINEDEBUG", "-all")
    Path(os.environ["WINEPREFIX"]).mkdir(parents=True, exist_ok=True)
    references = {}
    platforms = [False, True] if shutil.which("wine") else [False]
    if len(platforms) == 1:
        print("NIET GEVERIFIEERD: Windows (Wine ontbreekt)", flush=True)
    for windows in platforms:
        directory = Path("build/activity-windows" if windows else "build/activity-linux")
        directory.mkdir(exist_ok=True)
        command = ["wine", "build/test-activity.exe"] if windows else ["build/test-activity"]
        result = subprocess.run(command+[str(directory)], capture_output=True, text=True, check=True)
        print(result.stdout, end="", flush=True)
        for i, line in enumerate(result.stdout.splitlines()):
            path, bitrate, fps, gop, w, h, seconds, _ = line.split()
            bitrate, fps, gop, w, h, seconds = map(int, (bitrate, fps, gop, w, h, seconds))
            check(Path(path), bitrate, fps, gop, w, h, seconds, motion=i<8)
            frames = decode(path, w, h)
            assert len(frames) == fps*seconds
            if i<8:
                assert len(set(frames)) > 1, "Marker did not move"
                # Only the reserved lane moves; tolerate quantization changes near its edge.
                top = [f[:w*(h*75//100)] for f in frames]
                delta = max(sum(abs(a-b) for a, b in zip(top[0], f))/len(f) for f in top)
                assert delta < 3, (i, delta)
            ppm = Path(path).with_suffix(".ppm").read_bytes()
            if windows:
                assert (ppm, frames) == references[i], f"Different platform output: case {i}"
            else:
                references[i] = (ppm, frames)
        for gop in (1, 4):
            capture(directory, windows, gop)
    print("Activity: both platforms, small cards, black/EBU, short export, motion loops and static cards OK.")


if __name__ == "__main__":
    main()
