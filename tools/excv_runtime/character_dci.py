# Generated converter-only source; historical analysis entry points omitted.
"""Bounded local DCI image-index remap; observed resident DCI/DLD/TXN data.
The two u16s are old/new indices; 0xffff suppresses an obsolete image.
"""
from title_assets import read, take

def remaps(b):
    key, block, n, pad = read(b, 0, '4I')
    if not key or block != 4096 or pad or (n > 4096):
        raise ValueError('DCI header')
    out = {}
    for i in range(n):
        key, offset, flags = read(b, 16 + 12 * i, '3I')
        count = flags & 65535
        if not count:
            continue
        if offset < 16 + 12 * n:
            raise ValueError('DCI map overlap')
        take(b, offset, count * 4)
        mapping = {}
        for j in range(count):
            old, new = read(b, offset + j * 4, '2H')
            if old in mapping:
                raise ValueError('Duplicate DCI source index')
            mapping[old] = new
        out[key] = mapping
    return out
