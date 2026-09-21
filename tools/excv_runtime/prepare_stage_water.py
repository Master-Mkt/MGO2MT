# Generated converter-only source; historical analysis entry points omitted.
"""Extract verified root-0 water FIELDs; never infer a volume from a polygon.

Current ELF 186E88/18C1B0 stride table 1229740, 180DE8 FIELD layout,
1B1168 water hash and 395C0 level consumer. Source assets remain untouched.
"""
import argparse, hashlib, json, math, struct
from pathlib import Path
from stage_scene import geometry, hash24
from title_assets import read, take

def extract_surfaces(path):
    """Finite named kind2 geometry for a native crossing adapter, never a volume."""
    from stage_geometry import polygons
    b = path.read_bytes()
    p = polygons(b, allow_stale_size=True, world_only=True)
    named = {v['offset']: v for v in p['primitives'] if v['kind'] == 2 and v['name'] & 16777215 == hash24('water')}
    rows = []
    for t in p['triangles']:
        if t['primitive'] not in named:
            continue
        vertices = [list(v) for v in (p['vertices'][i] for i in t['vertices'])]
        vertices = [list(struct.unpack('<3f', struct.pack('<3f', *v))) for v in vertices]
        a, c, d = vertices
        e = [c[i] - a[i] for i in range(3)]
        f = [d[i] - a[i] for i in range(3)]
        normal = [e[1] * f[2] - e[2] * f[1], e[2] * f[0] - e[0] * f[2], e[0] * f[1] - e[1] * f[0]]
        if not any(normal):
            raise ValueError('degenerate named water surface')
        rows.append({'vertices': vertices, 'primitive_offset': t['primitive'], 'block_offset': named[t['primitive']]['block'], 'attribute': hex(t['attribute']), 'polygon_attribute': hex(t['polygon_attribute'])})
    if len(rows) > 4096:
        raise ValueError('water surface count')
    return {'source': str(path.resolve()), 'source_sha256': hashlib.sha256(b).hexdigest(), 'source_bytes': len(b), 'named_primitive_count': len(named), 'triangles': rows, 'original_level_query_accepts_kind2': False, 'water_level': None, 'depth': None, 'speed_ratio': None, 'limits': 'Finite world kind2 water surfaces only. Native segment crossing is distinct from original point-query and BOX/FIELD water volume semantics. No infinite plane, synthesized depth, slowdown or shader parity.'}

def extract(path):
    b = path.read_bytes()
    meta = geometry(b, allow_stale_size=True)
    blocks = []
    at = 32 + 12 * len(meta['chunks']) + 36
    for group in range(256):
        take(b, at, 64)
        head, flag = read(b, at + 44, '2I')
        data, block = read(b, at + 56, '2I')
        n = head - (block - data)
        if n < 0 or n % 32 or n > 3200000:
            raise ValueError('group extent')
        take(b, block, n)
        blocks.extend(((block + i * 32, 'world', group) for i in range(n // 32)))
        at += 64
        if flag == 1:
            break
    else:
        raise ValueError('group terminator')
    seen = set()
    rows = []
    fields = {}
    errors = []
    visited = set()
    for block, owner, group in blocks:
        if block in seen:
            continue
        seen.add(block)
        take(b, block, 32)
        count = b[block + 1]
        at = read(b, block + 16, 'I')[0]
        pending = [at] if count else []
        chain_seen = set()
        for index in range(65536):
            if not pending:
                break
            at = pending.pop()
            if at in chain_seen:
                continue
            chain_seen.add(at)
            try:
                length, typ, field, flags = read(b, at, '4B')
                kind = flags & 7
                if kind > 5:
                    raise ValueError('primitive kind')
                payload = 32 if kind == 4 else length * {0: 32, 1: 48, 2: 8, 3: 96, 5: 112}[kind]
                extra = 8 if kind == 2 and length % 2 else 16 if kind == 3 and flags == 35 or (kind == 4 and typ and (flags == 36)) else 0
                take(b, at, 32 + payload + extra)
                name = read(b, at + 16, 'I')[0] & 16777215
                if name == hash24('water'):
                    visited.add(at)
                    row = {'offset': at, 'block': block, 'owner': owner, 'group': group, 'kind': kind, 'count': length, 'flags': flags, 'attribute': hex(read(b, at + 24, 'Q')[0])}
                    if kind == 4:
                        size = list(read(b, at + 32, '3f'))
                        center = list(read(b, at + 48, '3f'))
                        root = read(b, at + 60, 'I')[0]
                        row.update(center=center, half_size=size, root_id=root)
                        if length == 1 and root == 0 and (owner == 'world') and all((math.isfinite(x) and abs(x) <= 10000000.0 for x in center + size)) and all((x > 0 for x in size)):
                            key = tuple(center + size)
                            fields.setdefault(key, {'center': center, 'half_size': size, 'source_offsets': [], 'groups': []})['source_offsets'].append(at)
                            if group not in fields[key]['groups']:
                                fields[key]['groups'].append(group)
                            row['supported'] = True
                    rows.append(row)
                nxt, prev, child = read(b, at + 4, '3i')
                for delta in (nxt, child):
                    if delta:
                        target = at + 16 * delta
                        take(b, target, 32)
                        pending.append(target)
            except (ValueError, IndexError, struct.error) as e:
                errors.append({'block': block, 'primitive_index': index, 'offset': at, 'error': str(e)})
                break
    raw = set()
    needle = b'\x00\xa2]\x19'
    at = 0
    while (at := b.find(needle, at)) >= 0:
        if at >= 16 and (at - 16) % 16 == 0:
            raw.add(at - 16)
        at += 4
    if raw - visited:
        raise ValueError(f'unvisited water name candidates: {sorted(raw - visited)}')
    if errors:
        raise ValueError(f'incomplete primitive traversal: {errors[:3]}')
    report = {'source': str(path.resolve()), 'source_sha256': hashlib.sha256(b).hexdigest(), 'source_bytes': len(b), 'declared_bytes': read(b, 4, 'I')[0], 'chunks': meta['chunks'], 'water_hash': '0xA25D19', 'primitive_stride': [32, 48, 8, 96, 32, 112], 'blocks_checked': len(seen), 'reference_blocks_not_parsed': sum((r['blocks'] for r in meta['collision_references'])), 'water_primitives': rows, 'fields': list(fields.values()), 'unsupported_water_primitives': sum((not r.get('supported', False) for r in rows)), 'movement_scale': None, 'splash_cue': None, 'footstep_cue': None, 'limits': 'Root-0 world FIELDs only. Polygons are evidence, not synthesized volumes. Region activation and overlapping-volume query order are not reproduced.'}
    return report
