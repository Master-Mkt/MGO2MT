# Generated converter-only source; historical analysis entry points omitted.
"""Rebuild reviewed stage runtime companions from original extracted sources.

No installed FULL resources are used as inputs. Source/scene/ELF paths are
explicit. Recipes cover the five current maps, not automatic game activation.
"""
from pathlib import Path
import argparse, hashlib, json, math, struct, tempfile
from archive_inventory import dar
from title_assets import read, take
from excv_core import atomic, save_json, file_record, read_package, local_root, validate_output
from stage_scene import scripts, geometry, property_children
from gcx_inspect import walk
from convert_character_model import header
from elf_image import ElfImage
PROFILES = {'n022a': (20, 70, 71), 'n001a': (1, 70, 71), 'n004a': (4, 69, 70), 'n007a': (7, 66, 67), 'n023a': (21, 70, 71)}
SKIES = {'n022a': ('s01_sky', 107, 107), 'n001a': ('n001a_skyE', 112, 110), 'n023a': ('s02a_skyE', 123, 121), 'n007a': ('n007a_sky', 101, 100)}
ELF_SHA = '1a55a41ee5afdebd89075bd205e412c8311f569ba9dc022f629bb63fb4bfd13a'
sha = lambda b: hashlib.sha256(b).hexdigest()
f32 = lambda x: struct.unpack('<f', struct.pack('<f', x))[0]
line = lambda v: ' '.join((format(x, '.9g') if isinstance(x, float) else str(x) for x in v)) + '\n'

def require(value, reason):
    if not value:
        raise ValueError(reason)

def supplemental_qq_images(root, destination):
    """Reviewed door/item diffuse hashes from original n012a, unchanged BC."""
    from title_assets import qar, txn, dlz, dld
    keys = {12074203, 374972, 10023981, 15796352, 16431439, 6841419}
    folder = root / 'stage/n012a'
    sources = []

    def source(name):
        p = folder / name
        before = file_record(p)
        raw, mode, _ = read_package(p, 'n012a')
        require(before == file_record(p), 'Original supplemental image changed')
        sources.append(dict(source=before, mode=mode, decoded_sha256=sha(raw)))
        return raw
    raw = source('cache.qar')
    refs = {}
    for e in qar(raw):
        if not e['name'].endswith('.txn'):
            continue
        b = take(raw, e['offset'], e['size'])
        t = txn(b)
        for x in t['textures']:
            if x['key'] not in keys:
                continue
            require(x['key'] not in refs, 'Supplemental texture ambiguity')
            im = t['images'][x['image_index']]
            require((x['width'], x['height']) == (im['width'], im['height']) and (not x['x']) and (not x['y']) and (x['uv_scale'] == [1, 1]) and (x['uv_offset'] == [0, 0]) and (im['codec'] in (9, 11)), 'Supplemental image layout')
            refs[x['key']] = (x, im, e)
    require(set(refs) == keys, 'Missing supplemental texture metadata')
    found = {k: [] for k in keys}
    for name in ('cache.dlz', 'cache_d.dlz', 'cache_nodld.dlz'):
        b, _ = dlz(source(name))
        for r in dld(b):
            if r['parent_size']:
                continue
            for k, (x, im, e) in refs.items():
                size = (im['width'] + 3) // 4 * ((im['height'] + 3) // 4) * (8 if im['codec'] == 9 else 16)
                if r['key'] == x['archive'] and r['index'] == x['index'] and (r['size'] >= size):
                    found[k].append((r['priority'], take(b, r['offset'] + 32, size), name, r))
    outputs = {}
    proof = []
    for k, matches in found.items():
        require(matches, 'Missing supplemental image payload')
        priority, b, name, r = min(matches, key=lambda v: v[0])
        require(all((pr != priority or p == b for pr, p, _, _ in matches)), 'Ambiguous supplemental image payload')
        x, im, e = refs[k]
        path = destination / f'{k:06x}.bin'
        atomic(path, struct.pack('<4I', im['width'], im['height'], im['codec'], len(b)) + b)
        outputs[k] = path
        proof.append(dict(key=k, txn=e, texture=x, image=im, dld=name, record=r, payload_sha256=sha(b), output=file_record(path)))
    save_json(destination / 'supplemental.json', dict(sources=sources, images=proof, policy='Explicit native dependency using exact original diffuse hash; no source image editing.'))
    return outputs

def pickups(stage, source, destination, qq_source):
    from compile_stage_pickups import PROFILES as VERIFIED, normalized, main_call_prefix
    mid, pid, fid = PROFILES[stage]
    gcx = (source / 'scenerio.gcx').read_bytes()
    geom = (source / (stage + '.geom')).read_bytes()
    expected = next((r for r in VERIFIED if r[1] == stage))
    require((sha(gcx), sha(geom)) == expected[-2:], 'Unreviewed pickup GCX/GEOM revision')
    procs, errors = scripts(gcx, {pid, fid})
    baseline, baseline_errors = scripts(qq_source, {70, 71})
    require(not errors and (not baseline_errors) and (normalized(procs, fid) == normalized(baseline, 71)), 'Unreviewed pickup factory structure')
    main_call = main_call_prefix(gcx, pid)
    caller = next((p for p in procs if p['procedure'] == pid))
    nodes = [n for top in caller['nodes'] for n in walk(top)]
    randoms = [n for n in nodes if n.get('kind') == 'command' and n.get('code') == '0x82bc9' and (n['arguments'][0].get('value') == 3838500)]
    require(len(randoms) == 2, 'Pickup random draws')
    geo = geometry(geom, allow_stale_size=stage == 'n023a')
    text = [f'MGO2MT.GCX_ROUND_ITEMS 1 {mid} verified', 'groups 4']
    groups = []
    for rule in (0, 1):
        for index, (world, item) in enumerate(((140, 22), (113, 10))):
            calls = [n for n in nodes if n.get('kind') == 'proc_call' and n.get('procedure_id') == fid and (n['children'][1].get('value') == world)]
            require(len(calls) == 4, 'Pickup candidate count')
            group = int(randoms[index]['offset'], 16)
            anchors = []
            text.append(f'group {rule} {group} {world} equipment {item} 4')
            for call in calls:
                key = call['children'][0]['value']
                found = [p for p in geo['properties'] if p['hash'] == key]
                require(len(found) == 1 and 'position' in found[0], 'Pickup anchor ambiguity')
                p = found[0]
                x, y, z = p['position'][:3]
                text.append(f"anchor {p['offset']} {key} {x:.9g} {y + 250:.9g} {z:.9g} 0")
                anchors.append(dict(property=p['offset'], hash=key, gcx_call=call['offset']))
            groups.append(dict(rule=rule, world=world, item=item, anchors=anchors))
    atomic(destination / (stage + '.gcx-items.cfg'), ('\n'.join(text) + '\n').encode())
    return dict(main_call=main_call, groups=groups, original_vertical_bias=250)

def water(stage, source, destination):
    from prepare_stage_water import extract, extract_surfaces
    from extract_authored_water_surface import SELECTIONS, decode_mesh, make_gws
    path = source / (stage + '.geom')
    report = extract(path)
    data = b'GWW1' + struct.pack('<II', 1, len(report['fields']))
    for field in report['fields']:
        data += struct.pack('<6f', *field['center'], *field['half_size'])
    atomic(destination / (stage + '.gww'), data)
    surfaces = extract_surfaces(path)
    if surfaces['triangles']:
        data = b'GWS1' + struct.pack('<II', 1, len(surfaces['triangles']))
        for triangle in surfaces['triangles']:
            for v in triangle['vertices']:
                data += struct.pack('<3f', *v)
        atomic(destination / (stage + '_surface.gws'), data)
    authored = None
    if stage in SELECTIONS:
        name, expected, shader = SELECTIONS[stage]
        raw = (source / 'cache.dar').read_bytes()
        entry = next((e for e in dar(raw)[0] if e['name'] == name))
        blob = take(raw, int(entry['offset'], 0), entry['size'])
        require(sha(blob) == expected, 'Authored water MDN revision')
        authored = decode_mesh(blob, shader)
        atomic(destination / (stage + '_render.gws'), make_gws(authored))
    return dict(volumes=report, surfaces=surfaces, authored=authored)

def sky(stage, source, destination, contexts):
    if stage not in SKIES:
        return dict(status='no_reviewed_NewSky_registration')
    from convert_stage_model import main as convert
    name, pid, lpid = SKIES[stage]
    procs, _ = scripts((source / 'scenerio.gcx').read_bytes(), {pid, lpid})
    commands = []
    fog_commands = []
    for p in procs:
        for top in p['nodes']:
            for n in walk(top):
                if n.get('kind') != 'command':
                    continue
                if p['procedure'] == pid and any((a.get('value') == 13511972 for a in n.get('arguments', []))):
                    commands.append(n)
                opts = {o['code']: [v.get('value') for v in o['children']] for o in n.get('children', []) if o.get('kind') == 'option'}
                if p['procedure'] == lpid and '0x1d542' in opts and any((a.get('value') in (14543124, 6729881) for a in n.get('arguments', []))):
                    fog_commands.append((n, opts))
    require(len(commands) == len(fog_commands) == 1, 'Reviewed sky/fog registration changed')
    command = commands[0]
    opts = {o['code']: [v['value'] for v in o['children']] for o in command['children'] if o['kind'] == 'option'}
    raw = (source / 'cache.dar').read_bytes()
    entry = next((e for e in dar(raw)[0] if e['name'] == name + '.mdn'))
    mdn = take(raw, int(entry['offset'], 0), entry['size'])
    h = header(mdn)
    require(int(entry['stem_hash24'], 16) == opts['0x91d13'][0], 'Sky GCX model hash')
    output = destination / (stage + '.sky.gwm')
    report = destination / 'audit' / (stage + '.sky.model.json')
    convert(selected=[name], output=output, report=report, stage_folder=source, texture_context_folders=contexts)
    converted = json.loads(report.read_text(encoding='utf-8'))
    require(not converted['missing_textures'] and (not converted['unsupported_meshes']), 'Incomplete sky conversion')
    uv2 = []
    if any((m['original_key'] == 1048679 for m in converted['original_materials'])):
        for mi in range(h[4]):
            m = read(mdn, h[12] + mi * 80, '8I12f')
            vd = h[14] + 48 * m[4]
            _, n, stride, vo = read(mdn, vd, '4I')
            sem = {mdn[vd + 16 + i] & 15: (mdn[vd + 16 + i] >> 4, mdn[vd + 32 + i]) for i in range(n)}
            require(sem.get(10, (None,))[0] == 7, 'Sky UV2 declaration')
            uv2.extend((read(mdn, h[18] + vo + vi * stride + sem[10][1], '2e') for vi in range(m[6])))
        require(len(uv2) == converted['vertices'], 'Sky UV2 vertex count')
    rgb8 = [int(f32(v * f32(0.255))) & 255 for v in fog_commands[0][1]['0x1d542']]
    fog = [f32(v / 255) for v in rgb8]
    text = 'MGO2MT.SKY 1\n' + f"{opts['0x91d13'][0]} {pid} {sha(mdn)}\n" + line(opts.get('0x1ce53', [0, 0, 0])) + line(opts.get('0x1d654', [0, 0, 0]))
    text += line([opts.get('0x2f5eb3', [0])[0], opts.get('0x7185fe', [1])[0]])
    text += ' '.join(map(str, [f32(v * f32(0.001)) for v in opts['0x222634']] + [f32(opts['0x1a647'][0] * f32(0.001))])) + '\n'
    text += ' '.join(map(str, fog)) + '\n' + f'UV2 {len(uv2)}\n' + ''.join((f'{u} {v}\n' for u, v in uv2))
    atomic(destination / (stage + '.sky.cfg'), text.encode('ascii'))
    return dict(status='rebuilt', procedure=pid, light_procedure=lpid, model=entry, command=command, fog_rgb8=rgb8, limits=['Native elapsed-seconds clock; original normal viewport fog chosen; runtime preset transitions remain separate.'])

def material_companions(stage, source, gwm, conversion, destination, contexts):
    """Rebase packed normals/QQ alpha and floor blend onto each new GWM SHA."""
    from audit_texture_alpha import rgba
    from stage_texture_context import TextureContexts
    data = gwm.read_bytes()
    version, nv, ni, np, nt = struct.unpack_from('<5I', data, 4)
    require(data[:4] == b'GWM1' and version in (2, 3), 'Runtime companion GWM format')
    stride = 48 if version == 2 else 56
    part_offset = 48 + nv * stride + ni * 4
    parts = [struct.unpack_from('<5I', data, part_offset + i * 32) for i in range(np)]
    textures = []
    at = part_offset + np * 32
    for _ in range(nt):
        w, h, c, size = struct.unpack_from('<4I', data, at)
        textures.append((w, h, c, take(data, at + 16, size)))
        at += 16 + size
    report = json.loads(conversion.read_text(encoding='utf-8'))
    archive = (source / 'cache.dar').read_bytes()
    entries = {e['name']: e for e in dar(archive)[0]}
    ctx = TextureContexts(contexts)
    normal_records = []
    alpha_rows = []
    blend_vertices = {}
    blend_parts = []
    blend_images = []
    blend_proof = []
    alpha_cache = {}
    vo = po = 0

    def blend_image(key, name, mdn_sha):
        raw, proof = ctx.resolve(key, name, mdn_sha, strict=True)
        require(raw is not None, 'Unresolved original floor blend image')
        index = blend_images.index(raw) if raw in blend_images else len(blend_images)
        if index == len(blend_images):
            blend_images.append(raw)
            blend_proof.append(proof)
        return index
    for model in report['models']:
        entry = entries[model['name'] + '.mdn']
        md = take(archive, int(entry['offset'], 0), entry['size'])
        require(sha(md) == model['mdn_sha256'], 'Runtime companion MDN identity')
        h = header(md)
        local_part = 0
        for mi in range(h[4]):
            m = read(md, h[12] + mi * 80, '8I12f')
            decl = h[14] + 48 * m[4]
            vd = read(md, decl, '4I')
            sem = {md[decl + 16 + i] & 15: (md[decl + 16 + i] >> 4, md[decl + 32 + i]) for i in range(vd[1])}
            if sem.get(0, (None,))[0] != 1 or sem.get(8, (None,))[0] != 7:
                continue
            for vi in range(m[6]):
                vertex = h[18] + vd[3] + vi * vd[2]
                actual = struct.unpack_from('<8f', data, 48 + (vo + vi) * stride)
                require(actual[:3] == read(md, vertex + sem[0][1], '3f') and actual[6:8] == read(md, vertex + sem[8][1], '2e'), 'Runtime companion vertex order')
                if sem.get(2, (None,))[0] == 10:
                    normal_records.append((vo + vi, read(md, vertex + sem[2][1], 'I')[0]))
            for fi in range(m[3], m[3] + m[2]):
                f = read(md, h[13] + fi * 16, 'HHIIHH')
                mat = read(md, h[15] + f[3] * 112, '12I')
                part = parts[po]
                previous = model['parts'][local_part]
                first, count, tex, flags, key = part
                require(key == mat[0] == previous['material_flags'] and previous['mesh'] == mi and (count == f[1]), 'Runtime companion material order')
                original_ids = read(md, h[20] + f[2], str(f[1]) + 'H')
                ids = struct.unpack_from('<' + str(count) + 'I', data, 48 + nv * stride + first * 4)
                require(ids == tuple((vo + i for i in original_ids)), 'Runtime companion index order')
                if stage == 'n022a' and key == 1179648:
                    if tex not in alpha_cache:
                        alpha_cache[tex] = min(rgba(textures[tex])[3::4]) if textures[tex][2] in (9, 11) else 255
                    minimum = min((struct.unpack_from('<f', data, 48 + i * stride + 44)[0] for i in set(ids)))
                    if alpha_cache[tex] < 255 or minimum < 1:
                        alpha_rows.append((po, first, count, tex, key, 1))
                if stage == 'n022a' and model['name'] == 's01a33a' and (key == 1245187):
                    require(mat[2] == 3, 'Floor blend material slots')
                    refs = [read(md, h[16] + mat[4 + s] * 32, '2I4f2I') for s in range(3)]
                    require(all((r[2:6] == (1.0, 1.0, 0.0, 0.0) for r in refs)), 'Floor blend UV transform')
                    if sem.get(9, (None,))[0] == 7 and sem.get(10, (None,))[0] == 7:
                        normal = blend_image(refs[1][0], model['name'], model['mdn_sha256'])
                        blend = blend_image(refs[2][0], model['name'], model['mdn_sha256'])
                        for index in set(ids):
                            at = h[18] + vd[3] + (index - vo) * vd[2]
                            uv = read(md, at + sem[9][1], '2e') + read(md, at + sem[10][1], '2e')
                            require(index not in blend_vertices or blend_vertices[index] == uv, 'Conflicting floor blend UV')
                            blend_vertices[index] = uv
                        blend_parts.append((po, tex, normal, blend))
                po += 1
                local_part += 1
            vo += m[6]
        require(local_part == len(model['parts']), 'Runtime companion source part count')
    require(vo == nv and po == np, 'Runtime companion vertex/part totals')
    digest = hashlib.sha256(data).digest()
    if normal_records:
        atomic(destination / (gwm.stem + '.gwn'), b'GWN1' + struct.pack('<2I', nv, len(normal_records)) + digest + b''.join((struct.pack('<2I', *v) for v in normal_records)))
    if alpha_rows:
        atomic(destination / (gwm.stem + '.gsa'), b'GSA1' + struct.pack('<3I', 1, np, len(alpha_rows)) + digest + b''.join((struct.pack('<6I', *v) for v in alpha_rows)))
    if blend_parts:
        blob = b'GFB1' + struct.pack('<4I', nv, len(blend_vertices), len(blend_parts), len(blend_images)) + digest
        blob += b''.join((struct.pack('<I4f', i, *uv) for i, uv in sorted(blend_vertices.items()))) + b''.join((struct.pack('<4I', *p) for p in blend_parts)) + b''.join(blend_images)
        atomic(destination / (gwm.stem + '.gfb'), blob)
    return dict(gwm=file_record(gwm), conversion=file_record(conversion), normal_records=len(normal_records), alpha_parts=len(alpha_rows), blend_parts=len(blend_parts), blend_images=blend_proof, policy='Rebased to newly generated GWM SHA; original packed normal/UV/alpha bytes. QQ alpha blend/depth ordering and terrain shader remain existing native adapters.')

def objects(stage, source, destination, root, elf, contexts):
    from convert_stage_model import main as convert
    from compile_stage_objects import write_hit_box
    from compile_stage_scene import cbox_layout, write_collision, collision_materials
    from excv_stage import parse_scripts
    from stage_geometry import polygons
    mid = PROFILES[stage][0]
    procs, _ = parse_scripts((source / 'scenerio.gcx').read_bytes())
    geo = geometry((source / (stage + '.geom')).read_bytes(), allow_stale_size=stage == 'n023a')
    (destination / 'objects').mkdir(exist_ok=True)
    archive = (source / 'cache.dar').read_bytes()
    entries = {e['name']: e for e in dar(archive)[0]}
    model_reports = []
    overrides = {}
    external_models = {}
    external_sources = []
    if stage == 'n022a':
        from excv_profiles_b import recover_qq_diffuse, OBJECTS
        overrides, proof = recover_qq_diffuse(root, destination / 'audit', OBJECTS)
        save_json(destination / 'audit/objects-textures.json', proof)
        overrides.update(supplemental_qq_images(root, destination / 'audit/supplemental-images'))
        if 'cbox_a_sk.mdn' not in entries:
            original = root / 'stage/r_onlinelobby/resident.dar'
            before = file_record(original)
            raw, mode, _ = read_package(original, 'r_onlinelobby')
            e = next((e for e in dar(raw)[0] if e['name'] == 'cbox_a_sk.mdn'))
            blob = take(raw, int(e['offset'], 0), e['size'])
            require(sha(blob) == 'af6cca57a01fc4d3dff5cc4338d423d7d019af99518586c048ac20adff6cfeb1', 'Reviewed resident CBOX model changed')
            path = destination / 'audit/cbox_a_sk.mdn'
            atomic(path, blob)
            external_models['cbox_a_sk'] = path
            require(before == file_record(original), 'Original resident changed')
            external_sources.append(dict(source=before, mode=mode, entry=e, output=file_record(path)))

    def model(name, relative=None):
        name = Path(name).stem
        path = destination / (relative or 'objects/' + name + '.gwm')
        path.parent.mkdir(parents=True, exist_ok=True)
        report = destination / 'audit' / (name + '.model.json')
        if any((r['name'] == name for r in model_reports)):
            prior = next((r for r in model_reports if r['name'] == name))
            atomic(path, Path(prior['output']['path']).read_bytes())
            return
        convert(selected=[name], output=path, report=report, stage_folder=source, texture_overrides=overrides, texture_context_folders=contexts, model_sources=external_models)
        r = json.loads(report.read_text(encoding='utf-8'))
        require(not r['missing_textures'] and (not r['unsupported_meshes']), 'Incomplete object model: ' + name)
        model_reports.append(dict(name=name, output=file_record(path), report=file_record(report)))
    if stage not in ('n022a', 'n007a'):
        profile = stage + '_native_static_v1'
        atomic(destination / (stage + '.objects.cfg'), f'MGO2MT.STAGE_OBJECTS 1 {profile} {mid} 255 0\n'.encode())
        atomic(destination / (stage + '.bindings.cfg'), f'MGO2MT.STAGE_BINDINGS 1 {profile} 0\n'.encode())
        atomic(destination / (stage + '.cbox.cfg'), b'MGO2MT.STAGE_CBOX 1 0 0\n')
        return dict(status='existing_native_static_profile', models=[])
    if stage == 'n007a':
        rows = []
        for name, key, count, radius in [('n007a_light_a0', 1907790, 4, 6000), ('n007a_light_b0', 1907822, 11, 3000)]:
            model(name)
            e = entries[name + '.mdn']
            raw = take(archive, int(e['offset'], 0), e['size'])
            h = header(raw)
            require(h[4] == 2, 'Lamp mesh count')
            unit = h[12] + 80
            require(read(raw, unit, 'I')[0] == 2, 'Lamp target unit group')
            maximum = read(raw, unit + 32, '3f')
            minimum = read(raw, unit + 48, '3f')
            hit = 'objects/' + name + '.hit.cfg'
            write_hit_box(destination / hit, [(a + z) * 0.5 for a, z in zip(maximum, minimum)], [(a - z) * 0.5 for a, z in zip(maximum, minimum)])
            children = property_children(geo, key)
            require(len(children) == count, 'Lamp original child chain')
            for child in children:
                require('scale_raw' not in child or all((abs(v - 1) <= 1e-06 for v in child['scale_raw'][:3])), 'Lamp scaled placement')
                index = len(rows)
                rows.append(dict(index=index, binding=2953248769 + index, component=2953252865 + 2 * index, position=child['position'][:3], degrees=child.get('rotation_degrees', [0, 0, 0]), model='objects/' + name + '.gwm', hit=hit, radius=radius, property=child))
        require(len(rows) == 15, 'Lamp profile count')
        registry = 'MGO2MT.STAGE_OBJECTS 1 n007a_native_lights_v1 7 255 15\n'
        bindings = 'MGO2MT.STAGE_BINDINGS 1 n007a_native_lights_v1 15\n'
        for r in rows:
            registry += line([r['index'], r['binding'], 1, 'bits'])
            bindings += line([r['binding'], 1, -1, *r['position'], *r['degrees'], 2, 2])
            bindings += line([r['component'], 1, 0, r['model'], 4, r['hit'], 1, 0, 0, 0, 0, 0, 0, 0]) + line([r['component'] + 1, 1, 1, r['model'], 3, '-', 0, 0, 0, 0, 0, 0, 0, 0])
            for value, enabled in [(0, 1), (1, 0)]:
                bindings += line([1, value, 0, 4294967295, enabled, *r['position'], r['radius']])
        atomic(destination / 'n007a.objects.cfg', registry.encode())
        atomic(destination / 'n007a.bindings.cfg', bindings.encode())
        atomic(destination / 'n007a.cbox.cfg', b'MGO2MT.STAGE_CBOX 1 0 0\n')
        return dict(status='existing_native_lamp_profile', entries=rows, models=model_reports, original_entire_registry=False)
    from compile_object_registry import compile_profile
    from audit_object_registry import expand
    registry_text, registry = compile_profile(source, elf, {'entries': list(entries.values())})
    atomic(destination / 'n022a.objects.cfg', registry_text.encode())
    save_json(destination / 'audit/object-registry.json', registry)
    cbox, cp = cbox_layout(procs, geo)
    atomic(destination / 'n022a.cbox.cfg', cbox.encode())
    names = {'cbox_a_sk', 'cbox_a_kuzure_sk'}
    for e in registry['entries']:
        if e['kind'] != 'cbox':
            names.add(Path(e['models'][0]).stem)
        if 'glass_model' in e:
            names.add(Path(e['glass_model']).stem)
    for name in sorted(names):
        model(name)
    raw = (source / 'n022a.geom').read_bytes()
    polys = polygons(raw)
    defs = collision_materials(procs)
    collisions = {}
    for key in (9973560, 9973688, 10402662, 713957):
        path = f'objects/geom_{key:06x}.collision.cfg'
        r = write_collision(destination / path, polys, raw, defs, key)
        require(r['triangles'] >= 12, 'Missing physical object GEOM')
        collisions[key] = path
    hits = {}
    for r in registry['resources']:
        name = r['name']
        if not name.startswith('s01a_btle_') or not name.endswith('0_sk.mdn'):
            continue
        b = take(archive, int(r['offset'], 0), r['size'])
        h = header(b)
        a = read(b, h[12] + 32, '3f')
        z = read(b, h[12] + 48, '3f')
        lo = [min(x, y) for x, y in zip(a, z)]
        hi = [max(x, y) for x, y in zip(a, z)]
        hit = 'objects/' + Path(name).stem + '.hit.cfg'
        hits[name] = hit
        write_hit_box(destination / hit, [(x + y) * 0.5 for x, y in zip(lo, hi)], [(y - x) * 0.5 for x, y in zip(lo, hi)])
    drum = next((e for e in registry['entries'] if e['kind'] == 'blast_drum'))['scene_semantics']['gm_hit']
    drum_hits = []
    for i, box in enumerate(drum['boxes']):
        path = f'objects/blast_drum_{i}.hit.cfg'
        write_hit_box(destination / path, box['center'], box['half_extent'])
        drum_hits.append(path)
    bindings = 'MGO2MT.STAGE_BINDINGS 1 n022a_success32 32\n'
    component = 0
    for e in registry['entries']:
        parts = []

        def part(mask, value, name='-', selection=0, collision='-', hit=False, placement=None):
            nonlocal component
            component += 1
            filename = 'objects/' + Path(name).stem + '.gwm' if name != '-' else '-'
            parts.append([component, mask, value, filename, selection, collision, int(hit), int(placement is not None), *(placement['position'] if placement else [0, 0, 0]), *(placement['degrees'] if placement else [0, 0, 0])])
        kind = e['kind']
        if kind == 'car':
            key = e['command']['options']['0x30bdb7'][0]['value']
            part(0, 0, e['models'][0], collision=collisions[key])
            part(1, 0, e['glass_model'], 6 if e['models'][0] == 's01a_car_a0_sk.mdn' else 2)
            part(1, 1, e['glass_model'], 1)
        elif kind == 'blast_drum':
            part(1, 0, e['models'][0], collision=collisions[10402662])
            for path in drum_hits:
                part(1, 0, collision=path, hit=True)
        elif kind == 'bottle_group':
            for i, p in enumerate(e['children']):
                part(1 << i, 0, e['models'][0], collision=hits[e['models'][0]], hit=True, placement=p)
        elif kind == 'cbox':
            part(3, 0, 'cbox_a_sk.mdn', collision=collisions[713957])
            part(3, 1, 'cbox_a_kuzure_sk.mdn', 1, collisions[713957])
            part(3, 2, 'cbox_a_kuzure_sk.mdn', 2, collisions[713957])
        else:
            raise ValueError('Unknown object actor kind')
        bindings += line([e['binding_id'], e['width'], e.get('layout_ordinal', -1), *e.get('position', [0, 0, 0]), *e.get('degrees', [0, 0, 0]), len(parts), 0]) + ''.join((line(p) for p in parts))
    atomic(destination / 'n022a.bindings.cfg', bindings.encode())
    prop_names = ['s01a_car_a0_sk', 's01a_car_b0_sk', 's01a_drum_a0_sk', 's01a_idor_a0_sk', 's01a_lckr_door_sk', 's01a_lckr_body_sk']
    prop_hashes = {int(entries[n + '.mdn']['stem_hash24'], 16): i for i, n in enumerate(prop_names)}
    placements = []
    for i, name in enumerate(prop_names):
        model(name, f'props/{i}.gwm')
    require(elf.cstring(16685080) == 'ibox_item_large' and elf.cstring(16685096) == 'ibox_item_mid' and (elf.cstring(16519040) == 'ibox_item_small'), 'Original item model literals')
    for name, file in [('ibox_item_large', '113.gwm'), ('ibox_item_small', '140.gwm'), ('ibox_item_mid', 'ibox_item_mid.gwm')]:
        model(name, 'items/' + file)
    for e in registry['entries']:
        if e['kind'] not in ('car', 'blast_drum'):
            continue
        which = prop_names.index(Path(e['models'][0]).stem)
        placements.append([e['hash'], which, int(which in (0, 1)), *e['position'], e['degrees'][1]])
    for c in expand({p['procedure']: p for p in procs}, 87):
        opts = c['options']
        names = opts.get('0x91d13', [])
        if len(names) != 1 or names[0].get('value') not in prop_hashes:
            continue
        which = prop_hashes[names[0]['value']]
        if which < 3:
            continue
        for key in ('0x39d650', '0x34ebb8', '0x1a134', '0x34eba4'):
            for ref in opts.get(key, []):
                found = [p for p in geo['properties'] if p['hash'] == ref.get('value') and 'position' in p]
                if not found:
                    continue
                if which == 3 and len(found) == 2:
                    expected = {13944971: {5565200, 5614576}, 13944972: {5565248, 5614624}}
                    require({p['offset'] for p in found} == expected.get(ref['value']), 'QQ native door root profile changed')
                    found = [max(found, key=lambda p: p['offset'])]
                require(len(found) == 1, 'Prop placement ambiguity')
                p = found[0]
                row = [p['hash'], which, int(which in (0, 1)), *p['position'][:3], p.get('rotation_degrees', [0, 0, 0])[1]]
                if row not in placements:
                    placements.append(row)
    require(len(placements) == 18, 'QQ original placement count')
    text = 'MGO2MT.STAGE_PLACEMENTS 1 18\n' + ''.join((' '.join((str(0 if i == 6 and v == 0 else v) for i, v in enumerate(r))) + '\n' for r in placements))
    atomic(destination / 'n022a.placements.cfg', text.encode())
    return dict(status='reviewed_success32', registry=registry, cbox=cp, models=model_reports, components=component, placements=placements, external_sources=external_sources, door_policy='Existing native preview selects final GEOM root duplicate; original active door root dispatch remains unverified.')

def compile_runtime(stage, source, destination, source_root, elf_path, converted_stage=None):
    require(stage in PROFILES, 'Stage runtime recipe is not reviewed')
    source = Path(source)
    root = local_root(source_root)
    destination = validate_output(root, destination)
    destination.mkdir(parents=True, exist_ok=True)
    (destination / 'audit').mkdir(exist_ok=True)
    elf = ElfImage(elf_path)
    require(elf.sha256 == ELF_SHA, 'Original ELF revision not reviewed')
    from compile_combat_spawn import compile_stage
    paths = [source / 'scenerio.gcx', source / (stage + '.geom'), source / 'cache.dar']
    before = [file_record(p) for p in paths]
    qq = read_package(root / 'stage/n022a/scenerio.gcx', 'n022a')[0]
    with tempfile.TemporaryDirectory(prefix='excv-runtime-') as tmp:
        contract = Path(tmp) / stage
        contract.mkdir()
        for path in paths[:2]:
            atomic(contract / path.name, path.read_bytes())
        spawn = compile_stage(contract, PROFILES[stage][0], destination, elf_path)
    item_report = pickups(stage, source, destination, qq)
    water_report = water(stage, source, destination)
    contexts = sorted(source.parent.glob('layer*'))
    sky_report = sky(stage, source, destination, contexts)
    object_report = objects(stage, source, destination, root, elf, contexts)
    companions = []
    if converted_stage is not None:
        for gwm in sorted(Path(converted_stage).glob('*.gwm')):
            if '.sky.' in gwm.name:
                continue
            companions.append(material_companions(stage, source, gwm, gwm.with_suffix('.model.json'), destination, contexts))
    require(before == [file_record(p) for p in paths], 'Runtime original source changed')
    report = dict(format='MGO2MTEXCV.STAGE_RUNTIME.1', stage=stage, map=PROFILES[stage][0], sources=before, elf=file_record(Path(elf_path)), spawn=spawn, items=item_report, water=water_report, sky=sky_report, material_companions=companions, objects=object_report, game_activation=False, files=[file_record(p) for p in destination.rglob('*') if p.is_file() and 'audit' not in p.relative_to(destination).parts and (p.name != 'runtime.json')])
    save_json(destination / 'runtime.json', report)
    return report
