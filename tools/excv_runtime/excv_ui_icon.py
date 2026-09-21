# Generated converter-only source; historical analysis entry points omitted.
"""Pure original icon pose adapter; no source discovery or import-time I/O.
Copied from the reviewed local extractors without their workspace entry points.
"""
import struct, io, hashlib
from PIL import Image
from title_assets import read, take
from la2_inspect import Layout
from export_title_preview import state, apply, geometry_from_states
sha = lambda b: hashlib.sha256(b).hexdigest()

def integer(value):
    v = round(value)
    if abs(v - value) > 1e-05:
        raise ValueError('Unreviewed fractional icon extent')
    return v

def rectangle(points):
    if len(points) != 4:
        raise ValueError('Expected one rectangular original quad')
    x0, y0 = map(integer, points[0])
    x1, y1 = map(integer, points[2])
    if points[1] != [x1, y0] and tuple(points[1]) != (x1, y0):
        raise ValueError('Rotated/sheared original quad is unreviewed')
    if points[3] != [x0, y1] and tuple(points[3]) != (x0, y1):
        raise ValueError('Rotated/sheared original quad is unreviewed')
    if not 0 < x1 - x0 <= 1024 or not 0 < y1 - y0 <= 1024:
        raise ValueError('Invalid icon extent')
    return (x0, y0, x1, y1)

def priority_remaps(data):
    """DCI entries retain their priority, including duplicate archive keys.

    resident_nodld has (tri_layout_icon, 3) and (tri_layout_icon, 0).
    The original records, entry flags, old/new indices and extents agree;
    archive-only dictionaries silently discard the priority-3 image map.
    """
    key, block, count, pad = read(data, 0, '4I')
    if not key or block != 4096 or pad or (count > 4096):
        raise ValueError('DCI header')
    take(data, 16, count * 12)
    result = {}
    for entry in range(count):
        key, offset, flags = read(data, 16 + entry * 12, '3I')
        length, priority = (flags & 65535, flags >> 16)
        if not length:
            continue
        if priority not in (0, 3) or offset < 16 + count * 12:
            raise ValueError('Unreviewed DCI priority or overlapping map')
        take(data, offset, length * 4)
        identity = (key, priority)
        if identity in result:
            raise ValueError('Duplicate DCI archive/priority')
        mapping = {}
        for i in range(length):
            old, new = read(data, offset + i * 4, '2H')
            if old in mapping:
                raise ValueError('Duplicate DCI source index')
            mapping[old] = new
        result[identity] = {'entry': entry, 'offset': offset, 'flags': flags, 'priority': priority, 'mapping': mapping}
    return result

def texture_png(raw, image, payload):
    width, height = (image['width'], image['height'])
    codec = image['codec']
    block = {9: 8, 11: 16}[codec]
    size = (width + 3) // 4 * ((height + 3) // 4) * block
    if len(payload) != size:
        raise ValueError('Top mip extent')
    header = bytearray(128)
    header[:4] = b'DDS '
    struct.pack_into('<7I', header, 4, 124, 528391, height, width, size, 0, 1)
    struct.pack_into('<2I4s', header, 76, 32, 4, b'DXT1' if codec == 9 else b'DXT5')
    struct.pack_into('<I', header, 108, 4096)
    return Image.open(io.BytesIO(bytes(header) + payload)).convert('RGBA')

def render(sources, layout_hash):
    path, entry, raw = sources.layouts[layout_hash]
    layout = Layout(raw)
    states = {n['id']: state() for n in layout.nodes}
    for node in layout.nodes:
        apply(layout, states[node['id']], layout.properties[node['property']])
    base_event = next((e for e in layout.events if e['name'] == 6562711), None)
    if base_event:
        for track in base_event['tracks']:
            for cmd in track['commands']:
                if not cmd['opcode']:
                    continue
                if cmd['opcode'] != 7:
                    raise ValueError('Unreviewed timed default icon event')
                ptr = read(bytes.fromhex(cmd['data']), 0, 'I')[0]
                apply(layout, states[track['node']], layout.properties[ptr])
    decoded, textures, images, texture_evidence = ([], [], [], [])
    for ref in layout.textures:
        image, proof = sources.resolve(ref['texture'])
        tex = dict(proof['texture'], image_index=len(images))
        decoded.append(image)
        textures.append(tex)
        images.append(proof['image'])
        texture_evidence.append(proof)
    quads, omitted = geometry_from_states(layout, {'txn': {'textures': textures, 'images': images}}, states)
    if omitted or not quads:
        raise ValueError(f'Incomplete icon layout: {omitted}')
    raster_mode = 'source-resolution, original integer quad assembly'
    original_points = [q['points'] for q in quads]
    if len(quads) == 1:
        q = quads[0]
        if q['atlas'] < 0 or q['blend']:
            raise ValueError('Unreviewed standalone icon primitive')
        image = decoded[q['atlas']]
        crop = rectangle([(u * image.width, v * image.height) for u, v in q['uv']])
        p = q['points']
        x0, y0 = p[0]
        x1, y1 = p[2]
        if tuple(p[1]) != (x1, y0) or tuple(p[3]) != (x0, y1) or x1 <= x0 or (y1 <= y0):
            raise ValueError('Unreviewed standalone icon transform')
        sx, sy = ((x1 - x0) / (crop[2] - crop[0]), (y1 - y0) / (crop[3] - crop[1]))
        if abs(sx - sy) < 1e-05 and abs(sx - 1) > 1e-05:
            rects = [(0, 0, crop[2] - crop[0], crop[3] - crop[1])]
            raster_mode = 'uniform LA2 scale normalized; original source RGBA retained'
        else:
            rects = [rectangle(p)]
            if abs(sx - 1) > 1e-05 or abs(sy - 1) > 1e-05:
                raster_mode = 'explicit nonuniform LA2 extent baked with bilinear sampling'
    else:
        rects = [rectangle(q['points']) for q in quads]
    left, top = (min((r[0] for r in rects)), min((r[1] for r in rects)))
    right, bottom = (max((r[2] for r in rects)), max((r[3] for r in rects)))
    if not 0 < right - left <= 512 or not 0 < bottom - top <= 512:
        raise ValueError('Icon output boundary')
    result = Image.new('RGBA', (right - left, bottom - top), (0, 0, 0, 0))
    evidence = []
    for q, rect in zip(quads, rects):
        if q['atlas'] < 0 or q['blend']:
            raise ValueError('Unreviewed untextured/additive primitive')
        image = decoded[q['atlas']]
        uv = [(u * image.width, v * image.height) for u, v in q['uv']]
        crop = rectangle(uv)
        if min(crop) < 0 or crop[2] > image.width or crop[3] > image.height:
            raise ValueError('Out-of-atlas crop')
        pic = image.crop(crop)
        source_crop_sha = sha(pic.tobytes())
        output_extent = (rect[2] - rect[0], rect[3] - rect[1])
        if pic.size != output_extent:
            if len(quads) != 1 or not raster_mode.startswith('explicit nonuniform'):
                raise ValueError('Unreviewed multipart resampling')
            pic = pic.resize(output_extent, Image.Resampling.BILINEAR)
        color = q['color'][0]
        if any((c != color for c in q['color'])):
            raise ValueError('Unreviewed gradient tint')
        if color != [255, 255, 255, 128]:
            channels = pic.split()
            pic = Image.merge('RGBA', tuple((c.point(lambda v, k=k: min(255, round(v * color[k] / (128 if k == 3 else 255)))) for k, c in enumerate(channels))))
        result.alpha_composite(pic, (rect[0] - left, rect[1] - top))
        evidence.append({'node': q['node'], 'source_texture': layout.textures[q['atlas']], 'source_crop_xyxy': crop, 'destination_xyxy': rect, 'color': color, 'source_crop_rgba_sha256': source_crop_sha, 'crop_rgba_sha256': sha(pic.tobytes()), 'source': texture_evidence[q['atlas']]})
    if not result.getchannel('A').getbbox():
        raise ValueError('Empty original icon')
    return (result, {'layout_source': sources.use(path), 'layout_entry': entry, 'layout_sha256': sha(raw), 'base_event': base_event['name'] if base_event else None, 'pose': 'default with no attachments', 'bounds_xyxy': [left, top, right, bottom], 'original_quad_points': original_points, 'raster_mode': raster_mode, 'quads': evidence, 'rgba_sha256': sha(result.tobytes())})

def source_icon(sources, layout_hash):
    path, entry, raw = sources.layouts[layout_hash]
    layout = Layout(raw)
    states = {n['id']: state() for n in layout.nodes}
    for node in layout.nodes:
        apply(layout, states[node['id']], layout.properties[node['property']])
    base = next((e for e in layout.events if e['name'] == 6562711), None)
    if base:
        for track in base['tracks']:
            for cmd in track['commands']:
                if not cmd['opcode']:
                    continue
                if cmd['opcode'] != 7:
                    raise ValueError('Unreviewed timed base event')
                ptr = read(bytes.fromhex(cmd['data']), 0, 'I')[0]
                apply(layout, states[track['node']], layout.properties[ptr])
    decoded = []
    textures = []
    images = []
    evidence = []
    for ref in layout.textures:
        image, proof = sources.resolve(ref['texture'])
        decoded.append(image)
        textures.append(dict(proof['texture'], image_index=len(images)))
        images.append(proof['image'])
        evidence.append(proof)
    quads, omitted = geometry_from_states(layout, {'txn': {'textures': textures, 'images': images}}, states)
    if omitted or len(quads) != 1:
        raise ValueError('Equipment default must have one complete visible quad')
    quad = quads[0]
    if quad['atlas'] < 0 or quad['blend'] or any((c != [255, 255, 255, 128] for c in quad['color'])):
        raise ValueError('Unreviewed primitive/blend/default tint')
    image = decoded[quad['atlas']]
    crop = rectangle([(u * image.width, v * image.height) for u, v in quad['uv']])
    if min(crop) < 0 or crop[2] > image.width or crop[3] > image.height:
        raise ValueError('Crop out of bounds')
    points = quad['points']
    x0, y0 = points[0]
    x1, y1 = points[2]
    if tuple(points[1]) != (x1, y0) or tuple(points[3]) != (x0, y1) or x1 <= x0 or (y1 <= y0):
        raise ValueError('Unreviewed source transform')
    result = image.crop(crop)
    if not result.getchannel('A').getbbox():
        raise ValueError('Empty original icon')
    proof = {'layout_source': sources.use(path), 'layout_entry': entry, 'layout_sha256': sha(raw), 'base_event': base['name'] if base else None, 'source_texture': layout.textures[quad['atlas']], 'source': evidence[quad['atlas']], 'crop_xyxy': crop, 'original_quad': quad, 'display_extent': [x1 - x0, y1 - y0], 'source_extent': list(result.size), 'display_scale': [(x1 - x0) / result.width, (y1 - y0) / result.height], 'rgba_sha256': sha(result.tobytes()), 'raster_mode': 'exact source crop; no resize or tint', 'events': layout.events}
    return (result, raw, proof)
