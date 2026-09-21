# Generated converter-only source; historical analysis entry points omitted.
"""Reproducible briefing glyph reconstruction after bounded original asset survey.

The seven shapes are new code geometry based on the user's reference screenshot;
they are NOT extracted original MGO2 glyphs. No AI image processing or patch input.
Run with the bundled Python/Pillow runtime. Outputs are isolated from runtime assets.
"""
from pathlib import Path
import hashlib
import json
from PIL import Image, ImageDraw
from title_assets import ROOT, qar, txn, take
OUT = ROOT / 'outputs/briefing_ui_20260913'
NAMES = ('start', 'map', 'rules', 'skills', 'host', 'options', 'quit')
COLORS = {'normal': (157, 115, 47), 'selected': (255, 216, 103)}
SCALE = 4

def record(p):
    b = p.read_bytes()
    return {'path': str(p.resolve()), 'size': len(b), 'sha256': hashlib.sha256(b).hexdigest()}

def mask(name):
    im = Image.new('L', (48 * SCALE, 48 * SCALE))
    d = ImageDraw.Draw(im)

    def pts(p):
        return [(round(x * SCALE), round(y * SCALE)) for x, y in p]

    def poly(p, fill=255):
        d.polygon(pts(p), fill=fill)

    def rect(p, fill=255):
        d.rectangle(tuple((round(v * SCALE) for v in p)), fill=fill)

    def line(p, width=3):
        d.line(pts(p), fill=255, width=round(width * SCALE), joint='curve')

    def ellipse(p, fill=255):
        d.ellipse(tuple((round(v * SCALE) for v in p)), fill=fill)
    if name == 'start':
        for x in (4, 15, 26):
            poly([(x, 8), (x + 5, 8), (x + 17, 24), (x + 5, 40), (x, 40), (x + 12, 24)])
    elif name == 'map':
        poly([(6, 5), (19, 5), (19, 15), (27, 15), (27, 7), (40, 7), (40, 41), (30, 41), (30, 30), (19, 30), (19, 41), (6, 41)])
        rect((10, 10, 14, 24), 0)
        rect((10, 19, 25, 24), 0)
        rect((23, 18, 27, 41), 0)
        rect((32, 12, 36, 25), 0)
        rect((30, 25, 40, 28), 0)
    elif name == 'rules':
        rect((8, 7, 39, 41))
        rect((12, 12, 35, 37), 0)
        rect((19, 4, 28, 10))
        for row in range(5):
            for col in range(5):
                if (row + col) % 2 == 0:
                    rect((12 + col * 4.8, 12 + row * 5, 16.8 + col * 4.8, 17 + row * 5))
    elif name == 'skills':
        ellipse((20, 6, 28, 14))
        poly([(19, 15), (27, 15), (29, 25), (25, 30), (22, 30), (17, 23)])
        line([(24, 18), (31, 21), (35, 15), (41, 15)], 4)
        line([(19, 18), (12, 23), (13, 29)], 4)
        line([(23, 27), (28, 32), (32, 40)], 5)
        line([(21, 27), (17, 33), (10, 39)], 5)
        rect((34, 12, 42, 15))
        rect((39, 11, 42, 14))
    elif name == 'host':
        poly([(10, 5), (32, 5), (39, 12), (39, 42), (10, 42)])
        poly([(32, 5), (32, 13), (39, 13)], 0)
        for y in (26, 31, 36):
            rect((15, y, 34, y + 1.5), 0)
        rect((15, 11, 18, 16), 0)
    elif name == 'options':
        poly([(14, 32), (30, 16), (28, 11), (30, 6), (35, 4), (34, 11), (38, 15), (44, 14), (42, 20), (36, 23), (32, 21), (20, 35)])
        ellipse((5, 29, 21, 45))
        ellipse((9, 33, 17, 41), 0)
    elif name == 'quit':
        rect((7, 6, 40, 42))
        rect((11, 10, 35, 38), 0)
        poly([(13, 11), (33, 8), (33, 41), (13, 38)])
        ellipse((26, 24, 29, 28), 0)
    else:
        raise ValueError(name)
    return im.resize((48, 48), Image.Resampling.LANCZOS)

def colored(alpha, color):
    result = Image.new('RGBA', alpha.size, color + (255,))
    result.putalpha(alpha)
    return result
