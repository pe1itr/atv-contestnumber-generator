#!/usr/bin/env python3
"""Check encoded DAR, transport and visible fallback text centering."""
import json
from fractions import Fraction
from integration import EXE, OUT, run, check

for width, aspect, height in [(320,43,240),(320,169,180),(720,169,406),(1280,169,720),(1920,169,1080),(1920,43,1440)]:
    name=f'aspect-{width}-{aspect}'
    config=OUT/(name+'.ini')
    run([EXE,'--write-defaults',config])
    text=config.read_text().replace('width=320',f'width={width}').replace('aspect=43',f'aspect={aspect}').replace('callsign=NOCALL','callsign=PE1TEST')
    assert 'height=' not in text
    config.write_text(text)
    capture=OUT/(name+'.ts')
    run([EXE,'--headless','--config',config,'--seconds','4','--capture',capture])
    check(capture,440310,callsign='PE1TEST',service_type=0x19 if width>720 or height>576 else 0x16)
    result=json.loads(run(['ffprobe','-v','error','-select_streams','v:0','-show_entries','stream=width,height,display_aspect_ratio','-of','json',capture]).stdout)['streams'][0]
    assert (result['width'],result['height'])==(width,height),result
    assert Fraction(result['display_aspect_ratio'].replace(':','/'))==Fraction(16,9) if aspect==169 else Fraction(result['display_aspect_ratio'].replace(':','/'))==Fraction(4,3)
    import subprocess
    raw=subprocess.run(['ffmpeg','-v','error','-i',str(capture),'-frames:v','1','-pix_fmt','rgb24','-f','rawvideo','-'],capture_output=True,check=True).stdout
    points=[(n%width,n//width) for n in range(width*height) if min(raw[3*n:3*n+3])>180]
    x0,x1=min(x for x,y in points),max(x for x,y in points)
    y0,y1=min(y for x,y in points),max(y for x,y in points)
    assert abs((x0+x1)/2-(width-1)/2)<=2,(x0,x1,width)
    assert abs((y0+y1)/2-(height-1)/2)<=2,(y0,y1,height)
    rows=sorted(set(y for x,y in points));groups=[]
    for y in rows:
        if not groups or y>groups[-1][-1]+1:groups.append([])
        groups[-1].append(y)
    assert len(groups)==2,groups
    for group in groups:
        xs=[x for x,y in points if y in group]
        assert abs((min(xs)+max(xs))/2-(width-1)/2)<=2
    run(['ffmpeg','-v','error','-i',capture,'-frames:v','1',OUT/(name+'.png')])
    print('OK',name,result,'centered visible text',flush=True)
    if width>720:
        live=OUT/(name+'-live.ts')
        run([EXE,'--headless','--config',config,'--test-source','--seconds','4','--capture',live])
        check(live,440310,callsign='PE1TEST',service_type=0x19 if width>720 or height>576 else 0x16)
print('Captures:',OUT)
