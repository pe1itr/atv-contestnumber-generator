"""Independent TS/AVC structure checks, plus real decoding and midstream joins.

Usage: python3 checks/check_ts.py [file.ts bitrate fps gop width height seconds]
Without a file, generate a native encoder test matrix in a temporary directory.
FFmpeg/ffprobe and TSDuck are test tools only, never application dependencies.
"""
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


def run(args, check=True):
    return subprocess.run(args, check=check, capture_output=True, text=True)


def crc(data):
    value = 0xFFFFFFFF
    for byte in data:
        value ^= byte << 24
        for _ in range(8):
            value = ((value << 1) ^ (0x04C11DB7 if value & 0x80000000 else 0)) & 0xFFFFFFFF
    return value


def check(path, bitrate, fps, gop, width, height, seconds):
    data = path.read_bytes()
    assert len(data) % 1316 == 0
    assert 0 <= len(data)*8/bitrate-(seconds+1) < 1316*8/bitrate
    counters, tables, clocks, units = {}, {}, [], []
    unit = bytearray()
    last_video_packet = 0
    for index in range(len(data)//188):
        p = data[index*188:(index+1)*188]
        assert p[0] == 0x47 and not (p[1] & 0x80) and not (p[3] & 0xC0)
        pid = ((p[1]&31)<<8) | p[2]
        assert pid in (0, 17, 256, 4096, 8191)
        afc, cc = (p[3]>>4)&3, p[3]&15
        assert afc in (1, 2, 3)
        if pid != 8191:
            if pid in counters:
                assert cc == (counters[pid]+bool(afc&1)) % 16, (index, pid)
            counters[pid] = cc
        pos = 4
        if afc&2:
            assert p[4] <= 183
            pos = 5+p[4]
            if p[4] and p[5]&16:
                base = (p[6]<<25)|(p[7]<<17)|(p[8]<<9)|(p[9]<<1)|(p[10]>>7)
                ticks = base*300 + ((p[10]&1)<<8) + p[11]
                assert ticks == (index*188+11)*8*27000000//bitrate
                clocks.append((index, ticks))
        if not afc&1:
            continue
        payload = p[pos:]
        if pid in (0, 17, 4096):
            assert p[1]&64
            section = payload[1+payload[0]:]
            size = 3+((section[1]&15)<<8)+section[2]
            section = section[:size]
            assert crc(section) == 0
            tables.setdefault(pid, []).append(index)
            if pid == 4096:
                assert section[8:12] == bytes([0xE1, 0, 0xF0, 0])
                assert section[12:17] == bytes([0x1B, 0xE1, 0, 0xF0, 0])
            if pid == 17:
                assert section.count(b"PE1ITR") == 2
        if pid == 256:
            if p[1]&64:
                if unit:
                    units.append((bytes(unit), last_video_packet))
                unit = bytearray()
            unit.extend(payload)
            last_video_packet = index
    if unit:
        units.append((bytes(unit), last_video_packet))
    assert set(tables) == {0, 17, 4096}
    assert len(clocks) > seconds*10
    for (_, a), (_, b) in zip(clocks, clocks[1:]):
        assert 0 < (b-a)/27000000 < 0.1  # MPEG-TS ceiling; low packet rates exceed DVB's 40 ms.
    for pid, positions in tables.items():
        maximum = 1.1 if pid == 17 else 0.3
        assert max((b-a)*1504/bitrate for a, b in zip(positions, positions[1:])) <= maximum
    assert len(units) == fps*seconds
    for n, (pes, end_packet) in enumerate(units):
        assert pes[:9] == bytes([0,0,1,0xE0,0,0,0x80,0x80,5])
        p = pes[9:14]
        pts = ((p[0]&14)<<29)|(p[1]<<22)|((p[2]&254)<<14)|(p[3]<<7)|(p[4]>>1)
        assert pts == 90000+n*90000//fps
        assert (end_packet+1)*1504*90000//bitrate <= pts
        nals = [part[0]&31 for part in re.split(b"\x00\x00\x00?\x01", pes[14:]) if part]
        assert nals[0] == 9
        if n % gop == 0:
            assert all(t in nals for t in (7, 8, 5)), (n, nals)
        else:
            assert 1 in nals and 5 not in nals

    if shutil.which("ffprobe"):
        probe = json.loads(run(["ffprobe", "-v", "error", "-count_frames", "-show_programs", "-of", "json", str(path)]).stdout)
        program, = probe["programs"]
        assert program["program_id"] == 1
        assert program["tags"] == {"service_name": "PE1ITR", "service_provider": "PE1ITR"}
        stream, = program["streams"]
        assert stream["codec_name"] == "h264" and stream["has_b_frames"] == 0
        assert (stream["width"], stream["height"]) == (width, height)
        assert int(stream["nb_read_frames"]) == fps*seconds
    if shutil.which("ffmpeg"):
        reference = run(["ffmpeg", "-v", "error", "-xerror", "-i", str(path), "-f", "framemd5", "-"])
        hashes = {line.rsplit(",", 1)[-1].strip() for line in reference.stdout.splitlines() if line and not line.startswith("#")}
        # New process = fresh decoder, no headers cached from the original file.
        # A finite reception window starts at an arbitrary TS packet, including
        # in a PES. Initial damaged P pictures may warn; an IDR must recover.
        with tempfile.TemporaryDirectory(prefix="atv-join-") as temp:
            for start in (0.35, 1.15, 2.65):
                packet = int(start*bitrate/1504)
                length = int((gop/fps+1.5)*bitrate/1504)
                cut = Path(temp)/"join.ts"
                cut.write_bytes(data[packet*188:(packet+length)*188])
                result = run(["ffmpeg", "-v", "error", "-i", str(cut), "-f", "framemd5", "-"], check=False)
                recovered = {line.rsplit(",", 1)[-1].strip() for line in result.stdout.splitlines() if line and not line.startswith("#")}
                assert recovered & hashes, (start, result.stderr)  # Correct pixels, not merely a concealed damaged frame.
    if shutil.which("tsp"):
        result = run(["tsp", "-I", "file", str(path), "-P", "continuity", "-P", "pcrverify", "--bitrate", str(bitrate), "--jitter-max", "1", "-O", "drop"])
        assert "error" not in result.stderr.lower(), result.stderr
        assert re.search(r"\b0 with jitter", result.stderr), result.stderr
    print(f"OK {path.name}: {bitrate} bit/s, {width}x{height}, {fps} fps, GOP {gop}, {len(units)} beelden")


if len(sys.argv) > 1:
    check(Path(sys.argv[1]), *map(int, sys.argv[2:]))
else:
    with tempfile.TemporaryDirectory(prefix="atv-ts-check-") as temp:
        for i, (bitrate, fps, gop, width, height) in enumerate([
            (120000, 5, 5, 320, 240), (60000, 5, 5, 320, 240),
            (60000, 2, 1, 320, 240), (60000, 5, 1, 160, 120),
            (120000, 5, 5, 120, 68), (240000, 10, 10, 640, 360),
            (120000, 10, 2, 240, 180), (120000, 10, 2, 240, 136),
            (115196, 10, 2, 240, 180), (123607, 10, 2, 240, 136),
        ]):
            path = Path(temp)/f"test-{i}.ts"
            run(["dist/atv-contestnummer", "--ts-test", str(path), str(bitrate), "6", str(fps), str(gop), str(width), str(height)])
            check(path, bitrate, fps, gop, width, height, 6)
        # Invalid input and existing output must fail without damaging files.
        path = Path(temp)/"protected.ts"
        path.write_bytes(b"keep me")
        for value in ("120000", "0", "999999999999999999999"):
            assert run(["dist/atv-contestnummer", "--ts-test", str(path), value], check=False).returncode != 0
            assert path.read_bytes() == b"keep me"
        impossible = Path(temp)/"too-large.ts"
        assert run(["dist/atv-contestnummer", "--ts-test", str(impossible), "48000", "2", "25", "1", "640", "480"], check=False).returncode != 0
        assert not impossible.exists()
