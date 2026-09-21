# Generated converter-only source; historical analysis entry points omitted.
"""Convert GCX proc84's static stage set and its original diffuse images to GWM.

Uses the existing MDN/TXN/DLD readers. No guessed filename prefix enumeration.
GWM version 3 retains MDN authored COLOR0, UV1, and lossless material evidence.
Only the audited 0x120000 additive COLOR0 term is currently rendered. Other
material vectors and shader visibility flags remain evidence, not guessed state.
"""
from extract_stage_normals import normal as rsx_normal
from pathlib import Path
import json, struct, math, hashlib
from title_assets import read, take, qar, txn, dld
from convert_character_model import header
from stage_scene import ROOT, hash24
from gwp import record
from original_material import UNRESOLVED, material_record, encode_material_trailer

def main(selected=None, output=None, report=None, texture_overrides=None, model_sources=None, stage_folder=None, scene_audit=None, payload_names=None, material_profiles=None, texture_context_folders=None):
    folder = Path(stage_folder) if stage_folder is not None else ROOT / 'work/stages/n022a'
    alias_index = ROOT / 'work/stages/native/material_textures/index.json'
    if stage_folder is None and texture_overrides is None and alias_index.exists():
        texture_overrides = {}
        for key, item in json.loads(alias_index.read_text(encoding='utf-8'))['images'].items():
            key = int(key)
            path = alias_index.parent / f'{key:06x}.bin'
            actual = record(path)
            expected = item['payload_file']
            if (actual['size'], actual['sha256']) != (expected['size'], expected['sha256']):
                raise ValueError('Reviewed stage texture alias changed')
            texture_overrides[key] = path
    audit_path = Path(scene_audit) if scene_audit is not None else folder / 'scenerio.gcx' if stage_folder is not None else ROOT / 'outputs/stage_scene_audit.json'
    audit = json.loads(audit_path.read_text(encoding='utf-8')) if stage_folder is None else {'commands': []}
    names = []
    for c in audit['commands']:
        if c['procedure'] == 84 and c['args'] == ['NewForeach']:
            data = next((o['values'] for o in c['options'] if o['code'] == '0x3392e1'))
            names.extend(data[1::2])
    if stage_folder is None and (len(names) != 17 or len(set(names)) != 17):
        raise ValueError('GCX static set contract changed')
    if selected is not None:
        names = selected
    dar = (folder / 'cache.dar').read_bytes()
    if stage_folder is not None:
        from archive_inventory import dar as read_dar
        inventory = read_dar(dar)[0]
    else:
        inventory = json.loads((ROOT / 'outputs/stage_n022_inventory.json').read_text(encoding='utf-8'))[0]['entries']
    entries = {e['name']: e for e in inventory}
    q = (folder / 'cache.qar').read_bytes()
    textures = {}
    for e in qar(q):
        if e['name'].endswith('.txn'):
            t = txn(take(q, e['offset'], e['size']))
            for x in t['textures']:
                textures.setdefault(x['key'], []).append((e, t, x))
    payloads = []
    for p in sorted(folder.glob('*.dlz.dld')):
        if payload_names is not None and p.name not in payload_names:
            continue
        b = p.read_bytes()
        for r in dld(b):
            payloads.append((p, b, r))
    vertices = []
    indices = []
    parts = []
    images = []
    image_ids = {}
    evidence = []
    models = []
    missing = []
    unsupported = []
    materials = []
    original_texture_cache = {}
    file_records = {}

    def source_record(path):
        path = Path(path).resolve()
        key = str(path)
        if key not in file_records:
            file_records[key] = record(path)
        return file_records[key]

    def sha256(data):
        return hashlib.sha256(data).hexdigest()
    package_path = str((folder / 'cache.dar').resolve())
    package_sha = sha256(dar)
    qar_path = str((folder / 'cache.qar').resolve())
    qar_sha = sha256(q)
    contexts = None
    context_model_shas = {}
    if texture_context_folders is not None:
        from stage_texture_context import TextureContexts
        contexts = TextureContexts(texture_context_folders, payload_names)

    def original_texture(key, model):
        """Strict stage-local evidence resolver, separate from diffuse aliases.

        A model-specific TXN narrows package resolution. Other stage TXNs are
        accepted only when exactly one remains. Neither common.txn preference
        nor reviewed diffuse override files authorize normal/reflection inputs.
        """
        cache_key = (key, model)
        if cache_key in original_texture_cache:
            return original_texture_cache[cache_key]
        info = {'texture_key': key, 'model': model, 'qar_path': qar_path, 'qar_sha256': qar_sha, 'status': 'unresolved', 'resolution_policy': 'strict stage-local TXN/DLD; no diffuse aliases'}
        result = UNRESOLVED
        if contexts is not None:
            image, info = contexts.resolve(key, model, context_model_shas[model], strict=True)
            if image is not None:
                result = images.index(image) if image in images else len(images)
                if result == len(images):
                    images.append(image)
            original_texture_cache[cache_key] = (result, info)
            return (result, info)
        try:
            candidates = textures.get(key, [])
            primary = [v for v in candidates if v[0]['name'] == model + '.txn']
            if primary:
                candidates = primary
            info['candidate_txn_entries'] = [v[0] for v in candidates]
            if len(candidates) != 1:
                raise ValueError('No unique stage-context TXN reference')
            entry, t, x = candidates[0]
            if not 0 <= x['image_index'] < len(t['images']):
                raise ValueError('TXN image index extent')
            im = t['images'][x['image_index']]
            info.update({'txn_entry': entry, 'txn_sha256': sha256(take(q, entry['offset'], entry['size'])), 'txn_texture': x, 'txn_image': im, 'original_image_flags': im['flags'], 'original_image_descriptor': take(q, entry['offset'] + im['offset'], 16).hex()})
            if im['flags'] & 15 != 1:
                info['image_shape_status'] = 'unresolved_original_shape; possible cube or other non-2D resource'
                raise ValueError(f"Original TXN image flags 0x{im['flags']:04X} have non-2D shape nibble; cube face/layout mapping unverified")
            info['image_shape_status'] = 'original_shape_nibble_1_2D'
            if (x['width'], x['height']) != (im['width'], im['height']) or x['x'] or x['y']:
                raise ValueError('TXN atlas mapping is not supported for restored material inputs')
            if x['uv_scale'] != [1, 1] or x['uv_offset'] != [0, 0]:
                raise ValueError('TXN UV transform is not supported for restored material inputs')
            if not 0 < im['width'] <= 4096 or not 0 < im['height'] <= 4096 or im['codec'] not in (9, 11, 16):
                raise ValueError('Original image dimensions or codec unsupported')
            size = (im['width'] + 3) // 4 * ((im['height'] + 3) // 4) * {9: 8, 11: 16, 16: 16}[im['codec']]
            matches = [(p, b, r) for p, b, r in payloads if r['key'] == x['archive'] and r['index'] == x['index'] and (r['parent_size'] == 0) and (r['size'] >= size)]
            if not matches:
                raise ValueError('Original image DLD payload missing in stage context')
            p, b, r = min(matches, key=lambda v: v[2]['priority'])
            payload = take(b, r['offset'] + 32, size)
            if any((take(bb, rr['offset'] + 32, size) != payload for _, bb, rr in matches if rr['priority'] == r['priority'])):
                raise ValueError('Original image DLD priority is ambiguous')
            image = struct.pack('<4I', im['width'], im['height'], im['codec'], size) + payload
            result = images.index(image) if image in images else len(images)
            if result == len(images):
                images.append(image)
            info.update({'status': 'resolved_original_bytes', 'dld': source_record(p), 'dld_record': r, 'payload_sha256': sha256(payload), 'embedded_image_sha256': sha256(image), 'color_space': 'raw block bytes; no color conversion', 'sampler_interpretation': 'unresolved; raw MDN/TXN retained'})
        except (ValueError, KeyError, IndexError, OSError, struct.error) as exc:
            result = UNRESOLVED
            info['fallback_reason'] = str(exc)
        original_texture_cache[cache_key] = (result, info)
        return (result, info)

    def texture(key, model):
        cache_key = (key, model)
        if cache_key in image_ids:
            return image_ids[cache_key]
        if texture_overrides and key in texture_overrides:
            source = Path(texture_overrides[key])
            image = source.read_bytes()
            if len(image) < 16:
                raise ValueError('Texture override extent')
            w, h, codec, size = struct.unpack_from('<4I', image)
            if not 0 < w <= 4096 or not 0 < h <= 4096 or codec not in (9, 11, 16):
                raise ValueError('Texture override layout')
            if size != (w + 3) // 4 * ((h + 3) // 4) * {9: 8, 11: 16, 16: 16}[codec] or len(image) != 16 + size:
                raise ValueError('Texture override payload')
            result = len(images)
            images.append(image)
            image_ids[cache_key] = result
            evidence.append({'texture_key': key, 'model': model, 'reviewed_original_texture': record(source)})
            return result
        if contexts is not None:
            image, info = contexts.resolve(key, model, context_model_shas[model])
            if image is None:
                missing.append({'model': model, 'key': key, 'reason': info['fallback_reason'], 'context_attempts': info['context_attempts']})
                image = struct.pack('<4I', 4, 4, 9, 8) + struct.pack('<HHI', 33808, 33808, 0)
            else:
                evidence.append(info)
            result = images.index(image) if image in images else len(images)
            if result == len(images):
                images.append(image)
            image_ids[cache_key] = result
            return result
        found = textures.get(key, [])
        available = []
        for v in found:
            x = v[2]
            im = v[1]['images'][x['image_index']]
            size = (im['width'] + 3) // 4 * ((im['height'] + 3) // 4) * {9: 8, 11: 16}[im['codec']]
            if any((r['key'] == x['archive'] and r['index'] == x['index'] and (r['parent_size'] == 0) and (r['size'] >= size) for _, _, r in payloads)):
                available.append(v)
        found = available
        primary = [v for v in found if v[0]['name'] == model + '.txn']
        if primary:
            found = primary
        elif len(found) > 1:
            common = [v for v in found if v[0]['name'] == 'common.txn']
            if common:
                found = common
        if not found:
            missing.append({'model': model, 'key': key, 'reason': 'texture not in stage TXN set'})
            image = struct.pack('<4I', 4, 4, 9, 8) + struct.pack('<HHI', 33808, 33808, 0)
            result = images.index(image) if image in images else len(images)
            image_ids[cache_key] = result
            if result == len(images):
                images.append(image)
            return result
        if len(found) != 1:
            raise ValueError(('TXN lookup', hex(key), len(found)))
        entry, t, x = found[0]
        im = t['images'][x['image_index']]
        if (x['width'], x['height']) != (im['width'], im['height']) or x['x'] or x['y']:
            raise ValueError('Stage texture atlas requires UV conversion')
        block = {9: 8, 11: 16}[im['codec']]
        size = (im['width'] + 3) // 4 * ((im['height'] + 3) // 4) * block
        matches = [(p, b, r) for p, b, r in payloads if r['key'] == x['archive'] and r['index'] == x['index'] and (r['parent_size'] == 0) and (r['size'] >= size)]
        if not matches:
            raise ValueError(('DLD lookup', hex(key)))
        p, b, r = min(matches, key=lambda v: v[2]['priority'])
        payload = take(b, r['offset'] + 32, size)
        if any((take(bb, rr['offset'] + 32, size) != payload for _, bb, rr in matches if rr['priority'] == r['priority'])):
            raise ValueError(('DLD priority ambiguity', model, hex(key), [(str(pp), rr['key'], rr['index'], rr['priority']) for pp, bb, rr in matches]))
        image = struct.pack('<4I', im['width'], im['height'], im['codec'], size) + payload
        result = images.index(image) if image in images else len(images)
        image_ids[cache_key] = result
        if result == len(images):
            images.append(image)
        evidence.append({'key': key, 'txn': entry['name'], 'dld': str(p), 'record': r, 'image': im, 'txn_texture_index': x['index'], 'txn_image_index': x['image_index'], 'uv_scale': x['uv_scale'], 'uv_offset': x['uv_offset']})
        return result
    for name in names:
        if model_sources and name in model_sources:
            source = Path(model_sources[name])
            entry = record(source)
            b = source.read_bytes()
            mdn_path = str(source.resolve())
            material_package_path = ''
            material_package_sha = ''
            context_proven = False
            packaged = entries.get(name + '.mdn')
            if packaged and packaged['size'] == len(b):
                packaged_bytes = take(dar, int(packaged['offset'], 0), packaged['size'])
                if packaged_bytes == b:
                    context_proven = True
                    material_package_path = package_path
                    material_package_sha = package_sha
        else:
            entry = entries.get(name + '.mdn')
            if not entry:
                raise ValueError(('GCX required MDN missing', name))
            b = take(dar, int(entry['offset'], 0), entry['size'])
            mdn_path = package_path + '#' + name + '.mdn'
            material_package_path = package_path
            material_package_sha = package_sha
            context_proven = True
        mdn_sha = sha256(b)
        context_model_shas[name] = mdn_sha
        material_source = {'mdn_path': mdn_path, 'mdn_sha256': mdn_sha, 'package_path': material_package_path, 'package_sha256': material_package_sha}
        h = header(b)
        if h[2] and selected is None:
            raise ValueError(('Static stage has bones', name, h[2]))
        model_parts = []
        for mi in range(h[4]):
            m = read(b, h[12] + 80 * mi, '8I12f')
            if m[4] >= h[6] or m[3] + m[2] > h[5] or (not 0 < m[6] <= 65536):
                raise ValueError('Stage mesh references')
            raw_declaration = take(b, h[14] + 48 * m[4], 48)
            vd = read(raw_declaration, 0, '4I')
            if vd[1] > 16:
                raise ValueError('Stage vertex attribute count')
            if m[0] >= h[3]:
                raise ValueError('Mesh group index outside MDN group table')
            mesh_name_hash = read(b, h[11] + 16 * m[0], 'I')[0]
            defs = take(b, h[14] + 48 * m[4] + 16, 16)
            pos = take(b, h[14] + 48 * m[4] + 32, 16)
            sem = {defs[i] & 15: (defs[i] >> 4, pos[i]) for i in range(vd[1])}
            if sem.get(0, (None,))[0] != 1 or sem.get(8, (None,))[0] != 7:
                unsupported.append({'model': name, 'mesh': mi, 'declaration': sem, 'reason': 'non-diffuse vertex declaration'})
                continue
            if sem[0][1] + 12 > vd[2] or sem[8][1] + 4 > vd[2]:
                raise ValueError('Stage vertex attribute extent')
            has_uv1 = sem.get(9, (None,))[0] == 7 and sem[9][1] + 4 <= vd[2]
            uv1_semantic = 'MDN semantic 9 half2' if has_uv1 else 'UV0 copy; semantic 9 absent or unsupported'
            vb = take(b, h[18] + vd[3], m[6] * vd[2])
            base = len(vertices)
            local = []
            normals = [[0.0, 0.0, 0.0] for _ in range(m[6])]
            for vi in range(m[6]):
                xyz = read(vb, vi * vd[2] + sem[0][1], '3f')
                uv = read(vb, vi * vd[2] + sem[8][1], '2e')
                uv1 = read(vb, vi * vd[2] + sem[9][1], '2e') if has_uv1 else uv
                if not all((math.isfinite(v) for v in (*xyz, *uv, *uv1))):
                    raise ValueError('Nonfinite stage vertex')
                color = (0.0, 0.0, 0.0, 1.0)
                if 3 in sem:
                    if sem[3][0] != 8 or sem[3][1] + 4 > vd[2]:
                        raise ValueError('Unsupported authored COLOR0 declaration')
                    color = tuple((c / 255.0 for c in take(vb, vi * vd[2] + sem[3][1], 4)))
                local.append([*xyz, *uv, *color, *uv1])
            for fi in range(m[3], m[3] + m[2]):
                f = read(b, h[13] + 16 * fi, 'HHIIHH')
                ids = read(b, h[20] + f[2], str(f[1]) + 'H')
                if not ids or len(ids) % 3 or max(ids) >= m[6] or (f[3] >= h[7]):
                    raise ValueError('Stage face range')
                raw_material = take(b, h[15] + 112 * f[3], 112)
                mat = read(raw_material, 0, '12I')
                if not 0 < mat[2] <= 8 or mat[4] >= h[8]:
                    raise ValueError('Stage diffuse slot')
                tx = read(b, h[16] + 32 * mat[4], '2I4f2I')
                if tx[2:6] != (1.0, 1.0, 0.0, 0.0):
                    raise ValueError('Stage MDN UV transform')
                diffuse = read(b, h[15] + 112 * f[3] + 48, '4e')
                tex = texture(tx[0], name)
                parts.append((len(indices), len(ids), tex, 0, mat[0], 1.0, 1.0, 1.0))
                indices.extend((base + i for i in ids))
                original_textures = []
                restore_missing = []
                for slot, reference_index in enumerate(mat[4:4 + mat[2]]):
                    if reference_index >= h[8]:
                        raise ValueError('Stage material texture reference extent')
                    raw_reference = take(b, h[16] + 32 * reference_index, 32)
                    texture_key = read(raw_reference, 0, 'I')[0]
                    if context_proven:
                        image_index, provenance = original_texture(texture_key, name)
                    else:
                        image_index = UNRESOLVED
                        provenance = {'status': 'unresolved', 'texture_key': texture_key, 'fallback_reason': 'External model source has no verified association with this stage texture package'}
                    original_textures.append({'slot': slot, 'raw_reference': raw_reference.hex(), 'embedded_image_index': image_index, 'provenance': provenance})
                    if image_index == UNRESOLVED:
                        restore_missing.append(f'slot{slot}: ' + provenance['fallback_reason'])
                fallback = 'No reviewed model/package/VFP shader rule attached; retain existing diffuse rendering'
                if restore_missing:
                    fallback += '; ' + '; '.join(restore_missing)
                original = material_record(raw_material, raw_declaration, f[3], material_source, original_textures, uv1_semantic, fallback)
                if material_profiles:
                    from material_restore_profiles import apply_profile
                    original = apply_profile(original, material_profiles)
                materials.append(original)
                model_parts.append({'material_flags': mat[0], 'material_name_hash': mat[1], 'packet_flags': f[0], 'texture_key': tx[0], 'mesh': mi, 'mesh_group': m[0], 'mesh_name_hash': mesh_name_hash, 'mesh_flags': m[1], 'source_vector0': diffuse, 'authored_color0': 3 in sem, 'rendered_authored_rgb_scale': 2 if mat[0] == 1179648 else 0, 'material_record_index': len(materials) - 1, 'uv1_semantic': uv1_semantic})
                for k in range(0, len(ids), 3):
                    a, c, d = [local[ids[k + j]] for j in range(3)]
                    u = [c[j] - a[j] for j in range(3)]
                    v = [d[j] - a[j] for j in range(3)]
                    n = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]]
                    for j in ids[k:k + 3]:
                        for axis in range(3):
                            normals[j][axis] += n[axis]
            for vi, (v, n) in enumerate(zip(local, normals)):
                if sem.get(2, (None,))[0] == 10:
                    if sem[2][1] + 4 > vd[2]:
                        raise ValueError('Stage normal attribute extent')
                    packed = read(vb, vi * vd[2] + sem[2][1], 'I')[0]
                    n = rsx_normal(packed)
                length = math.sqrt(sum((x * x for x in n)))
                n = [x / length for x in n] if length > 1e-12 else [0, 1, 0]
                vertices.append((*v[:3], *n, *v[3:]))
        models.append({'name': name, 'mdn_path': mdn_path, 'mdn_sha256': mdn_sha, 'texture_context_proven': context_proven, 'parts': model_parts})
    bounds = [min((v[i] for v in vertices)) for i in range(3)] + [max((v[i] for v in vertices)) for i in range(3)]
    data = b'GWM1' + struct.pack('<5I6f', 3, len(vertices), len(indices), len(parts), len(images), *bounds)
    data += b''.join((struct.pack('<14f', *v) for v in vertices)) + struct.pack('<' + str(len(indices)) + 'I', *indices) + b''.join((struct.pack('<5I3f', *p) for p in parts)) + b''.join(images)
    data += encode_material_trailer(materials)
    if len(data) > 64 * 1024 * 1024:
        raise ValueError('GWM3 exceeds runtime 64 MiB file limit')
    out = output or ROOT / 'work/stages/native/n022a_textured.gwm'
    out.write_bytes(data)
    result = {'output': record(out), 'script_audit': record(audit_path), 'converter': record(Path(__file__)), 'vertices': len(vertices), 'triangles': len(indices) // 3, 'parts': len(parts), 'textures': evidence, 'models': models, 'bounds': bounds, 'missing_textures': missing, 'unsupported_meshes': unsupported, 'format_version': 3, 'original_materials': materials, 'material_converter': record(Path(__file__).with_name('original_material.py')), 'authored_color0_nonzero_vertices': sum((any((v[i] > 0 for i in (8, 9, 10))) for v in vertices)), 'limits': ['Diffuse textures; original DEC3N normals, triangle fallback for other declarations', 'GCX proc84 static sets; flag8 and dynamic actors excluded', 'Authored COLOR0.rgb*2 enabled only for exact material 0x120000; other material behavior pending', 'LT3 sampled illumination is a native diffuse approximation of the unrecovered three-basis background prelight', 'All material raw records/P0-P7 and texture references preserved; only active coefficient count is meaningful', 'Without an explicit reviewed material profile, shader program provenance/rules remain unbound and flags zero', 'Additional material inputs resolve only within the unambiguous stage context; diffuse aliases cannot authorize them', 'Original image shape nibble other than 1 is unresolved; single-face DDS headers do not exclude cube textures', 'UV1 is semantic 9 half2 where present, otherwise a documented UV0 copy; original tangent frame remains unresolved', 'Transparency, normal/specular, sampler state, gamma and RSX parity pending']}
    (report or ROOT / 'outputs/stage_textured_conversion.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print('GCX static models', len(models), 'textures', len(images), 'vertices', len(vertices), 'triangles', len(indices) // 3, 'bytes', len(data))
