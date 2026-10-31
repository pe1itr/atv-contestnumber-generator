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
def check_bitmap(path):
    # Independent decoder: inspect output from real PES, not the editor model.
    rgb = subprocess.check_output([
        'ffmpeg', '-v', 'error', '-txt_format', 'bitmap', '-txt_page', '100',
        '-txt_chop_top', '0', '-canvas_size', '492x250', '-i', str(path),
        '-filter_complex', '[0:s:0]null[out]', '-map', '[out]', '-ss', '1',
        '-frames:v', '1', '-pix_fmt', 'rgb24', '-threads', '1', '-f', 'rawvideo', '-'])
    assert len(rgb) == 492 * 250 * 3
    def count(row, col, color):
        return sum(rgb[(y*492+x)*3:(y*492+x)*3+3] == bytes(color)
                   for y in range((row+1)*10, (row+2)*10)
                   for x in range(col*12, (col+1)*12))
    for row, col, expected in [(6,2,120),(6,3,120),(6,4,120),(6,5,70),(6,7,120),
                               (7,3,70),(7,4,70),(7,5,70),(7,6,120),(9,7,120),(10,7,120)]:
        assert count(row, col, (0,255,255)) == expected, (path, row, col)
    assert count(10,3,(0,0,255)) == 120 and count(10,9,(0,0,255)) == 120
    # Glyph designs are decoder-specific. Flash/conceal and normative held-mosaic
    # resets are tested in test_teletext_render, not inferred from FFmpeg defaults.
def check(path):
    rate = int(path.stem.split('-')[-2])
    grid = '-grid-' in path.name
    interval = int(path.stem.split('-')[-1]) if grid else 2
    starts = []
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
        if page is not None:
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
                    assert page is not None and set(rows) == set(range(1, 24))
                    if grid:
                        assert page == 100 + pages % 4
                        assert rows[1].rstrip() == f'CAROUSEL PAGE {page}'
                        assert rows[3][:13] == ''.join(map(chr, [1,82,17,63,127,7,84,3,29,2,71,28,75]))
                        assert rows[4][:8] == ''.join(map(chr, [22,33,34,36,40,48,96,127]))
                        assert rows[5].rstrip() == ''.join(map(chr, [4,29,7])) + 'WHITE ON BLUE'
                        assert rows[6][:5] == ''.join(map(chr, [22,20,29,22,127]))
                        assert rows[7][:8] == ''.join(map(chr, [22,127,30,26,31,127,25,127]))
                        assert rows[8][:7] == ''.join(map(chr, [22,26,127,30,25,31,127]))
                        assert rows[9][:10] == ''.join(map(chr, [7,24,72,73,1,82,8,70,9,83]))
                        assert rows[10][:10] == ''.join(map(chr, [4,29,7,78,13,65,22,127,12,90]))
                        assert rows[11].rstrip() == 'THIS LOWER ROW MUST BE IGNORED'
                        assert rows[12][:13] == ''.join(map(chr, [22,127,30,7,22,29,127,12,29,13,29,12,29]))
                        assert rows[14][:7] == ''.join(map(chr, [22,127,8,30,9,31,127]))
                    else:
                        assert rows[1].rstrip() == 'ATV CONTEST'
                        assert rows[2].rstrip() == 'Pagina 100'
                        assert rows[4].rstrip() == '73 de PE1ITR'
                    page = None
                    pages += 1
                else:
                    assert number == (pages % 4 if grid else 0)
                    page, rows, header_time = 100 + number, {}, stamp
                    starts.append(stamp)
                    if grid:
                        header = ''.join(chr(reverse(v) & 127) for v in unit[14:])
                        assert header.rstrip() == f'CAROUSEL PAGE {page}'
                    assert [unham(v) for v in unit[8:14]] == [0, 8, 0, 0, 0, 1]
            else:
                assert page is not None and 1 <= row <= 23
                assert stamp - header_time >= .020
                decoded = [reverse(v) for v in unit[6:]]
                assert all(v.bit_count() % 2 for v in decoded)
                rows[row] = ''.join(chr(v & 127) for v in decoded)
        last = end
    assert pages >= (5 if grid else 3)
    assert all(abs(b-a-interval) < .04 for a,b in zip(starts,starts[1:]))
    probe = json.loads(subprocess.check_output(['ffprobe', '-v', 'error', '-show_streams', '-of', 'json', str(path)]))
    stream = next(s for s in probe['streams'] if s['codec_name'] == 'dvb_teletext')
    assert stream['tags']['language'] == 'nld'
    for selected in (range(100,104) if grid else [100]):
        result = subprocess.run(['ffmpeg', '-v', 'error', '-txt_format', 'text', '-txt_page', str(selected),
                                 '-i', str(path), '-map', '0:s:0', '-c:s', 'srt', '-f', 'srt', '-'],
                                check=True, capture_output=True, text=True)
        if grid:
            assert f'CAROUSEL PAGE {selected}' in result.stdout
            assert all(f'CAROUSEL PAGE {other}' not in result.stdout for other in range(100,104) if other != selected)
        else:
            assert 'ATV CONTEST' in result.stdout and '73 de PE1ITR' in result.stdout
    subprocess.run(['ffmpeg', '-v', 'error', '-i', str(path), '-map', '0:v:0', '-f', 'null', '-'], check=True)
    if grid:
        check_bitmap(path)
    print(f'{path}: {pages} complete pages; transport, timing, text, bitmap attributes and H.264 decoding OK')
paths = sorted(Path(p) for p in glob.glob(sys.argv[1] + '-*.ts'))
assert len(paths) == 9
for path in paths:
    check(path)
