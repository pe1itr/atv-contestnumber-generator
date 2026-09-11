"""Validate actual JPEG output from --smoke-test; requires Pillow."""
import sys
from pathlib import Path
from PIL import Image, ImageChops, ImageOps

directory = Path(sys.argv[1])
sizes = [(120, 90), (160, 120), (120, 68), (160, 90), (320, 240), (640, 480), (800, 600), (1024, 768),
         (1080, 810), (1280, 960), (1600, 1200), (1920, 1440),
         (320, 180), (640, 360), (800, 450), (960, 540),
         (1024, 576), (1280, 720), (1600, 900), (1920, 1080)]
for top_code in (False, True):
    for show_sum in (False, True):
        for inverse in (False, True):
            suffix = ("-inverse" if inverse else "") + ("-sum" if show_sum else "") + ("-top" if top_code else "")
            for width, height in sizes:
                footers = []
                for show in (0, 1):
                    path = directory / f"test-{width}x{height}-locator{show}{suffix}.jpg"
                    with Image.open(path) as image:
                        assert image.format == "JPEG", path
                        assert image.size == (width, height), path
                        red, green, blue = image.convert("RGB").split()
                        assert ImageChops.difference(red, green).getextrema()[1] <= 3, path
                        assert ImageChops.difference(red, blue).getextrema()[1] <= 3, path
                        gray = image.convert("L")
                        if inverse:
                            gray = ImageOps.invert(gray)
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
                assert difference.histogram()[255] <= (2 if width < 320 else 0), (width, height)
print("320 JPEGs checked: optional top code, digit sum, normal/inverse, dimensions and text positions.")

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
print("40 PM5544 JPEGs checked: both templates, output dimensions, color and text panels.")
