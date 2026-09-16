"""Independent PSI reassembly, EIT/UTC/UTF-8 and timing checks on emitted TS.

Usage: check_eit.py build/eit-linux (or build/eit-windows).
"""
from collections import defaultdict
from datetime import datetime, timedelta, timezone
from pathlib import Path
import json
import os
import shutil
import socket
import subprocess
import sys
import time

from check_ts import crc


def text(data):
    if not data:
        return ""
    assert data[0] == 0x15
    value = data[1:].decode("utf-8")
    assert all(32 <= ord(c) <= 0xFFFF and not 127 <= ord(c) <= 159 for c in value)
    return value


def utc(data):
    assert len(data) == 5
    def bcd(v):
        assert v >> 4 < 10 and v & 15 < 10
        return (v >> 4) * 10 + (v & 15)
    h, m, s = map(bcd, data[2:])
    assert h < 24 and m < 60 and s < 60
    return datetime(1858, 11, 17, tzinfo=timezone.utc) + timedelta(
        days=int.from_bytes(data[:2]), hours=h, minutes=m, seconds=s)


def check(path, rate, maximum=False, live=False):
    data = path.read_bytes()
    assert len(data) % 1316 == 0
    buffers, starts, counters = {}, {}, {}
    sections = []
    pcrs = []
    video_starts = []
    for index in range(len(data) // 188):
        p = data[index * 188:(index + 1) * 188]
        assert p[0] == 0x47 and not p[1] & 0x80 and not p[3] & 0xC0
        pid = ((p[1] & 31) << 8) | p[2]
        assert pid in (0, 0x11, 0x12, 0x14, 0x100, 0x1000, 0x1FFF)
        if pid == 0x100 and p[1] & 64:
            video_starts.append(index * 188)
        afc, cc = p[3] >> 4 & 3, p[3] & 15
        assert afc in (1, 2, 3)
        if pid != 0x1FFF:
            if pid in counters:
                assert cc == (counters[pid] + bool(afc & 1)) % 16
            counters[pid] = cc
        pos = 4
        if afc & 2:
            assert p[4] <= 183
            pos = 5 + p[4]
            if p[4] and p[5] & 16:
                base = p[6] << 25 | p[7] << 17 | p[8] << 9 | p[9] << 1 | p[10] >> 7
                ticks = base * 300 + ((p[10] & 1) << 8 | p[11])
                assert ticks == (index * 188 + 11) * 8 * 27000000 // rate
                pcrs.append(ticks)
        if not afc & 1 or pid not in (0, 0x11, 0x12, 0x14, 0x1000):
            continue
        if p[1] & 64:
            assert not buffers.get(pid), (pid, "unfinished section")
            assert p[pos] == 0
            pos += 1
            starts[pid] = (index * 188 + pos) * 8 / rate
            buffers[pid] = bytearray()
        assert pid in buffers
        buf = buffers[pid]
        buf.extend(p[pos:])
        size = 3 + ((buf[1] & 15) << 8) + buf[2]
        if len(buf) >= size:
            # Packet stuffing is not section data.
            assert all(v == 0xFF for v in buf[size:])
            end = (index * 188 + pos + size - (len(buf) - (188 - pos))) * 8 / rate
            section = bytes(buf[:size])
            assert pid == 0x14 or crc(section) == 0
            sections.append((pid, section, starts[pid], end))
            buffers[pid] = bytearray()
    if not live:  # Stop can interrupt the last section, never a TS packet.
        assert all(not b for b in buffers.values())
    assert max(b - a for a, b in zip(pcrs, pcrs[1:])) <= 2700000
    repetitions = defaultdict(list)
    previous_eit_end = None
    days, versions, descriptions = set(), set(), set()
    day_versions = {}
    clock_origin = None
    for pid, s, start, end in sections:
        key = (pid, s[6] if pid == 0x12 else 0)
        repetitions[key].append(end)
        if pid == 0x11:
            assert s[13] & 3 == 1  # actual p/f available; no schedule
        if pid == 0x14:
            assert s[:3] == bytes([0x70, 0x70, 5]) and len(s) == 8
            value = utc(s[3:])
            origin = value.timestamp() - start
            if clock_origin is None:
                clock_origin = origin
            assert abs(origin - clock_origin) < 1.1
        if pid != 0x12:
            continue
        if previous_eit_end is not None:
            assert start - previous_eit_end >= 0.025
        previous_eit_end = end
        assert s[0] == 0x4E and s[1] & 0xF0 == 0xF0
        assert s[3:5] == b"\0\1" and s[5] & 0xC1 == 0xC1
        assert s[6] in (0, 1) and s[7:14] == bytes([1, 0, 1, 0, 1, 1, 0x4E])
        if s[6] == 1:
            assert len(s) == 18
            continue
        event_start = utc(s[16:21])
        assert event_start.hour == event_start.minute == event_start.second == 0
        day = int(event_start.timestamp()) // 86400
        assert int.from_bytes(s[14:16]) == day & 65535
        version = s[5] >> 1 & 31
        if day in day_versions:
            assert day_versions[day] == version
        elif day_versions:
            assert version == (list(day_versions.values())[-1] + 1) % 32
        day_versions[day] = version
        days.add(day); versions.add(version)
        assert s[21:24] == bytes([0x24, 0, 0]) and s[24] & 0xF0 == 0x80
        length = (s[24] & 15) << 8 | s[25]
        assert len(s) == 26 + length + 4
        descriptors = s[26:-4]
        extended, last_number, short_seen = {}, None, False
        while descriptors:
            tag, n = descriptors[:2]
            d = descriptors[2:2+n]
            assert len(d) == n
            descriptors = descriptors[2+n:]
            if tag == 0x4D:
                assert not short_seen and d[:3] == b"nld"
                short_seen = True
                k = d[3]
                title = text(d[4:4+k]); city = text(d[5+k:])
                assert d[4+k] == len(d[5+k:])
                assert title == ("PE1ITR/P - JO21QK86DV12" if maximum else "PE1ITR - JO21QK")
                assert city == ("漢" * 40 if maximum else "Eindhoven")
            elif tag == 0x4E:
                assert d[1:4] == b"nld" and d[4] == 0 and d[5] == len(d[6:])
                number = d[0] >> 4
                assert number not in extended
                if last_number is not None:
                    assert last_number == d[0] & 15
                last_number = d[0] & 15
                extended[number] = text(d[6:])
            else:
                raise AssertionError(tag)
        assert short_seen and set(extended) == set(range(last_number + 1))
        description = "".join(extended[i] for i in range(last_number + 1))
        expected = "Operator: " + ("é" * 40 + " | " + "語" * 240 if maximum else "René | ATV-contest; 70 cm; antenne richting zuid.")
        assert description == expected
        descriptions.add(description)
    limits = {0: .5, 0x1000: .5, 0x11: 2, 0x12: 2, 0x14: 30}
    assert set(repetitions) == {(0, 0), (0x1000, 0), (0x11, 0), (0x12, 0), (0x12, 1), (0x14, 0)}
    for (pid, _), times in repetitions.items():
        assert times[0] <= limits[pid]
        assert all(b - a <= limits[pid] for a, b in zip(times, times[1:])), (pid, times)
    assert len(descriptions) == 1
    if maximum:
        assert len(days) == len(versions) == 2 and len(repetitions[0x14, 0]) >= 3
        assert abs(clock_origin - 1789689590) < 1
    if shutil.which("ffprobe"):
        probe = subprocess.run(["ffprobe", "-v", "error", "-show_programs", "-of", "json", str(path)], capture_output=True, text=True, check=True)
        program, = json.loads(probe.stdout)["programs"]
        assert program["program_id"] == 1 and program["streams"][0]["codec_name"] == "h264"
    if shutil.which("ffmpeg"):
        decode_path = path
        if live:
            # Stop may cut the last video PES, as in the baseline UDP test.
            assert len(video_starts) > 5
            decode_path = path.with_suffix(".complete-video.ts")
            decode_path.write_bytes(data[:video_starts[-1]])
        result = subprocess.run(["ffmpeg", "-v", "error", "-xerror", "-i", str(decode_path), "-f", "null", "-"], capture_output=True)
        assert result.returncode == 0, result.stderr
    if shutil.which("tsanalyze"):
        result = subprocess.run(["tsanalyze", str(path)], check=True, capture_output=True)
        path.with_suffix(".analysis.txt").write_bytes(result.stdout)
    print(f"{path}: EIT, CRC, continuity, UTF-8, SI deadlines, UTC and video OK")


def capture_udp(windows):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as receiver:
        receiver.bind(("127.0.0.1", 0))
        receiver.settimeout(.2)
        port = receiver.getsockname()[1]
        command = (["wine", "build/udp-sender.exe"] if windows else ["build/udp-sender"])
        process = subprocess.Popen(command + [str(port), "6", "60000", "2", "2", "eit"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=dict(os.environ, WINEPREFIX="/tmp/atv-contest-wine", WINEDEBUG="-all"))
        packets, arrivals = [], []
        deadline = time.monotonic() + 30
        try:
            while process.poll() is None:
                assert time.monotonic() < deadline
                try:
                    data = receiver.recv(2048)
                except socket.timeout:
                    continue
                assert len(data) == 1316
                packets.append(data); arrivals.append(time.monotonic())
            stdout, stderr = process.communicate(timeout=5)
            assert process.returncode == 0, (stdout, stderr)
            status = json.loads(stdout)
            assert status["packets"] == len(packets)
            assert status["stop_ms"] < 2000 and len(packets) > 25
            interval = 1316 * 8 / 60000
            assert abs((arrivals[-1] - arrivals[0]) / (len(arrivals) - 1) - interval) < .02
            path = Path("build/eit-udp-windows.ts" if windows else "build/eit-udp-linux.ts")
            path.write_bytes(b"".join(packets))
            check(path, 60000, live=True)
        finally:
            if process.poll() is None:
                process.kill(); process.communicate()


if __name__ == "__main__":
    if sys.argv[1] == "--udp":
        capture_udp("--windows" in sys.argv)
        sys.exit(0)
    prefix = sys.argv[1]
    for rate in (32000, 60000, 120000, 2000000):
        check(Path(f"{prefix}-{rate}.ts"), rate)
    check(Path(f"{prefix}-midnight.ts"), 120000, True)
