# Generated converter-only source; historical analysis entry points omitted.
"""Independent bounded GEOM polygon reader, following HavenPX's disk layout.

Preserve primitive/material attributes. Polygon extraction is not a gameplay
collision implementation: box/line/ref behavior and masks require runtime work.
"""
import collections, json, math, struct
from stage_scene import ROOT, geometry
from title_assets import read, take
from gwp import record

def polygons(b, allow_stale_size=False, world_only=False):
    meta = geometry(b, allow_stale_size=allow_stale_size)
    groups = []
    blocks = []
    vertices = []
    triangles = []
    primitives = []
    seen = set()
    unresolved = []
    at = 32 + 12 * len(meta['chunks']) + 36
    for _ in range(256):
        take(b, at, 64)
        head, flag = read(b, at + 44, '2I')
        data, block = read(b, at + 56, '2I')
        n = head - (block - data)
        if n < 0 or n % 32 or n > 3200000:
            raise ValueError('GEOM group block extent')
        material = read(b, at + 12, 'I')[0]
        take(b, block, n)
        groups.append({'offset': at, 'block_offset': block, 'blocks': n // 32, 'material_offset': material})
        blocks.extend(((block + i * 32, 'world', material) for i in range(n // 32)))
        at += 64
        if flag == 1:
            break
    else:
        raise ValueError('GEOM group terminator')
    for ref in [] if world_only else meta['collision_references']:
        take(b, ref['block_offset'], ref['blocks'] * 32)
        blocks.extend(((ref['block_offset'] + 32 * i, ref['hash'], 0) for i in range(ref['blocks'])))
    for offset, owner, group_material in blocks:
        if offset in seen:
            continue
        seen.add(offset)
        count = b[offset + 1]
        vo, fo, mo = read(b, offset + 12, '3I')
        material_header = mo or group_material
        if not vo:
            unresolved.append({'offset': offset, 'owner': owner, 'primitive_count': count, 'face_offset': fo, 'reason': 'no vertex arena; shared/compact block semantics unresolved'})
            continue
        base = len(vertices)
        local = []
        if vo:
            length, start, face, position = read(b, vo, '4I')
            if length > 65536 or start > length or position >= length:
                raise ValueError('GEOM vertex header')
            take(b, vo + 16, length * 16)
            origin = read(b, vo + 16 + position * 16, '3f')
            local = [tuple((x + y for x, y in zip(read(b, vo + 16 + i * 16, '3f'), origin))) for i in range(start, length)]
            if not all((math.isfinite(x) and abs(x) < 10000000.0 for v in local for x in v)):
                raise ValueError('GEOM vertex value')
            vertices.extend(local)
        pending = [fo]
        visited = set()
        while pending:
            at = pending.pop()
            if at in visited:
                continue
            visited.add(at)
            if len(visited) > count:
                raise ValueError('GEOM primitive count overflow')
            length, typ, field, flags = read(b, at, '4B')
            kind = flags & 7
            if kind > 5:
                raise ValueError(('GEOM unknown primitive', hex(at), hex(offset), owner, count, vo, fo, b[at:at + 32].hex()))
            name = read(b, at + 16, 'I')[0]
            attribute = read(b, at + 24, 'Q')[0]
            payload = 32 if kind == 4 else length * {0: 32, 1: 48, 2: 8, 3: 96, 5: 112}[kind]
            extra = 8 if kind == 2 and length % 2 else 16 if kind == 3 and flags == 35 or (kind == 4 and typ and (flags == 36)) else 0
            take(b, at, 32 + payload + extra)
            primitives.append({'offset': at, 'owner': owner, 'block': offset, 'kind': kind, 'count': length, 'name': name, 'attribute': attribute, 'flags': flags, 'material_offset': mo})
            if kind == 2 and vo:
                for i in range(length):
                    lo = take(b, at + 32 + i * 8, 8)
                    ids = [lo[j] + ((lo[4] >> 2 * j & 3) << 8) for j in range(4)]
                    if max(ids) >= len(local):
                        raise ValueError(('GEOM polygon vertex index', hex(offset), hex(at), owner, ids, len(local), vo, fo))
                    for t in [(ids[0], ids[1], ids[2]), (ids[0], ids[2], ids[3])]:
                        if len(set(t)) == 3:
                            triangles.append({'vertices': [base + x for x in t], 'owner': owner, 'primitive': at, 'attribute': attribute, 'polygon_attribute': int.from_bytes(lo[6:8], 'big'), 'material_header': material_header})
            for field in (4, 12):
                units = read(b, at + field, 'i')[0]
                if units:
                    if abs(units) * 16 < 32:
                        raise ValueError('GEOM primitive link extent')
                    if at + units * 16 + 32 > len(b):
                        raise ValueError(('GEOM link', hex(at), field, hex(units), hex(offset), count, kind))
                    take(b, at + units * 16, 32)
                    pending.append(at + units * 16)
        if len(visited) != count:
            raise ValueError(('GEOM primitive count mismatch', hex(offset), count, len(visited), [hex(v) for v in visited]))
    return {'groups': groups, 'vertices': vertices, 'triangles': triangles, 'primitives': primitives, 'unresolved_blocks': unresolved, 'properties': meta['properties'], 'references': meta['collision_references']}
