# Generated converter-only source; historical analysis entry points omitted.
"""Bounded original MTAR rotation/translation reader; Noesis format reference.
Outputs native sampled quaternions, not executable original motion bytecode.
"""
import math, struct
from title_assets import read, take

class Bits:

    def __init__(self, b):
        if len(b) % 2:
            raise ValueError('Motion word alignment')
        self.data = b''.join((b[i:i + 2][::-1] for i in range(0, len(b), 2)))
        self.at = 0

    def read(self, n):
        if n <= 0 or self.at + n > len(self.data) * 8:
            raise ValueError('Motion bit extent')
        v = 0
        for i in range(n):
            v |= (self.data[self.at // 8] >> self.at % 8 & 1) << i
            self.at += 1
        return v

def rotation(b, bits, frames):
    s = Bits(b)
    out = []
    frame = 0
    for _ in range(frames + 2):
        delta = s.read(8)
        frame += delta
        theta = s.read(bits) / (1 << bits) * math.pi
        x = s.read(bits) / (1 << bits)
        y = s.read(bits) / (1 << bits)
        z = 1 - x - y
        axis = [x, y, z]
        for i in range(3):
            if s.read(1):
                axis[i] = -axis[i]
        length = math.sqrt(sum((v * v for v in axis)))
        if length < 1e-09:
            raise ValueError('Motion quaternion axis')
        out.append((frame, [v / length * math.sin(theta / 2) for v in axis] + [math.cos(theta / 2)]))
        if frame >= frames:
            return out
        if len(out) > 1 and (not delta):
            raise ValueError('Nonprogressing rotation')
    raise ValueError('Motion key count')

def translation(b, frames):
    s = Bits(b)
    out = []
    frame = 0
    for _ in range(frames + 2):
        v = [struct.unpack('<e', struct.pack('<H', s.read(16)))[0] * 128 for _ in range(3)]
        delta = s.read(8)
        frame += delta
        if not all((math.isfinite(x) for x in v)):
            raise ValueError('Motion translation')
        out.append((frame, v))
        if frame >= frames:
            return out
        if len(out) > 1 and (not delta):
            raise ValueError('Nonprogressing translation')
    raise ValueError('Motion key count')

def sample(keys, t, quat=False):
    a = keys[0]
    b = a
    for k in keys[1:]:
        b = k
        if t <= k[0]:
            break
        a = k
    f = max(0, min(1, (t - a[0]) / (b[0] - a[0]))) if b[0] != a[0] else 0
    sign = -1 if quat and sum((x * y for x, y in zip(a[1], b[1]))) < 0 else 1
    v = [x * (1 - f) + y * sign * f for x, y in zip(a[1], b[1])]
    if quat:
        length = math.sqrt(sum((x * x for x in v)))
        v = [x / length for x in v]
    return v

def load(b, index=0):
    h = read(b, 0, 'I4H5I')
    if b[:4] != b'ratM' or h[3] > 256 or h[4] > 4096 or (index >= h[4]):
        raise ValueError('MTAR header')
    boneNames = read(b, h[8], str(h[3]) + 'I')
    entry = read(b, h[9] + 16 * index, '4I')
    m = take(b, h[6] + entry[0], entry[1])
    v = read(m, 0, '11I2BH4I')
    name, flags, ticks, frames, archive, size, n = v[:7]
    if not 0 < frames <= 3600 or not 0 < n <= 128 or (not 1 <= v[11] <= v[12] <= 24):
        raise ValueError('MTCM counts')
    data = take(m, archive, size * 2)
    offsets = read(m, 64, str(n) + 'I')
    joints = take(m, v[13], n)
    tracks = {}
    for i, j in enumerate(joints):
        if j >= len(boneNames):
            raise ValueError('MTCM bone index')
        end = offsets[i + 1] if i + 1 < n and offsets[i + 1] > offsets[i] else v[14]
        high = v[7 + i // 32] >> i % 32 & 1
        keys = rotation(take(data, offsets[i] * 2, (end - offsets[i]) * 2), v[12] if high else v[11], frames)
        tracks[boneNames[j]] = [sample(keys, f, True) for f in range(frames + 1)]
    root = translation(take(data, 0, offsets[0] * 2), frames)
    roots = [sample(root, f) for f in range(frames + 1)]
    return {'name': name, 'flags': flags, 'base_tick': ticks, 'frames': frames, 'fps': 60, 'tracks': tracks, 'roots': roots, 'root_bone': boneNames[joints[0]]}
