# Generated converter-only source; historical analysis entry points omitted.
"""Explicit local Gekko research candidate; preserves original bind coordinates/skin.
Uses only the user-selected MDN and reviewed DDS catalog. Does not run catalog.main().
"""
from pathlib import Path
import argparse, hashlib, json, math, struct, unittest
from convert_character_catalog import geometry, bone_rows
from convert_character_model import header

def record(p):
    b = p.read_bytes()
    return dict(path=str(p.resolve()), size=len(b), sha256=hashlib.sha256(b).hexdigest())

def dds_top(b):
    if len(b) < 128 or b[:4] != b'DDS ' or struct.unpack_from('<I', b, 4)[0] != 124:
        raise ValueError('DDS header')
    h, w = struct.unpack_from('<2I', b, 12)
    code = {b'DXT1': 9, b'DXT5': 11}.get(bytes(b[84:88]))
    if not code or not 0 < w <= 8192 or (not 0 < h <= 8192):
        raise ValueError('DDS format/size')
    if struct.unpack_from('<I', b, 112)[0] & 65024:
        raise ValueError('DDS cube/volume unsupported')
    n = (w + 3) // 4 * ((h + 3) // 4) * (8 if code == 9 else 16)
    if len(b) < 128 + n:
        raise ValueError('DDS mip extent')
    return (w, h, code, b[128:128 + n])

def initial_group_visibility(raw, h):
    """Current 117DA0/1182F8..118400: nonzero source flags hide descendants.

    113118 ORs 0xff to hide a group; 113560 clears those bits to show it.
    Freeze the original *initial* visibility. Runtime damage/line variants are
    not simulated by this local asset converter.
    """
    if not 0 < h[3] <= 64 or h[11] < 128 or h[11] + h[3] * 16 > len(raw):
        raise ValueError('MDN group extent')
    rows = []
    visible = []
    keys = set()
    for i in range(h[3]):
        key, flags, parent, pad = struct.unpack_from('>4I', raw, h[11] + 16 * i)
        if key in keys:
            raise ValueError('MDN duplicate group ID')
        keys.add(key)
        if parent == 4294967295:
            parent = -1
        if not -1 <= parent < i:
            raise ValueError('MDN group parent')
        own_hidden = bool(flags) or key == 0
        shown = not own_hidden and (parent < 0 or visible[parent])
        visible.append(shown)
        rows.append(dict(index=i, id=key, source_flags=flags, parent=parent, own_hidden=own_hidden, initial_visible=shown, unused_word=hex(pad)))
    return (visible, rows)

def visible_parts(raw, h, parts, indices):
    shown, groups = initial_group_visibility(raw, h)
    selected = []
    new_indices = []
    meshes = []
    part = 0
    for mi in range(h[4]):
        m = struct.unpack_from('>8I', raw, h[12] + 80 * mi)
        if m[0] >= len(shown):
            raise ValueError('MDN mesh group')
        row = dict(index=mi, group=m[0], flags=m[1], visible=shown[m[0]], source_parts=[])
        for _ in range(m[2]):
            if part >= len(parts):
                raise ValueError('MDN part extent')
            first, count, tex, pattern, shader = parts[part]
            row['source_parts'].append(part)
            if first < 0 or count <= 0 or count % 3 or (first + count > len(indices)):
                raise ValueError('MDN draw range')
            if shown[m[0]]:
                selected.append((len(new_indices), count, tex, pattern, shader))
                new_indices.extend(indices[first:first + count])
            part += 1
        meshes.append(row)
    if part != len(parts) or not selected:
        raise ValueError('MDN visible parts')
    return (selected, new_indices, groups, meshes)

def build(mdn, texture_dir, out, all_groups=False):
    raw = mdn.read_bytes()
    h = header(raw)
    bones = bone_rows(raw, h)
    if len({b[0] for b in bones}) != len(bones) or any((not -1 <= b[1] < i for i, b in enumerate(bones))):
        raise ValueError('bone graph')
    vertices, indices, parts, keys, evidence = geometry(mdn, bones)
    raw_part_count = len(parts)
    raw_triangle_count = len(indices) // 3
    selected, selected_indices, group_rows, mesh_visibility = visible_parts(raw, h, parts, indices)
    if not all_groups:
        parts, indices = (selected, selected_indices)
    if evidence['ancestor_bone_fallback'] or any(evidence['translation']):
        raise ValueError('unexpected bone remap')
    if not all((math.isfinite(x) for row in vertices for x in row)):
        raise ValueError('geometry finite')
    material_rows = []
    for index in range(h[7]):
        words = struct.unpack_from('>12I', raw, h[15] + 112 * index)
        bindings = []
        for texture_index in words[4:4 + words[2]]:
            if texture_index >= h[8]:
                raise ValueError('material texture index')
            key, flags, *uv = struct.unpack_from('>2I4f', raw, h[16] + 32 * texture_index)
            bindings.append(dict(index=texture_index, key=key, flags=flags, uv_transform=uv))
        if bindings and bindings[0]['uv_transform'] != [1.0, 1.0, 0.0, 0.0]:
            raise ValueError('first texture UV transform unsupported')
        material_rows.append(dict(index=index, shader=words[0], words=list(words), bindings=bindings))
    manifest_path = texture_dir / 'texture_manifest.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    selected = []
    images = {}
    for key in sorted(keys):
        path = texture_dir / f'{key:06x}.dds'
        r = record(path)
        matches = [v for v in manifest['textures'] if v['texture_key'] == key and v.get('dds', {}).get('sha256') == r['sha256']]
        if not matches:
            raise ValueError('DDS absent from reviewed manifest ' + path.name)
        images[key] = dds_top(path.read_bytes())
        selected.append(dict(key=key, dds=r, provenance=matches))
    bounds = [min((v[a] for v in vertices)) for a in range(3)] + [max((v[a] for v in vertices)) for a in range(3)]
    key_index = {k: i for i, k in enumerate(sorted(images))}
    gwm = bytearray(b'GWM1' + struct.pack('<5I6f', 2, len(vertices), len(indices), len(parts), len(images), *bounds))
    for v in vertices:
        gwm += struct.pack('<12f', *v[:8], 0, 0, 0, 1)
    gwm += struct.pack('<' + str(len(indices)) + 'I', *indices)
    for first, count, texture, pattern, shader in parts:
        if pattern:
            raise ValueError('unexpected pattern')
        gwm += struct.pack('<5I3f', first, count, key_index[texture], 0, shader, 1, 1, 1)
    for _, (w, hh, c, payload) in sorted(images.items()):
        gwm += struct.pack('<4I', w, hh, c, len(payload)) + payload
    gwc = bytearray(b'GWC1' + struct.pack('<7I', 3, 1, len(images), 1, 1, 60, 0))
    for _ in range(2):
        gwc += struct.pack('<I', len(bones))
        for key, parent, xyz in bones:
            gwc += struct.pack('<Ii3f8f', key, parent, *xyz, 0, 0, 0, 1, 0, 0, 0, 1)
    gwc += struct.pack('<6f', 0, 0, 0, 0, 0, 0)
    gwc += struct.pack('<5I', 0, h[1], len(vertices), len(indices), len(parts))
    for v in vertices:
        gwc += struct.pack('<10f4H16f', *v)
    gwc += struct.pack('<' + str(len(indices)) + 'I', *indices)
    for p in parts:
        gwc += struct.pack('<5I', *p)
    gwc += struct.pack('<12II3f', 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1)
    for k, (w, hh, c, payload) in sorted(images.items()):
        gwc += struct.pack('<5I', k, w, hh, c, len(payload)) + payload
    out.mkdir(parents=True, exist_ok=True)
    (out / 'gekko.gwm').write_bytes(gwm)
    (out / 'gekko.gwc').write_bytes(gwc)
    report = dict(schema=1, source=record(mdn), texture_manifest=record(manifest_path), converter=record(Path(__file__)), outputs=[record(out / 'gekko.gwm'), record(out / 'gekko.gwc')], vertices=len(vertices), triangles=len(indices) // 3, parts=len(parts), textures=len(images), original_bind_bounds=bounds, native_bind_floor_offset=[0, -bounds[1], 0], bone_count=len(bones), bones=[dict(index=i, id=k, parent=p, world_bind_position=xyz) for i, (k, p, xyz) in enumerate(bones)], material_parts=[dict(index=i, first=p[0], count=p[1], texture=p[2], shader=p[4]) for i, p in enumerate(parts)], initial_visibility=dict(applied=not all_groups, groups=group_rows, meshes=mesh_visibility, raw_parts=raw_part_count, raw_triangles=raw_triangle_count, visible_meshes=[m['index'] for m in mesh_visibility if m['visible']], hidden_meshes=[m['index'] for m in mesh_visibility if not m['visible']], policy='Freeze original initial group mask; no runtime damage or line-type selection'), original_materials=material_rows, original_meshes=[dict(index=i, words=struct.unpack_from('>8I', raw, h[12] + 80 * i)) for i in range(h[4])], texture_sources=selected, original_actor_transform=None, source_unchanged=record(mdn)['sha256'] == hashlib.sha256(raw).hexdigest(), limitations=['Static identity bind preview; motion handled separately', 'Original scale and root preserved; floor offset is native display policy', 'All vertices and bones retained; initial group visibility applied unless diagnostic all_groups selected; dynamic damage/line-type/LOD changes not reconstructed', 'Diffuse first binding only, normals rebuilt from authored triangles; specular/normal/alpha shader parity pending', 'Dedicated GWC gender0 rule id0 kind100 is storage adapter; other appearance slots unused/missing; never normal account equipment', '59-bone second GWC rig table is duplicate required by format, not female Gekko'], source_db_saved=False, original_patch_used=False)
    (out / 'model.json').write_text(json.dumps(report, indent=2, ensure_ascii=False, allow_nan=False), encoding='utf-8')
    return report

class Tests(unittest.TestCase):

    def sample(self):
        b = bytearray(136)
        b[:4] = b'DDS '
        struct.pack_into('<I', b, 4, 124)
        struct.pack_into('<2I', b, 12, 4, 4)
        b[84:88] = b'DXT1'
        return b

    def test_top(self):
        self.assertEqual(dds_top(self.sample())[:3], (4, 4, 9))

    def test_bad(self):
        good = self.sample()
        bad = [good[:127], good[:-1]]
        for off, val in [(0, 0), (4, 0), (12, 0), (16, 8193), (84, 0), (112, 512)]:
            b = bytearray(good)
            struct.pack_into('<I', b, off, val)
            bad.append(b)
        for b in bad:
            with self.assertRaises(ValueError):
                dds_top(b)

    def groups(self, rows):
        b = bytearray(128) + b''.join((struct.pack('>4I', *r) for r in rows))
        h = [0] * 24
        h[3] = len(rows)
        h[11] = 128
        return (b, h)

    def test_group_hierarchy(self):
        b, h = self.groups([(1, 0, 4294967295, 0), (2, 15, 0, 0), (3, 0, 1, 0), (4, 0, 0, 0), (0, 0, 0, 0)])
        self.assertEqual(initial_group_visibility(b, h)[0], [True, False, False, True, False])

    def test_group_bad(self):
        for rows in [[(1, 0, 0, 0)], [(1, 0, 3, 0)], [(1, 0, 4294967295, 0), (2, 0, 2, 0)], [(1, 0, 4294967295, 0), (1, 0, 0, 0)]]:
            with self.assertRaises(ValueError):
                initial_group_visibility(*self.groups(rows))
        b, h = self.groups([(1, 0, 4294967295, 0)])
        with self.assertRaises(ValueError):
            initial_group_visibility(b[:-1], h)
        h[3] = 65
        with self.assertRaises(ValueError):
            initial_group_visibility(b, h)

    def test_part_selection_and_extent(self):
        b, h = self.groups([(1, 0, 4294967295, 0), (2, 15, 0, 0)])
        h[4] = 2
        h[12] = len(b)
        b += struct.pack('>8I12f', 0, 0, 1, 0, 0, 0, 3, 0, *[0.0] * 12)
        b += struct.pack('>8I12f', 1, 0, 1, 1, 0, 0, 3, 0, *[0.0] * 12)
        parts = [(0, 3, 11, 0, 80), (3, 3, 12, 0, 80)]
        selected, ids, _, mesh = visible_parts(b, h, parts, [0, 1, 2, 3, 4, 5])
        self.assertEqual(selected, [parts[0]])
        self.assertEqual(ids, [0, 1, 2])
        self.assertFalse(mesh[1]['visible'])
        for broken in [[(-1, 3, 11, 0, 80), parts[1]], [(0, 4, 11, 0, 80), parts[1]], [(0, 9, 11, 0, 80), parts[1]], parts[:1]]:
            with self.assertRaises(ValueError):
                visible_parts(b, h, broken, [0, 1, 2, 3, 4, 5])
