# Generated converter-only source; historical analysis entry points omitted.
"""Lossless source inventory and diagnostic previews for reviewed stage alpha."""
from pathlib import Path
import collections, hashlib, json, struct, zlib
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'outputs/texture_alpha_20260921'

def digest(b):
    return hashlib.sha256(b).hexdigest()

def model(path):
    b = path.read_bytes()
    version, nv, ni, np, nt = struct.unpack_from('<5I', b, 4)
    assert b[:4] == b'GWM1' and version == 2
    po = 48 + nv * 48 + ni * 4
    parts = [struct.unpack_from('<5I', b, po + i * 32) for i in range(np)]
    at = po + np * 32
    textures = []
    for _ in range(nt):
        w, h, c, n = struct.unpack_from('<4I', b, at)
        textures.append((w, h, c, b[at + 16:at + 16 + n]))
        at += 16 + n
    assert at == len(b)
    return (b, nv, ni, parts, textures)

def rgba(texture):
    w, h, c, raw = texture
    stride = {9: 8, 11: 16}[c]
    out = bytearray(w * h * 4)

    def rgb(v):
        return [(v >> 11 & 31) * 255 // 31, (v >> 5 & 63) * 255 // 63, (v & 31) * 255 // 31]
    for by in range((h + 3) // 4):
        for bx in range((w + 3) // 4):
            off = (by * ((w + 3) // 4) + bx) * stride
            co = off + (8 if c == 11 else 0)
            a, b, bits = struct.unpack_from('<HHI', raw, co)
            colors = [rgb(a), rgb(b)]
            if a > b or c == 11:
                colors.extend([[(2 * x + y) // 3 for x, y in zip(*colors)], [(x + 2 * y) // 3 for x, y in zip(*colors)]])
            else:
                colors.extend([[(x + y) // 2 for x, y in zip(*colors)], [0, 0, 0]])
            if c == 11:
                a0, a1 = raw[off:off + 2]
                alphas = [a0, a1] + ([(a0 * (7 - i) + a1 * i) // 7 for i in range(1, 7)] if a0 > a1 else [(a0 * (5 - i) + a1 * i) // 5 for i in range(1, 5)] + [0, 255])
                abits = int.from_bytes(raw[off + 2:off + 8], 'little')
            for n in range(16):
                x = bx * 4 + n % 4
                y = by * 4 + n // 4
                if x >= w or y >= h:
                    continue
                index = bits >> n * 2 & 3
                alpha = alphas[abits >> n * 3 & 7] if c == 11 else 0 if a <= b and index == 3 else 255
                out[(y * w + x) * 4:(y * w + x) * 4 + 4] = bytes(colors[index] + [alpha])
    return out

def png(path, w, h, raw):

    def chunk(t, b):
        return struct.pack('>I', len(b)) + t + b + struct.pack('>I', zlib.crc32(t + b))
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>2I5B', w, h, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(b''.join((b'\x00' + raw[y * w * 4:(y + 1) * w * 4] for y in range(h))), 9)) + chunk(b'IEND', b''))
