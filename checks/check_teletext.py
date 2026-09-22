"""Independent byte/timing checks plus libzvbi decoder interoperability."""
import glob
import json
from pathlib import Path
import subprocess
import sys
HAM = [0x15, 0x02, 0x49, 0x5e, 0x64, 0x73, 0x38, 0x2f,
       0xd0, 0xc7, 0x8c, 0x9b, 0xa1, 0xb6, 0xfd, 0xea]
def reverse(v):
    return int(f'{v:08b}'[::-1], 2)
def unham(v):
    return HAM.index(reverse(v))
def pts(p):
    assert p[0] & 0xf1 == 0x21 and p[2] & 1 and p[4] & 1
    return ((p[0] >> 1 & 7) << 30) | (p[1] << 22) | ((p[2] >> 1) << 15) | (p[3] << 7) | (p[4] >> 1)
def check(path):
    rate = int(path.stem.split('-')[-2])
    raw = path.read_bytes()
    assert len(raw) % 1316 == 0
    previous = None
    page = None
    last = None
    rows = {}
    pages = 0
    header_time = None
    for i in range(0, len(raw), 188):
        p = raw[i:i+188]
        assert p[0] == 0x47
        pid = ((p[1] & 31) << 8) | p[2]
        if pid != 0x101:
            continue
        mode = p[3] >> 4 & 3
        assert mode in (1, 2)
        if mode == 2:
            assert p[4] == 183 and p[5] == 0x80
            previous = p[3] & 15
            continue
        assert p[1] & 64 and not p[1] & 0x80
        if previous is not None:
            assert p[3] & 15 == (previous + 1) % 16
        previous = p[3] & 15
        pes = p[4:]
        assert pes[:9] == bytes([0, 0, 1, 0xbd, 0, 178, 0x84, 0x80, 0x24])
        assert pes[14:45] == b'\xff' * 31 and pes[45] == 0x10
        stamp = pts(pes[9:14]) / 90000
        start, end = i * 8 / rate, (i + 188) * 8 / rate
        assert end <= stamp <= start + .040
        if page == 100:
            assert end - last <= .10001
        for pos in (46, 92, 138):
            unit = pes[pos:pos+46]
            if unit[0] == 255:
                assert unit == b'\xff\x2c' + b'\xff' * 44
                continue
            assert unit[:4] == b'\x02\x2c\xe0\xe4'
            address = unham(unit[4]) | (unham(unit[5]) << 4)
            assert address & 7 == 1
            row = address >> 3
            if row == 0:
                number = unham(unit[6]) | unham(unit[7]) << 4
                if number == 255:
                    assert page == 100 and set(rows) == set(range(1, 24))
                    assert rows[1].rstrip() == 'ATV CONTEST'
                    assert rows[2].rstrip() == 'Pagina 100'
                    assert rows[4].rstrip() == '73 de PE1ITR'
                    page = None
                    pages += 1
                else:
                    assert number == 0
                    page, rows, header_time = 100, {}, stamp
                    assert [unham(v) for v in unit[8:14]] == [0, 8, 0, 0, 0, 1]
            else:
                assert page == 100 and 1 <= row <= 23
                assert stamp - header_time >= .020
                decoded = [reverse(v) for v in unit[6:]]
                assert all(v.bit_count() % 2 for v in decoded)
                rows[row] = ''.join(chr(v & 127) for v in decoded)
        last = end
    assert pages >= 3
    probe = json.loads(subprocess.check_output(['ffprobe', '-v', 'error', '-show_streams', '-of', 'json', str(path)]))
    stream = next(s for s in probe['streams'] if s['codec_name'] == 'dvb_teletext')
    assert stream['tags']['language'] == 'nld'
    result = subprocess.run(['ffmpeg', '-v', 'error', '-txt_format', 'text', '-txt_page', '100',
                             '-i', str(path), '-map', '0:s:0', '-c:s', 'srt', '-f', 'srt', '-'],
                            check=True, capture_output=True, text=True)
    assert 'ATV CONTEST' in result.stdout and '73 de PE1ITR' in result.stdout
    subprocess.run(['ffmpeg', '-v', 'error', '-i', str(path), '-map', '0:v:0', '-f', 'null', '-'], check=True)
    print(f'{path}: {pages} complete pages; transport, timing, text and H.264 decoding OK')
paths = sorted(Path(p) for p in glob.glob(sys.argv[1] + '-*.ts'))
assert len(paths) == 6
for path in paths:
    check(path)
