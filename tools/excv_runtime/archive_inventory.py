# Generated converter-only source; historical analysis entry points omitted.
"""Read-only catalog of title DARs; format hypothesis from local Solideye DAR source.
Require every entry boundary to validate. Only layout candidates are copied locally.
"""
from pathlib import Path
import hashlib, json, struct
ROOT = Path(__file__).resolve().parents[1]
sha = lambda b: hashlib.sha256(b).hexdigest()

def strcode(name):
    n = 0
    for c in name.encode('ascii'):
        n = (n >> 19 | n << 5) + c & 16777215
    return n or 1

def dar(b):
    if len(b) < 4:
        raise ValueError('truncated count')
    count = int.from_bytes(b[:4], 'big')
    if not 0 < count <= 100000:
        raise ValueError('bad count')
    p = 4
    entries = []
    for _ in range(count):
        q = b.find(b'\x00', p, min(p + 256, len(b)))
        if q < 0:
            raise ValueError('filename')
        name = b[p:q].decode('ascii')
        if not name or any((c in name for c in '/\\:')) or name in ('.', '..'):
            raise ValueError('unsafe name')
        p = q + 1 + 3 & ~3
        if p + 4 > len(b):
            raise ValueError('size')
        size = int.from_bytes(b[p:p + 4], 'big')
        p = p + 4 + 15 & ~15
        if p + size + 1 > len(b):
            raise ValueError('payload')
        entries.append({'name': name, 'offset': hex(p), 'size': size, 'stem_hash24': hex(strcode(Path(name).stem)), 'sha256': sha(b[p:p + size])})
        p += size + 1
    if any(b[p:]):
        raise ValueError('unexplained nonzero tail')
    return (entries, p, len(b) - p)
