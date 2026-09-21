# Generated converter-only source; historical analysis entry points omitted.
"""Bounded title asset readers. Reference structures: Jayveer MGS-MDN-Noesis.
These inspect local asset copies, never load or execute extracted code.
"""
from pathlib import Path
import struct, zlib, json, hashlib
ROOT = Path(__file__).resolve().parents[1]

def take(b, p, n):
    if p < 0 or n < 0 or p + n > len(b):
        raise ValueError(f'extent {p:#x}+{n:#x}/{len(b):#x}')
    return b[p:p + n]

def read(b, p, fmt):
    return struct.unpack('>' + fmt, take(b, p, struct.calcsize('>' + fmt)))

def qar(b):
    table, = read(b, len(b) - 4, 'I')
    count, = read(b, table, 'H')
    p = table + 4 + 8 * count
    at = 0
    rows = []
    if count > 10000 or p > len(b) - 4:
        raise ValueError('QAR table')
    for i in range(count):
        key, size = read(b, table + 4 + 8 * i, 'II')
        q = b.find(b'\x00', p, min(p + 256, len(b) - 4))
        if q < 0:
            raise ValueError('QAR name')
        name = b[p:q].decode('ascii')
        p = q + 1
        if any((c in name for c in '/\\:')) or not name:
            raise ValueError('QAR unsafe name')
        if at + size > table:
            raise ValueError('QAR payload/table overlap')
        rows.append({'name': name, 'key': key, 'offset': at, 'size': size})
        at = at + size + 127 & ~127
    if at != table or any(b[p:-4]):
        raise ValueError('QAR unexplained tail')
    return rows

def dlz(b, allow_cd_padding=False, allow_legacy_remaining_size=False):
    out = bytearray()
    segments = []
    legacy_totals = False
    if allow_legacy_remaining_size and b:
        headers = []
        for start in range(0, len(b), 131072):
            if take(b, start, 4) != b'segs':
                raise ValueError('SEGS magic')
            flag, count, total = read(b, start + 4, 'HHI')
            if flag != 4 or count > 4096:
                raise ValueError('SEGS header')
            sizes = [read(b, start + 16 + 8 * i, 'HHI') for i in range(count)]
            headers.append((total, sum((size or 65536 for size, _, _ in sizes)), sum((expected or 65536 for _, expected, _ in sizes))))
        suffix = 0
        valid = True
        for total, size, _ in reversed(headers):
            suffix += size
            valid = valid and total == suffix
        legacy_totals = valid and any((total != expected for total, _, expected in headers))
    for start in range(0, len(b), 131072):
        if b[start:start + 4] != b'segs':
            raise ValueError(f'SEGS magic {start:#x}')
        flag, count, total = read(b, start + 4, 'HHI')
        compressed = struct.unpack('<I', take(b, start + 12, 4))[0]
        if flag != 4 or count > 4096 or total > (len(b) if legacy_totals else 16777216):
            raise ValueError('SEGS header')
        extent = min(131072, len(b) - start)
        if not 16 + 8 * count <= compressed <= extent:
            raise ValueError('SEGS compressed extent')
        if sum((read(b, start + 16 + 8 * i, 'HHI')[1] or 65536 for i in range(count))) > 16777216:
            raise ValueError('SEGS output bounds')
        block = bytearray()
        for i in range(count):
            size, expected, off = read(b, start + 16 + 8 * i, 'HHI')
            size = size or 65536
            expected = expected or 65536
            if off - 1 < 16 + 8 * count or off - 1 + size > compressed:
                raise ValueError('SEGS chunk extent')
            payload = take(b, start + off - 1, size)
            d = zlib.decompressobj(-15)
            plain = d.decompress(payload, expected + 1)
            tail = d.unused_data
            padded = allow_cd_padding and 0 < len(tail) < 16 and (size % 16 == 0) and ((off - 1) % 16 == 0) and (tail == b'\xcd' * len(tail))
            if not d.eof or d.unconsumed_tail or (tail and (not padded)) or (len(plain) != expected):
                raise ValueError('SEGS inflate extent')
            block.extend(plain)
        if not legacy_totals and len(block) != total:
            raise ValueError('SEGS total')
        out.extend(block)
        segments.append({'offset': start, 'chunks': count, 'output': len(block), 'compressed_header': compressed})
        if legacy_totals:
            segments[-1].update(declared_size=total, size_semantics='remaining_compressed_payload')
    return (bytes(out), segments)

def dld(b):
    rows = []
    p = 0
    while p < len(b):
        if not any(b[p:p + 32]) and (not any(b[p:])):
            break
        kind, priority, alignment, pad, *words = read(b, p, '4B7I')
        unused, key, parent, size, mips, index, pad2 = words
        take(b, p + 32, size)
        rows.append({'offset': p, 'kind': kind, 'priority': priority, 'alignment': alignment, 'key': key, 'parent_size': parent, 'size': size, 'mips': mips, 'index': index})
        p = p + 32 + size + 15 & ~15
        if p > len(b):
            raise ValueError('DLD missing alignment')
    return rows

def txn(b):
    pad, flags, ni, io, nt, to, pad1, pad2 = read(b, 0, '8I')
    if ni > 4096 or nt > 65536:
        raise ValueError('TXN count')
    images = []
    textures = []
    for i in range(ni):
        w, h, codec, flag, off, mip = read(b, io + 16 * i, '4H2I')
        images.append({'index': i, 'offset': io + 16 * i, 'width': w, 'height': h, 'codec': codec, 'flags': flag, 'data_offset': off, 'mip_offset': mip})
    for i in range(nt):
        flag, key, archive, w, h, x, y, image, pad, us, vs, uo, vo, pad2 = read(b, to + 48 * i, '3I4H2I4fI')
        if image < io or (image - io) % 16 or (image - io) // 16 >= ni:
            raise ValueError('TXN image reference')
        textures.append({'index': i, 'flags': flag, 'key': key, 'archive': archive, 'width': w, 'height': h, 'x': x, 'y': y, 'image_index': (image - io) // 16, 'uv_scale': [us, vs], 'uv_offset': [uo, vo]})
    return {'flags': flags, 'images': images, 'textures': textures}
