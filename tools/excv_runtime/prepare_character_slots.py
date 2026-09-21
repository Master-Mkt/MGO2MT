# Generated converter-only source; historical analysis entry points omitted.
"""Local SLOT section extraction; Solideye slot/config format reference.
No code execution, decryption, source modification or publication.
"""
from pathlib import Path
import json
from title_assets import ROOT, read, take, dlz
from gwp import record

def trim_dld(b):
    p = 0
    while p + 32 <= len(b):
        kind, priority, alignment, pad, unused, key, parent, size, mips, index, pad2 = read(b, p, '4B7I')
        if kind != 2 or alignment != 16 or pad or unused or pad2 or (size > len(b) - p - 32):
            if len(b) - p >= 2048:
                raise ValueError('Invalid SLOT DLD body')
            break
        p = p + 32 + size + 15 & ~15
    return take(b, 0, p)

def entries(b):
    _, version, pageSize, pages, _, _ = read(b, 0, 'I4HI')
    if version != 1 or not 0 < pages <= 4096 or pageSize != 1:
        raise ValueError('SLOT header')
    at = 2048
    for page in range(pages):
        n, pad = read(b, at, '2I')
        if not 1 < n <= 10000 or pad:
            raise ValueError('SLOT tag count')
        tags = [read(b, at + 8 + 16 * i, 'IIQ') for i in range(n)]
        cursor = 8 + 16 * n
        section = None
        for i, (id, size, offset) in enumerate(tags):
            kind = id >> 24
            key = id & 16777215
            if kind == 127:
                if key:
                    cursor = cursor + 2047 & ~2047
                    section = take(b, at + cursor, offset)
                else:
                    cursor += offset
                    section = None
            elif kind == 126:
                raise ValueError('Compressed SLOT section requires separate review')
            elif kind and section is not None and (i + 1 < n):
                end = tags[i + 1][2]
                if end < offset:
                    raise ValueError('SLOT file extent')
                if kind in (3, 13, 33):
                    yield (page, kind, key, at + cursor + offset, take(section, offset, end - offset))
        at += cursor + 2047 & ~2047
    if at != len(b):
        raise ValueError('SLOT tail')
