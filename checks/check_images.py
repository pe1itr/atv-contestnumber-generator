"""Validate actual JPEG output from --smoke-test; requires Pillow."""
import sys
from pathlib import Path
from PIL import Image, ImageChops, ImageOps

directory = Path(sys.argv[1])
sizes = [(128, 96), (144, 108), (128, 72), (144, 82), (120, 90), (160, 120), (120, 68), (160, 90), (320, 240), (640, 480), (800, 600), (1024, 768),
         (1080, 810), (1280, 960), (1600, 1200), (1920, 1440),
         (320, 180), (640, 360), (800, 450), (960, 540),
         (1024, 576), (1280, 720), (1600, 900), (1920, 1080), (240, 180), (240, 136)]
for colored in (False, True):
    for top_code in (False, True):
        for show_sum in (False, True):
            for inverse in (False, True):
                suffix = ("-blauw-geel" if colored else "") + ("-inverse" if inverse else "") + ("-sum" if show_sum else "") + ("-top" if top_code else "")
                for width, height in sizes:
                    footers = []
                    for show in (0, 1):
                        path = directory / f"test-{width}x{height}-locator{show}{suffix}.jpg"
                        with Image.open(path) as image:
                            assert image.format == "JPEG", path
                            assert image.size == (width, height), path
                            red, green, blue = image.convert("RGB").split()
                            if colored:
                                expected = (255, 255, 0) if inverse else (0, 0, 128)
                                assert max(abs(a-b) for a,b in zip(image.convert("RGB").getpixel((0,0)), expected)) < 35, path
                                assert ImageChops.difference(red, blue).getextrema()[1] > 120, path
                            else:
                                assert ImageChops.difference(red, green).getextrema()[1] <= 3, path
                                assert ImageChops.difference(red, blue).getextrema()[1] <= 3, path
                            gray = image.convert("L")
                            if inverse:
                                gray = ImageOps.invert(gray)
                            if colored:
                                base = 29 if inverse else 15
                                gray = gray.point(lambda v: max(0, min(255, round((v-base)*255/211))))
                            mask = gray.point(lambda value: 255 if value > 128 else 0)
                            corner = mask.crop((width * 4 // 5, 0, width, height * 9 // 100))
                            assert bool(corner.getbbox()) == bool(top_code), path
                            if top_code:
                                assert not mask.crop((0, 0, width // 2, height * 9 // 100)).getbbox(), path
                            assert mask.crop((0, 0, width, height * 22 // 100)).getbbox(), path
                            assert mask.crop((0, height * 22 // 100, width, height * 80 // 100)).getbbox(), path
                            locator = mask.crop((0, height * 80 // 100, width, height * 91 // 100))
                            if show:
                                assert locator.getbbox(), path
                            footer = mask.crop((0, height * 91 // 100, width, height))
                            left = footer.crop((0, 0, width // 2, footer.height)).getbbox()
                            assert bool(left) == bool(show_sum), path
                            if left:
                                assert left[0] >= width // 40, path
                            footer = footer.crop((width // 2, 0, width, footer.height))
                            bbox = footer.getbbox()
                            assert bbox and bbox[0] > 0 and bbox[2] < width // 2, path
                            # At very low resolutions, font metrics round down to whole pixels.
                            assert bbox[3] - bbox[1] >= max(2, int(height * 0.035)), path
                            footers.append(footer)
                            # Text remains inside the image; corners stay black despite JPEG ringing.
                            for point in ((0, 0), (width-1, 0), (0, height-1), (width-1, height-1)):
                                # Tiny images put the text and corners in the same JPEG block.
                                assert gray.getpixel(point) < (20 if width < 320 else 10), (path, point)
                    # Nearby locator pixels can move a thresholded JPEG pixel at tiny sizes.
                    difference = ImageChops.difference(*footers)
                    assert difference.histogram()[255] <= (6 if colored else (2 if width < 320 else 0)), (width, height)
print(f"{32*len(sizes)} JPEGs checked: blue/yellow and monochrome, optional top code, digit sum, normal/inverse, dimensions and text positions.")

# Actual PM5544 JPEGs: color survives and added white lettering remains in the name panels.
for width, height in sizes:
    wide = width * 3 != height * 4
    sw, sh = (1280, 720) if wide else (720, 576)
    panels = ((533, 67, 747, 120), (480, 547, 800, 600)) if wide else ((279, 51, 441, 95), (237, 439, 483, 482))
    with Image.open(directory / f"pm-{width}x{height}-0.jpg") as blank, Image.open(directory / f"pm-{width}x{height}-1.jpg") as text:
        assert blank.format == text.format == "JPEG"
        assert blank.size == text.size == (width, height)
        r, g, b = text.convert("RGB").split()
        assert ImageChops.difference(r, g).getextrema()[1] > 200
        diff = ImageChops.difference(blank.convert("RGB"), text.convert("RGB")).convert("L")
        for x0, y0, x1, y1 in panels:
            box = (x0*width//sw, y0*height//sh, x1*width//sw, y1*height//sh)
            assert diff.crop(box).getextrema()[1] > 100, (width, height, box)
print(f"{2*len(sizes)} PM5544 JPEGs checked: both templates, output dimensions, color and text panels.")

# Independent top/bottom switches: sample all eight bars away from footer text.
bar_colors = [(191,191,191), (191,191,0), (0,191,191), (0,191,0),
              (191,0,191), (191,0,0), (0,0,191), (0,0,0)]
for width, height in sizes:
    for flags in range(4):
        path = directory / f"ebu-{width}x{height}-{flags}.jpg"
        with Image.open(path) as image:
            image = image.convert("RGB")
            for enabled, y in ((flags & 1, 0), (flags & 2, height-1)):
                for bar, color in enumerate(bar_colors):
                    x = width*(2*bar+1)//16
                    expected = color if enabled else (0,0,0)
                    actual = image.getpixel((x,y))
                    # Tiny strips share chroma blocks with the black label backdrops;
                    # JPEG 4:2:0 can halve a primary channel at the bottom edge.
                    tolerance = 115 if width < 320 else 55
                    assert max(abs(a-b) for a,b in zip(actual, expected)) < tolerance, (path, x, y, actual, expected)
            # The central contest number is unaffected by either switch.
            with Image.open(directory / f"ebu-{width}x{height}-0.jpg") as base:
                box = (0, height*30//100, width, height*70//100)
                assert not ImageChops.difference(image.crop(box), base.convert("RGB").crop(box)).getbbox(), path
print(f"{4*len(sizes)} EBU JPEGs checked: independent strips, eight colors, unchanged central number.")

for width, height in sizes:
    wide = width*3 != height*4
    sw, sh = (2000,1125) if wide else (768,576)
    panels = ((545,565,999,638),(1002,565,1455,638)) if wide else ((240,290,382,327),(386,290,528,327))
    with Image.open(directory/f"fubk-{width}x{height}-0.jpg") as blank, Image.open(directory/f"fubk-{width}x{height}-1.jpg") as text, Image.open(directory/f"fubk-{width}x{height}-2.jpg") as six:
        assert blank.size == text.size == six.size == (width,height)
        assert not ImageChops.difference(text,six).getbbox(), "FUBK must truncate locator at six characters"
        diff = ImageChops.difference(blank.convert("RGB"),text.convert("RGB")).convert("L")
        for x0,y0,x1,y1 in panels:
            box = (x0*width//sw,y0*height//sh,x1*width//sw,y1*height//sh)
            assert diff.crop(box).getextrema()[1]>100, (width,height,box)
        # Outside the middle text row, preserve the source test pattern exactly.
        top = max(0, (panels[0][1]*height//sh//16-1)*16)
        bottom = min(height, ((panels[0][3]*height//sh+15)//16+1)*16)
        assert not diff.crop((0,0,width,top)).getbbox()
        assert not diff.crop((0,bottom,width,height)).getbbox()
print(f"{3*len(sizes)} FUBK JPEGs checked: both templates, left/right labels, six-character locator and intact surrounding pattern.")

# PM5644 ROM-derived patterns: both aspect ratios, distinct from PM5544,
# labels in the original name panels and no changes to the central grating.
for width, height in sizes:
    wide = width*3 != height*4
    panels = ((303,59,416,101),(274,437,445,479)) if wide else ((285,55,434,97),(266,433,473,475))
    with Image.open(directory/f"pm5644-{width}x{height}-0.jpg") as blank, Image.open(directory/f"pm5644-{width}x{height}-1.jpg") as text:
        assert blank.format == text.format == "JPEG"
        assert blank.size == text.size == (width,height)
        r,g,b = blank.convert("RGB").split()
        assert ImageChops.difference(r,g).getextrema()[1] > 100
        diff = ImageChops.difference(blank.convert("RGB"),text.convert("RGB")).convert("L")
        for x0,y0,x1,y1 in panels:
            box = (x0*width//720,y0*height//576,x1*width//720,y1*height//576)
            assert diff.crop(box).getextrema()[1] > 100, (width,height,box)
        assert not diff.crop((0,height//3,width,height*2//3)).getbbox()
        with Image.open(directory/f"pm-{width}x{height}-0.jpg") as old:
            assert ImageChops.difference(blank.convert("RGB"),old.convert("RGB")).getbbox()
print(f"{2*len(sizes)} PM5644 JPEGs checked: G00/G924, dimensions, color, name panels and unchanged central grating.")
