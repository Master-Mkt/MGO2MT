# Generated converter-only source; historical analysis entry points omitted.
"""Bounded stage dependency inspection of local GCX, GEOM and LT3.

References: local HavenPX stage editor structures and MGO2 DG_GetLight PPC.
No source asset writes and no script execution. Out-of-parent GCX blocks are
reported explicitly and must not be used as proof of runtime branch reachability.
"""
import collections
import json
import math
import struct
from pathlib import Path
from gcx_inspect import GcxReader, walk
from title_assets import read, take
from gwp import record
ROOT = Path(__file__).resolve().parents[1]

def hash24(s):
    h = 0
    for c in s.encode('utf-8'):
        h = (h >> 19 | h << 5) + c & 16777215
    return h

def foreach_rows(node):
    """E1A50: consume argc values exactly repeat times, not all trailing data.

    Only literal loop bounds are accepted. This is an audit expansion, not
    execution of GCX conditions or actor callbacks. Options use their letters
    here because the original handler calls GCL_GetOption('a'/'r'/'d'/'e').
    """
    args = node.get('arguments', [])
    if node.get('code') != '0x82bc9' or not args or args[0].get('value') != 5516077:
        raise ValueError('not the reviewed NewForeach handler')
    options = {o['letter']: o['children'] for o in node.get('children', []) if o['kind'] == 'option'}

    def integer(letter):
        values = options.get(letter, [])
        if len(values) != 1 or values[0]['kind'] != 'integer':
            raise ValueError('unresolved foreach bound')
        return values[0]['value']
    argc, repeat = (integer('a'), integer('r'))
    if not 1 <= argc <= 32 or not 0 <= repeat <= 4096:
        raise ValueError('foreach expansion bound')
    data = options.get('d', [])
    if len(data) < argc * repeat:
        raise ValueError('truncated foreach argument stream')
    return [data[i * argc:(i + 1) * argc] for i in range(repeat)]

class InventoryReader(GcxReader):
    """Keep absolute file extents strict; record known nested length discrepancies."""

    def __init__(self, b):
        super().__init__(b)
        self.anomalies = []
        self.procedure_end = len(b)

    def block(self, p, end):
        size = self.uint(p, 1, end) & 15
        p += 1
        if size >= 13:
            count = size - 12
            size = self.uint(p, count, end)
            p += count
        finish = p + size
        if finish > end:
            if finish - end > 4 or finish > self.procedure_end:
                raise ValueError(f'GCX block outside procedure: {p:#x}')
            self.anomalies.append({'offset': p, 'parent_end': end, 'declared_end': finish})
        self.take(p, size, max(end, finish))
        return (p, finish)

    def token(self, p, end, depth):
        n, finish = super().token(p, end, depth)
        for c in n.get('children', []):
            finish = max(finish, int(c['end'], 16))
        n['end'] = hex(finish)
        return (n, finish)

def scripts(b, selected_procedures=None):
    u = lambda p: struct.unpack('<I', take(b, p, 4))[0]
    p = 4
    offsets = []
    while u(p) != 4294967295:
        offsets.append(u(p) & 16777215)
        p += 4
        if len(offsets) > 4096:
            raise ValueError('GCX procedure count')
    base = p + 4
    body = base + u(base) + 4
    size = u(body - 4)
    take(b, body, size)
    physical = sorted(set(offsets)) + [size]
    rows = []
    for i, start in enumerate(offsets):
        end = physical[physical.index(start) + 1]
        rows.append((i + 1, body + start, body + end))
    main = body + size + 4
    rows.append(('main', main, main + u(main - 4)))
    reader = InventoryReader(b)
    parsed = []
    for name, start, end in rows:
        if selected_procedures is not None and name not in selected_procedures:
            continue
        reader.procedure_end = end
        parsed.append({'procedure': name, 'offset': start, 'end': end, 'nodes': reader.seq(start, end)})
    return (parsed, reader.anomalies)

def geometry(b, allow_stale_size=False):
    version, size, count, _ = read(b, 0, '4I')
    if size != len(b) and (not allow_stale_size) or not 0 < count <= 16:
        raise ValueError('GEOM header extent')
    chunks = []
    for i in range(count):
        kind, pad, length, offset = read(b, 32 + 12 * i, 'HHII')
        take(b, offset, length)
        chunks.append({'type': kind, 'offset': offset, 'size': length, 'pad': pad})
    if size != len(b) and (chunks[-1]['offset'] + chunks[-1]['size'] != len(b) or any((a['offset'] + a['size'] > z['offset'] for a, z in zip(chunks, chunks[1:])))):
        raise ValueError('GEOM stale-size chunk extent')
    for i, c in enumerate(chunks[:-1]):
        c['extent'] = max(c['size'], chunks[i + 1]['offset'] - c['offset'])
    if chunks:
        chunks[-1]['extent'] = len(b) - chunks[-1]['offset']
    effects, refs = ([], [])
    for c in chunks:
        data = take(b, c['offset'], c['extent'])
        if c['type'] == 6:
            seen = set()

            def chain(at, parent=None, parent_offset=None, depth=0):
                if depth > 64:
                    raise ValueError('GEOM property depth')
                while True:
                    if at in seen or len(seen) > 100000:
                        raise ValueError('GEOM property cycle')
                    seen.add(at)
                    nxt, child, key, index = read(data, at, '4I')
                    pos, rot, scale = [index >> shift & 1023 for shift in (0, 10, 20)]
                    e = {'hash': key, 'offset': c['offset'] + at, 'parent': parent, 'packed_slots': index, 'parent_offset': parent_offset, 'next_offset': c['offset'] + at + nxt if nxt else None, 'child_offset': c['offset'] + at + child if child else None}
                    if pos:
                        e['position'] = read(data, at + 8 * pos, '4f')
                    if rot:
                        e['rotation_units'] = read(data, at + 8 * rot, '3h')
                        e['rotation_degrees'] = [v * 360 / 65536 for v in e['rotation_units']]
                    if scale:
                        e['scale_raw'] = read(data, at + 8 * scale, '4f')
                    effects.append(e)
                    if child:
                        chain(at + child, key, c['offset'] + at, depth + 1)
                    if not nxt:
                        break
                    at += nxt
            if data:
                chain(0)
        if c['type'] == 1:
            at = 0
            while at + 112 <= len(data):
                blocks = read(data, at + 14, 'H')[0]
                if not blocks:
                    break
                refs.append({'offset': c['offset'] + at, 'blocks': blocks, 'size': read(data, at, '3f'), 'position': read(data, at + 16, '3f'), 'matrix': read(data, at + 32, '16f'), 'attribute': read(data, at + 96, 'Q')[0], 'block_offset': read(data, at + 104, 'I')[0], 'hash': read(data, at + 108, 'I')[0]})
                at += 112
    return {'version': version, 'chunks': chunks, 'properties': effects, 'collision_references': refs}

def property_children(geo, parent_hash, active_parent_offsets=None):
    """1A1468/1A1348: immediate children in link order, never the parent.

    A hash is not a node identity (n022a's CBOX children all share a hash).
    More than one matching container requires the caller's reviewed active
    GEOM-root order. A flat hash match or a recursive walk is not equivalent.
    """
    nodes = {p['offset']: p for p in geo['properties']}
    if len(nodes) != len(geo['properties']):
        raise ValueError('duplicate GEOM node offset')
    if active_parent_offsets is None:
        active_parent_offsets = [p['offset'] for p in geo['properties'] if p['hash'] == parent_hash]
        if len(active_parent_offsets) > 1:
            raise ValueError('ambiguous active GEOM parent order')
    result = []
    seen = set()
    if len(set(active_parent_offsets)) != len(active_parent_offsets):
        raise ValueError('duplicate active GEOM parent')
    for offset in active_parent_offsets:
        parent = nodes.get(offset)
        if not parent or parent['hash'] != parent_hash:
            raise ValueError('invalid active GEOM parent')
        at = parent['child_offset']
        while at is not None:
            child = nodes.get(at)
            if at in seen or not child or child['parent_offset'] != offset:
                raise ValueError('invalid GEOM child chain')
            seen.add(at)
            result.append(child)
            at = child['next_offset']
    return result

def lighting(b):
    if b[:4] != b'LT3 ' or read(b, 4, 'I')[0] != 1:
        raise ValueError('LT3 prefix')
    count = read(b, 40, 'I')[0]
    if count > 4096:
        raise ValueError('LT3 group count')
    records, groups = ([], [])
    strides = {1: 48, 2: 96, 4: 96, 8: 80, 16: 80, 32: 80, 64: 160}
    for i in range(count):
        p = 48 + i * 48
        n, bits, offset = read(b, p + 32, '3I')
        kind = bits & 255
        if kind not in strides or n > 65536:
            raise ValueError('LT3 unsupported group')
        stride = strides[kind]
        take(b, offset, n * stride)
        groups.append({'offset': p, 'type_bits': bits, 'count': n, 'record_offset': offset, 'maximum': read(b, p, '4f'), 'minimum': read(b, p + 16, '4f')})
        for j in range(n):
            q = offset + j * stride
            r = {'type_bits': bits, 'kind': kind, 'offset': q, 'group': i, 'raw': take(b, q, stride).hex()}
            if kind == 64:
                for key, at in [('max', 0), ('min', 16), ('center', 32), ('extent', 48), ('fade_positive', 64), ('fade_negative', 80), ('quaternion', 96), ('direction', 112)]:
                    r[key] = read(b, q + at, '4f')
                r.update(front=list(take(b, q + 128, 4)), back=list(take(b, q + 132, 4)), rotation=read(b, q + 136, '2f'), flags=read(b, q + 144, 'I')[0])
            elif kind == 32:
                r.update(max=read(b, q, '4f'), min=read(b, q + 16, '4f'), direction=read(b, q + 32, '4f'), color=list(take(b, q + 48, 4)), ambient=list(take(b, q + 52, 4)), force=read(b, q + 56, 'f')[0], flags=read(b, q + 64, 'I')[0])
            elif kind == 1:
                r.update(position=read(b, q, '4f'), color=list(take(b, q + 16, 4)), range=read(b, q + 20, 'f')[0], extended_range=read(b, q + 28, 'f')[0], flags=read(b, q + 32, 'I')[0])
            elif kind in (2, 4):
                r.update(max=read(b, q, '4f'), min=read(b, q + 16, '4f'), position=read(b, q + 32, '4f'), direction=read(b, q + 48, '4f'), color=list(take(b, q + 64, 4)), parameter0=read(b, q + 68, 'f')[0], parameter1=read(b, q + 72, 'f')[0], flags=read(b, q + 80, 'I')[0])
            flag_at = {1: 32, 2: 80, 4: 80, 8: 64, 16: 64, 32: 64, 64: 144}[kind]
            r.update(light_id=read(b, q + flag_at + 4, 'I')[0], light_key=read(b, q + flag_at + 8, 'I')[0])
            records.append(r)
    return {'direction': read(b, 16, '4f'), 'direct': list(take(b, 32, 4)), 'ambient': list(take(b, 36, 4)), 'groups': groups, 'records': records}
