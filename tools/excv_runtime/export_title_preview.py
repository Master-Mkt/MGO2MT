# Generated converter-only source; historical analysis entry points omitted.
"""Export an explicitly partial static LA2 pose for the native D3D11 adapter.
No GCX execution. Event 0xF7C4BC endpoints are applied without a clock or interpolation.
Pixel/fixed-point conversion and color composition remain preview hypotheses pending
full shader/render-command review; do not treat this as game title attainment.
"""
from pathlib import Path
import struct, json, math, collections, copy, hashlib
from title_assets import ROOT, read, take
from la2_inspect import Layout

def state():
    return {'position': [[0, 0]], 'size': [0, 0], 'color': [[255, 255, 255, 128]], 'visible': True, 'scale': 256, 'angle': 0, 'texture': 0, 'uv': None, 'blend': 0, 'text': None}

def text_quads(layout, s, tex, img):
    spacing, alignment, font, pad = read(bytes.fromhex(s['font_raw']), 0, 'h3H')
    if alignment not in [0, 1, 2] or not 1 <= font <= len(layout.fonts):
        raise ValueError('font settings')
    if any((ord(c) < 32 or ord(c) > 126 for c in s['text'])):
        raise ValueError('preview supports static ASCII only')
    bychar = {g['char']: g for g in layout.fonts[font - 1]['glyphs']}
    glyphs = [bychar[c] for c in s['text']]
    width = sum((g['width'] for g in glyphs)) + spacing * max(0, len(glyphs) - 1)
    offset = s['size'][0] - width
    cursor = offset if alignment == 1 else int(offset / 2) if alignment == 2 else 0
    result = []
    px, py = s['position'][0]
    norm = struct.unpack('>f', bytes.fromhex('37800080'))[0]
    f32 = lambda v: struct.unpack('f', struct.pack('f', v))[0]

    def uv_coord(value, extent, origin, dimension):
        fixed = f32(f32(f32(extent * f32(value * norm)) + origin) * 16)
        return math.floor(fixed + 0.5) / 16 / dimension
    for g in glyphs:
        x = px + cursor
        y = py
        w, h = (g['width'], g['height'])
        u0, u1, v0, v1 = g['uv']
        u0, u1 = [uv_coord(u, tex['width'], tex['x'], img['width']) for u in [u0, u1]]
        v0, v1 = [uv_coord(v, tex['height'], tex['y'], img['height']) for v in [v0, v1]]
        result.append(([(x, y), (x + w, y), (x + w, y + h), (x, y + h)], [(u0, v0), (u1, v0), (u1, v1), (u0, v1)]))
        cursor += w + spacing
    return result

def apply(layout, s, props):
    for op in props['commands']:
        code = op['opcode']
        b = bytes.fromhex(op['data'])
        if code in [0]:
            continue
        if code in [1, 2]:
            s['visible'] = code == 1
        elif code == 3:
            n, = read(b, 0, 'I')
            p = 4
            points = []
            for _ in range(n):
                count, mode = read(b, p, 'HH')
                p += 4
                if mode:
                    raise ValueError('nonzero position mode')
                for i in range(count):
                    points.append(list(read(b, p, 'hh')))
                    p += 4
            s['position'] = points
        elif code == 5:
            if read(b, 0, 'I')[0] != 1:
                raise ValueError('size cardinality')
            s['size'] = list(read(b, 4, 'hh'))
        elif code == 6:
            if read(b, 0, 'I')[0] != 1:
                raise ValueError('color cardinality')
            mode, index = read(b, 4, 'HH')
            if mode or index >> 12 != 15:
                raise ValueError('color partial mask')
            pool = layout.colors[index & 4095]
            raw = bytes.fromhex(pool['data'])
            s['color'] = [list(raw[i:i + 4]) for i in range(0, len(raw), 4)]
        elif code == 7:
            s['texture'] = read(b, 0, 'I')[0]
        elif code == 8:
            s['uv'] = read(b, 0, 'I')[0]
        elif code == 9:
            s['blend_raw'] = read(b, 0, 'I')[0]
            s['blend'] = 1 if s['blend_raw'] == 8193 else 0
        elif code == 10:
            s['scale'] = read(b, 2, 'h')[0]
        elif code == 11:
            s['angle'] = read(b, 0, 'h')[0]
        elif code == 13:
            n, = read(b, 0, 'I')
            s['text'] = take(b, 4, n).rstrip(b'\x00').decode('utf-8', errors='replace')
        elif code == 24:
            s['font_raw'] = b.hex()
        else:
            raise ValueError(f'unsupported property {code}')

def export(layout, manifest, final):
    states = {n['id']: state() for n in layout.nodes}
    for n in layout.nodes:
        apply(layout, states[n['id']], layout.properties[n['property']])
    if final:
        event = next((e for e in layout.events if e['name'] == 16237756))
        for track in event['tracks']:
            for cmd in track['commands']:
                b = bytes.fromhex(cmd['data'])
                op = cmd['opcode']
                if not op:
                    continue
                if op not in [7, 8]:
                    raise ValueError('unexpected startup event opcode')
                pointer = read(b, 0 if op == 7 else 4, 'I')[0]
                apply(layout, states[track['node']], layout.properties[pointer])
    return geometry_from_states(layout, manifest, states)

def geometry_from_states(layout, manifest, states, local=False):
    geometry = []
    omitted = []
    transforms = {0: (1, 0, 0, 0, 1, 0)}
    colors = {0: [255, 255, 255, 128]}
    visibility = {0: True}
    bykey = {t['key']: t for t in manifest['txn']['textures']}
    image_list = list(manifest['txn']['images'])
    if 'fallback_txn' in manifest:
        fallback = next((t for t in manifest['fallback_txn']['textures'] if t['key'] == 5842336)).copy()
        fallback['image_index'] = 9
        bykey[fallback['key']] = fallback
        image_list.append(manifest['fallback_txn']['images'][10])
    for n in layout.nodes:
        s = states[n['id']]
        pid = 0 if local else n['parent']
        a, b, x, c, d, y = transforms[pid]
        world = lambda p: (a * p[0] / 16 + b * p[1] / 16 + x, c * p[0] / 16 + d * p[1] / 16 + y)
        visible = visibility[pid] and s['visible']
        visibility[n['id']] = visible
        color = [[min(255, round(v * colors[pid][k] / (128 if k == 3 else 255))) for k, v in enumerate(col)] for col in s['color']]
        colors[n['id']] = color[0]
        transforms[n['id']] = transforms[pid]
        if n['type'] == 0:
            px, py = world(s['position'][0])
            angle = s['angle'] * 2 * math.pi / 4096
            scale = s['scale'] / 256
            co = math.cos(angle) * scale
            si = math.sin(angle) * scale
            transforms[n['id']] = (a * co + b * si, -a * si + b * co, px, c * co + d * si, -c * si + d * co, py)
            continue
        if not visible and (not local):
            continue
        if n['type'] == 10:
            ref = layout.textures[s['texture'] - 1]
            tex = bykey[ref['texture']]
            atlas = tex['image_index']
            img = image_list[atlas]
            for points, uv in text_quads(layout, s, tex, img):
                geometry.append({'node': n['id'], 'atlas': atlas, 'blend': s['blend'], 'points': [world(p) for p in points], 'uv': uv, 'color': color * 4})
            continue
        if n['type'] not in [2, 3, 4, 6]:
            omitted.append({'node': n['id'], 'type': n['type'], 'text': s['text'], 'reason': 'text or special primitive contract not implemented'})
            continue
        if n['type'] in [4, 6]:
            if len(s['position']) != 4:
                raise ValueError('quad vertices')
            points = [world(p) for p in s['position']]
        else:
            px, py = s['position'][0]
            w, h = s['size']
            points = [world(p) for p in [(px, py), (px + w, py), (px + w, py + h), (px, py + h)]]
        atlas = -1
        uv = [(0, 0), (1, 0), (1, 1), (0, 1)]
        if s['texture']:
            ref = layout.textures[s['texture'] - 1]
            tex = bykey.get(ref['texture'])
            if not tex:
                omitted.append({'node': n['id'], 'reason': 'unresolved texture', 'key': hex(ref['texture'])})
                continue
            atlas = tex['image_index']
            img = manifest['txn']['images'][atlas]
            if s['uv'] is None:
                omitted.append({'node': n['id'], 'reason': 'UV implicit behavior unresolved'})
                continue
            row = layout.uv[s['uv']]
            raw = bytes.fromhex(row['data'])
            coords = [read(raw, i * 4, 'HH') for i in range(row['count'])]
            if len(coords) == 2:
                uvpix = [coords[0], (coords[1][0], coords[0][1]), coords[1], (coords[0][0], coords[1][1])]
            elif len(coords) == 4:
                uvpix = coords
            else:
                raise ValueError('UV vertices')
            uv = [((u + tex['x']) / img['width'], (v + tex['y']) / img['height']) for u, v in uvpix]
        cols = color * 4 if len(color) == 1 else color
        if len(cols) != 4:
            raise ValueError('color vertices')
        geometry.append({'node': n['id'], 'atlas': atlas, 'blend': s['blend'], 'points': points, 'uv': uv, 'color': cols})
    return (geometry, omitted)
