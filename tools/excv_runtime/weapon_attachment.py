# Generated converter-only source; historical analysis entry points omitted.
"""Bounded AK102/OPERATOR model candidates from saved base-game extraction.

Does not run the appearance-catalog generator or read patch assets. Original
model/CNP bytes stay immutable; output is a native static GWM storage adapter.
"""
from pathlib import Path
import argparse, hashlib, json, math, struct, unittest
from convert_character_catalog import geometry, bone_rows
from convert_character_model import header
from prepare_gekko_model import dds_top
from weapon_connect_points import parse, hash24
from character_motion import Bits, load as load_motion, sample as sample_motion
from title_assets import read, take, txn
from original_material import UNRESOLVED, material_record, encode_material_trailer
from material_restore_profiles import apply_profile
ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT.parent / 'stage-extraction-20260913/stage01/stage/s01a55l'
TEXTURES = ROOT.parent / 'mdn-texture-catalog-20260914/textures/stage-extraction-20260913/stage01/stage/s01a55l'
WEAPONS = ((25, 'AK102', 9158920, 9152394, 10900846, 15486736), (3, 'OPERATOR', 450964, 444438, 5791270, 15447336))

def original_point_float(word):
    """D56CC0..D56E98 bit expansion; differs from IEEE half for subnormals."""
    if not 0 <= word <= 65535:
        raise ValueError('point word')
    exponent = word & 31744
    if exponent:
        exponent = (exponent << 13) + 998244352
    value = struct.unpack('>f', struct.pack('>I', exponent | word << 16 & 2147483648 | word << 13 & 8380416))[0]
    if not math.isfinite(value):
        raise ValueError('point float')
    return value

def mtp_points(raw):
    """Checked descriptors and initial point values, not an animation player.

    Rotation integers are preserved. D56A60 uses (2**bits-1), unlike the
    existing MTCM reader; current polynomial/SIMD interpolation is not ported.
    """

    def unpack(fmt, offset):
        size = struct.calcsize('>' + fmt)
        if offset < 0 or offset + size > len(raw):
            raise ValueError('MTAR extent')
        return struct.unpack_from('>' + fmt, raw, offset)
    h = unpack('I4H5I', 0)
    if raw[:4] != b'ratM' or not 0 < h[3] <= 256 or (not 0 < h[4] <= 4096):
        raise ValueError('MTAR header')
    bones = set(unpack(str(h[3]) + 'I', h[8]))
    clips = []
    for index in range(h[4]):
        entry = unpack('4I', h[9] + index * 16)
        start = h[7] + entry[2]
        size = entry[3]
        if size < 32 or start + size > len(raw):
            raise ValueError('MTP extent')
        q = raw[start:start + size]
        v = struct.unpack_from('>8I', q)
        if not 0 < v[2] <= 3600 or not 0 < v[4] <= 128 or (not 1 <= v[5] <= 24) or v[6] % 2 or (v[6] + 15 & ~15 != len(q)):
            raise ValueError('MTP header')
        q = q[:v[6]]
        end = 32 + 16 * v[4]
        if end > len(q):
            raise ValueError('MTP descriptors')
        desc = [struct.unpack_from('>4I', q, 32 + j * 16) for j in range(v[4])]
        offsets = sorted(set([2 * x for d in desc for x in d[2:]] + [len(q)]))
        points = []
        seen = set()
        for key, bone, translation, rotation in desc:
            if key in seen or bone not in bones:
                raise ValueError('MTP identity')
            seen.add(key)
            streams = []
            for off in (translation * 2, rotation * 2):
                if off < end or off >= len(q):
                    raise ValueError('MTP stream offset')
                nextoff = next((x for x in offsets if x > off))
                streams.append(q[off:nextoff])
            if len(streams[0]) < 8:
                raise ValueError('MTP translation extent')
            words = struct.unpack_from('>3H', streams[0])
            reader = Bits(streams[1])
            delta = reader.read(8)
            integers = [reader.read(v[5]) for _ in range(3)]
            signs = [reader.read(1) for _ in range(3)]
            points.append(dict(key=key, bone_hash=bone, translation_word_offset=translation, rotation_word_offset=rotation, position_words=list(words), initial_position=[original_point_float(x) for x in words], first_translation_delta=streams[0][7], rotation_first_delta=delta, rotation_quantized=integers, rotation_sign_bits=signs, rotation_denominator=(1 << v[5]) - 1, decoded_rotation=None, rotation_status='current SIMD axis/trigonometry and interpolation not ported', stream_sha256=[hashlib.sha256(x).hexdigest() for x in streams]))
        clips.append(dict(index=index, source_key=v[0], header=list(v), points=points))
    return clips

def record(path):
    b = path.read_bytes()
    return dict(path=str(path.resolve()), size=len(b), sha256=hashlib.sha256(b).hexdigest())

def point_quaternion(integers, signs, bits):
    denominator = (1 << bits) - 1
    angle = integers[0] / denominator * math.pi / 2
    x, y = (integers[i] / denominator for i in (1, 2))
    axis = [x, y, 1 - x - y]
    length = math.sqrt(sum((v * v for v in axis)))
    return [(-1 if signs[i] else 1) * v / length * math.sin(angle) for i, v in enumerate(axis)] + [math.cos(angle)]

def spherical(keys, time):
    a = keys[0]
    b = a
    for k in keys[1:]:
        b = k
        if time <= k[0]:
            break
        a = k
    t = max(0, min(1, (time - a[0]) / (b[0] - a[0]))) if b[0] != a[0] else 0
    qa, qb = (a[1], b[1])
    dot = sum((x * y for x, y in zip(qa, qb)))
    if dot < 0:
        qb = [-v for v in qb]
        dot = -dot
    if dot > 0.9995:
        v = [x * (1 - t) + y * t for x, y in zip(qa, qb)]
    else:
        angle = math.acos(max(-1, min(1, dot)))
        s = math.sin(angle)
        v = [(x * math.sin((1 - t) * angle) + y * math.sin(t * angle)) / s for x, y in zip(qa, qb)]
    n = math.sqrt(sum((x * x for x in v)))
    return [x / n for x in v]

def animated_point(raw, index, key=731764):
    if len(raw) < 32:
        raise ValueError('Truncated MTAR header')
    h = struct.unpack_from('>I4H5I', raw)
    if not 0 <= index < h[4]:
        raise ValueError('MTP clip index')
    single = bytearray(raw)
    struct.pack_into('>H', single, 10, 1)
    struct.pack_into('>I', single, 28, h[9] + index * 16)
    clip = mtp_points(single)[0]
    point = next((p for p in clip['points'] if p['key'] == key), None)
    if point is None:
        raise ValueError('Missing requested MTP point')
    h = struct.unpack_from('>I4H5I', raw)
    entry = struct.unpack_from('>4I', raw, h[9] + index * 16)
    q = raw[h[7] + entry[2]:h[7] + entry[2] + clip['header'][6]]
    offsets = sorted(set([p[k] * 2 for p in clip['points'] for k in ('translation_word_offset', 'rotation_word_offset')] + [len(q)]))

    def stream(name):
        start = point[name] * 2
        return Bits(q[start:next((v for v in offsets if v > start))])
    frames = clip['header'][2]
    bits = clip['header'][5]
    tr = stream('translation_word_offset')
    translations = []
    frame = 0
    for _ in range(frames + 2):
        value = [original_point_float(tr.read(16)) for _ in range(3)]
        translations.append((frame, value))
        if frame >= frames:
            break
        delta = tr.read(8)
        if not delta:
            raise ValueError('Nonprogressing MTP translation')
        frame += delta
    else:
        raise ValueError('MTP translation key budget')
    rr = stream('rotation_word_offset')
    rotations = []
    frame = 0
    for _ in range(frames + 2):
        delta = rr.read(8)
        frame += delta
        value = point_quaternion([rr.read(bits) for _ in range(3)], [rr.read(1) for _ in range(3)], bits)
        rotations.append((frame, value))
        if frame >= frames:
            break
        if len(rotations) > 1 and (not delta):
            raise ValueError('Nonprogressing MTP rotation')
    else:
        raise ValueError('MTP rotation key budget')
    return dict(bone=point['bone_hash'], key=key, source_key=clip['source_key'], frames=frames, positions=[sample_motion(translations, f) for f in range(frames + 1)], rotations=[spherical(rotations, f) for f in range(frames + 1)], translation_keys=translations, rotation_keys=rotations)

def runtime_points(output):
    output.mkdir(parents=True, exist_ok=True)
    rows = []
    payload = bytearray(b'GWH1' + struct.pack('<2I', 2, 15))
    for weapon, name in ((25, 'ak'), (3, 'oprt')):
        path = ROOT / 'work/player_motion/source' / f'sna_wp_{name}.mtar'
        raw = path.read_bytes()
        for stored_index in range(12 if weapon == 25 else 3):
            index = stored_index
            source_path = path
            clip_raw = raw
            point = animated_point(clip_raw, index)
            motion = load_motion(clip_raw, index)
            if point['source_key'] != motion['name'] or point['frames'] != motion['frames']:
                raise ValueError('MTP/MTCM clip identity')
            payload.extend(struct.pack('<8I', weapon, stored_index, motion['name'], motion['frames'], 60, motion['root_bone'], point['bone'], len(motion['tracks'])))
            for frame in range(motion['frames'] + 1):
                payload.extend(struct.pack('<10f', *motion['roots'][frame], *point['positions'][frame], *point['rotations'][frame]))
            for bone, track in sorted(motion['tracks'].items()):
                payload.extend(struct.pack('<I', bone))
                for quat in track:
                    payload.extend(struct.pack('<4f', *quat))
            magazine = animated_point(clip_raw, index, 731381) if weapon == 25 and index in (3, 4, 5) else None
            payload.extend(struct.pack('<I', magazine['bone'] if magazine else 0))
            if magazine:
                for frame in range(motion['frames'] + 1):
                    payload.extend(struct.pack('<7f', *magazine['positions'][frame], *magazine['rotations'][frame]))
            rows.append(dict(weapon=weapon, index=stored_index, source_index=index, source=record(source_path), point=point, magazine=magazine))
    target = output / 'hands.gwh'
    target.write_bytes(payload)
    (output / 'hands.json').write_text(json.dumps(dict(output=record(target), clips=rows, rotation='PPC field order / 2^bits-1 axis decoding; analytic trigonometry and shortest-arc slerp, not PPC bit-exact', translation='D56A60 first point at time zero, next point after first delta; D57A40 linear interpolation'), indent=2), encoding='utf-8')
    return record(target)

def compose(parent, local):
    """Column-major storage, column vectors: parent * local (D37940).

    Caller must supply a proved parent matrix; this does not select a hand bone.
    """
    if len(parent) != 16 or len(local) != 16 or (not all((math.isfinite(v) for v in (*parent, *local)))):
        raise ValueError('finite 4x4 matrices required')
    return [sum((parent[k * 4 + r] * local[c * 4 + k] for k in range(4))) for c in range(4) for r in range(4)]

def model(mdn, destination, material_profiles=None):
    raw = mdn.read_bytes()
    h = header(raw)
    bones = bone_rows(raw, h)
    vertices, indices, parts, keys, evidence = geometry(mdn, bones)
    if evidence['ancestor_bone_fallback'] or any(evidence['translation']):
        raise ValueError('unexpected bone remap')
    manifest_path = TEXTURES / 'texture_manifest.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    images = {}
    sources = []
    for key in sorted(keys):
        path = TEXTURES / f'{key:06x}.dds'
        r = record(path)
        matches = [v for v in manifest['textures'] if v['texture_key'] == key and v.get('dds', {}).get('sha256') == r['sha256']]
        if not matches:
            raise ValueError('unreviewed DDS')
        images[key] = dds_top(path.read_bytes())
        sources.append(dict(key=key, dds=r, provenance=matches))
    if any((p[3] for p in parts)):
        raise ValueError('pattern shader needs separate adapter')
    bounds = [min((v[a] for v in vertices)) for a in range(3)] + [max((v[a] for v in vertices)) for a in range(3)]
    index = {key: i for i, key in enumerate(sorted(images))}
    native_images = list(images.values())
    source = record(mdn)
    manifest_source = record(manifest_path)
    source_context = dict(mdn_path=source['path'], mdn_sha256=source['sha256'], package_path=manifest_source['path'], package_sha256=manifest_source['sha256'])
    model_rows = [row for row in manifest.get('models', []) if Path(row.get('source_mdn', {}).get('path', '')).resolve() == mdn.resolve() and row.get('source_mdn', {}).get('sha256') == source['sha256']]
    context_ok = Path(manifest.get('source_folder', '')).resolve() == mdn.parent.resolve() and len(model_rows) == 1
    texture_cache = {}
    file_records = {}
    txn_cache = {}

    def original_texture(reference_index, raw_reference):
        if reference_index in texture_cache:
            return texture_cache[reference_index]
        key = read(raw_reference, 0, 'I')[0]
        image_index = UNRESOLVED
        provenance = dict(status='unresolved', texture_key=key, mdn_texture_index=reference_index, manifest=manifest_source, model_sha256=source['sha256'], resolution_policy='Exact model SHA, MDN reference, same-folder TXN and reviewed DDS; no global aliases')
        try:
            if not context_ok:
                raise ValueError('No exact model/package manifest association')
            candidates = [row for row in model_rows[0]['texture_slots'] if row['mdn_texture_index'] == reference_index and row['texture_key'] == key]
            if len(candidates) != 1:
                raise ValueError('No unique model texture-reference association')
            slot = candidates[0]
            provenance['manifest_slot'] = slot
            if slot['status'] != 'resolved_same_folder':
                raise ValueError('Cross-folder texture candidate is not authorized')
            path = Path(slot['dds_path']).resolve()
            txn_path = Path(slot['texture_source_txn']).resolve()
            if path.parent != TEXTURES.resolve() or txn_path.parent != mdn.parent.resolve():
                raise ValueError('Texture/ TXN outside original package')
            for input_path in (path, txn_path):
                if input_path not in file_records:
                    file_records[input_path] = record(input_path)
            dds_record = file_records[path]
            txn_record = file_records[txn_path]
            if dds_record['sha256'] != slot['dds_sha256']:
                raise ValueError('Reviewed DDS content changed')
            if not any((Path(s['path']).resolve() == txn_path and s['sha256'] == txn_record['sha256'] for s in manifest['sources'])):
                raise ValueError('Original TXN hash is not in package manifest')
            matches = [t for t in manifest['textures'] if t['texture_key'] == key and Path(t.get('source_txn', '')).resolve() == txn_path and (t.get('dds', {}).get('sha256') == dds_record['sha256'])]
            if len(matches) != 1:
                raise ValueError('No unique TXN/DDS provenance')
            reviewed = matches[0]
            provenance.update(dds=dds_record, txn=txn_record, texture=reviewed)
            if txn_path not in txn_cache:
                txn_cache[txn_path] = txn(txn_path.read_bytes())
            original = txn_cache[txn_path]
            entries = [t for t in original['textures'] if t['key'] == key and t == reviewed['txn_texture']]
            if len(entries) != 1:
                raise ValueError('Original TXN texture descriptor differs from manifest')
            entry = entries[0]
            im = original['images'][entry['image_index']]
            if im != reviewed['txn_image']:
                raise ValueError('Original TXN image descriptor differs from manifest')
            if im['flags'] & 15 != 1:
                raise ValueError(f"Original TXN image kind 0x{im['flags'] & 15:x} needs reviewed dimension/face mapping; catalog DDS is 2D-only")
            if entry['x'] or entry['y'] or (entry['width'], entry['height']) != (im['width'], im['height']):
                raise ValueError('TXN atlas is not supported for restored slots')
            if entry['uv_scale'] != [1, 1] or entry['uv_offset'] != [0, 0]:
                raise ValueError('TXN UV transform is not supported for restored slots')
            if read(raw_reference, 8, '4f') != (1.0, 1.0, 0.0, 0.0):
                raise ValueError('MDN UV transform is not supported for restored slots')
            image = dds_top(path.read_bytes())
            if (image[0], image[1], image[2]) != (im['width'], im['height'], im['codec']):
                raise ValueError('DDS dimensions or codec differs from original TXN')
            image_index = native_images.index(image) if image in native_images else len(native_images)
            if image_index == len(native_images):
                native_images.append(image)
            provenance.update(status='resolved_original_bytes', color_space='Unmodified DDS blocks; no color conversion', sampler_interpretation='Raw MDN/TXN retained; original sampler mapping unresolved', payload_sha256=hashlib.sha256(image[3]).hexdigest())
        except (ValueError, KeyError, IndexError, OSError, struct.error) as exc:
            provenance['fallback_reason'] = str(exc)
        texture_cache[reference_index] = (image_index, provenance)
        return (image_index, provenance)
    restored_vertices = []
    materials = []
    part_audit = []
    vertex_base = 0
    part_index = 0
    index_offset = 0
    for mesh_index in range(h[4]):
        mesh = read(raw, h[12] + 80 * mesh_index, '8I12f')
        declaration = take(raw, h[14] + 48 * mesh[4], 48)
        vd = read(declaration, 0, '4I')
        if vd[1] > 16:
            raise ValueError('Weapon vertex declaration count')
        semantics = {declaration[16 + i] & 15: (declaration[16 + i] >> 4, declaration[32 + i]) for i in range(vd[1])}
        has_uv1 = semantics.get(9, (None,))[0] == 7 and semantics[9][1] + 4 <= vd[2]
        has_color = semantics.get(3, (None,))[0] == 8 and semantics[3][1] + 4 <= vd[2]
        has_normal = semantics.get(2, (None,))[0] == 10 and semantics[2][1] + 4 <= vd[2]
        uv1_semantic = 'MDN semantic 9 half2' if has_uv1 else 'UV0 copy; semantic 9 absent or unsupported'
        attributes = dict(uv1=uv1_semantic, color0='MDN semantic 3 RGBA8' if has_color else 'Native (0,0,0,1); COLOR0 absent or unsupported', normal='MDN semantic 2 RSX CMP 11/11/10, normalized' if has_normal else 'Native geometry triangle-normal fallback', tangent='No native tangent storage; derivative TBN remains a Windows approximation')
        vb = take(raw, h[18] + vd[3], mesh[6] * vd[2])
        for vi in range(mesh[6]):
            v = vertices[vertex_base + vi]
            at = vi * vd[2]
            if tuple(v[:3]) != read(vb, at + semantics[0][1], '3f'):
                raise ValueError('Weapon geometry vertex traversal changed')
            color = tuple((c / 255.0 for c in take(vb, at + semantics[3][1], 4))) if has_color else (0.0, 0.0, 0.0, 1.0)
            uv1 = read(vb, at + semantics[9][1], '2e') if has_uv1 else v[6:8]
            normal = v[3:6]
            if has_normal:
                packed = read(vb, at + semantics[2][1], 'I')[0]
                from extract_stage_normals import normal as rsx_normal
                normal = rsx_normal(packed)
                length = math.sqrt(sum((n * n for n in normal)))
                normal = [n / length for n in normal] if length > 1e-12 else v[3:6]
            vertex = (*v[:3], *normal, *v[6:8], *color, *uv1)
            if not all((math.isfinite(x) for x in vertex)):
                raise ValueError('Nonfinite weapon vertex')
            restored_vertices.append(vertex)
        for face_index in range(mesh[3], mesh[3] + mesh[2]):
            face = read(raw, h[13] + 16 * face_index, 'HHIIHH')
            local_indices = read(raw, h[20] + face[2], str(face[1]) + 'H')
            material = take(raw, h[15] + 112 * face[3], 112)
            words = read(material, 0, '12I')
            diffuse = read(raw, h[16] + 32 * words[4], 'I')[0]
            expected = (index_offset, len(local_indices), diffuse, 0, words[0])
            if part_index >= len(parts) or parts[part_index] != expected:
                raise ValueError('Weapon material/part traversal changed')
            if indices[index_offset:index_offset + len(local_indices)] != [vertex_base + i for i in local_indices]:
                raise ValueError('Weapon face/index traversal changed')
            textures = []
            missing = []
            for slot, reference_index in enumerate(words[4:4 + words[2]]):
                if reference_index >= h[8]:
                    raise ValueError('Weapon material texture reference extent')
                reference = take(raw, h[16] + 32 * reference_index, 32)
                image_index, provenance = original_texture(reference_index, reference)
                textures.append(dict(slot=slot, raw_reference=reference.hex(), embedded_image_index=image_index, provenance=dict(provenance, vertex_attributes=attributes)))
                if image_index == UNRESOLVED:
                    missing.append(f"slot{slot}: {provenance['fallback_reason']}")
            fallback = 'No reviewed model/package/VFP shader rule attached; retain existing diffuse rendering'
            if missing:
                fallback += '; ' + '; '.join(missing)
            original = material_record(material, declaration, face[3], source_context, textures, uv1_semantic, fallback)
            if material_profiles:
                original = apply_profile(original, material_profiles)
                if missing and original.get('profile_evidence'):
                    original['fallback_reason'] += '; ' + '; '.join(missing)
            materials.append(original)
            part_audit.append(dict(part=part_index, mesh=mesh_index, face=face_index, material=face[3], material_key=words[0], attributes=attributes, traversal='Matched geometry part and complete face indices'))
            part_index += 1
            index_offset += len(local_indices)
        vertex_base += mesh[6]
    if (vertex_base, part_index, index_offset) != (len(vertices), len(parts), len(indices)):
        raise ValueError('Weapon geometry traversal totals changed')
    out = bytearray(b'GWM1' + struct.pack('<5I6f', 3, len(vertices), len(indices), len(parts), len(native_images), *bounds))
    for v in restored_vertices:
        out += struct.pack('<14f', *v)
    out += struct.pack('<' + str(len(indices)) + 'I', *indices)
    for first, count, key, pattern, shader in parts:
        out += struct.pack('<5I3f', first, count, index[key], 0, shader, 1, 1, 1)
    for w, hh, code, data in native_images:
        out += struct.pack('<4I', w, hh, code, len(data)) + data
    out += encode_material_trailer(materials)
    destination.write_bytes(out)
    return dict(source=record(mdn), output=record(destination), vertices=len(vertices), triangles=len(indices) // 3, bounds=bounds, parts=len(parts), bones=[dict(index=i, id=k, parent=p, bind_position=xyz) for i, (k, p, xyz) in enumerate(bones)], texture_manifest=manifest_source, textures=sources, format_version=3, original_materials=materials, material_parts=part_audit, package_identity='Extraction-folder texture manifest and exact model SHA; original QAR identity not inferred', material_restore_status='Provisional source conversion; original shader parity and runtime attachment not validated', material_profile_status='Explicit profiles checked by source identity' if material_profiles else 'No model/package/VFP profile attached; MAT3 flags remain disabled', profile_attached_parts=sum((bool(m.get('profile_evidence')) for m in materials)))

def build(output, material_profiles=None):
    output.mkdir(parents=True, exist_ok=True)
    rows = []
    for wid, name, key, lod, secondary, constructor in WEAPONS:
        mdn = SOURCE / f'{key:06x}.mdn'
        cnp = SOURCE / f'{key:06x}.cnp'
        points = parse(cnp.read_bytes())
        for p in points:
            p['known_name'] = next((n for n in ['CNP_amp_def', 'CNP_mzf_def', 'CNP_fsp_def', 'CNP_rsp_def', 'CNP_lfp_def', 'CNP_rcp_def', 'CNP_smp_def'] if hash24(n) == p['id']), None)
        rows.append(dict(weapon_id=wid, name=name, constructor=hex(constructor), model_key=key, cnp_key=key, lod_key=lod, secondary_model_key=secondary, model=model(mdn, output / f'{name.lower()}.gwm', material_profiles), cnp=record(cnp), points=points, player_hand_bone=6029891, hand_binding_status='normal hand point 0x0B2A74; boneWorld * pointLocal -> weapon world; runtime not connected'))
        rows[-1]['secondary_model'] = model(SOURCE / f'{secondary:06x}.mdn', output / f'{name.lower()}_secondary.gwm', material_profiles)
    report = dict(schema=1, weapons=rows, tool=record(Path(__file__)), original_patch_used=False, source_db_saved=False, current_facts=['D2A2A8/D2A2B8 ID25 -> EC4F10; D2A040/D2A050 ID3 -> EBB528', 'Constructor primary/LOD/secondary/CNP keys passed to D3CB10 -> D36438 -> CD08F8', 'D69450 expands CNP quaternion and copies position; scale is not used there', 'D37940 composes parent matrix with attachment local matrix when parent pointer is present'], limitations=['Native static model adapter; original weapon bone animation and accessory visibility not restored', 'GWM3 preserves original material slots/coefficients and known vertex attributes; shader parity is not claimed', 'CNP child_id=1 is not a skeleton index; do not infer hand bone from it', 'Normal hand producer proved; active motion-layer/clip selection and MTP rotation interpolation not ported', 'CNP muzzle is model-local; actual firing-ray use of this point is not established'])
    motion = []
    for name in ('ak', 'oprt'):
        path = ROOT / 'work/player_motion/source' / f'sna_wp_{name}.mtar'
        motion.append(dict(name=name, source=record(path), clips=mtp_points(path.read_bytes())))
    (output / 'hand_points.json').write_text(json.dumps(dict(schema=1, archives=motion, normal_hand_point=731764, normal_hand_bone=6029891, normal_fallback=dict(position=[-148, -15.507800102233887, 23.65625], quaternion=[0.48445001244544983, -0.5843449831008911, -0.46867799758911133, 0.45186901092529297]), runtime_connected=False), indent=2, allow_nan=False), encoding='utf-8')
    (output / 'models.json').write_text(json.dumps(report, indent=2, ensure_ascii=False, allow_nan=False), encoding='utf-8')
    return report

class Tests(unittest.TestCase):

    def test_original_hand_rotation(self):
        source = ROOT / 'work/player_motion/source/sna_wp_oprt.mtar'
        point = animated_point(source.read_bytes(), 0)
        fallback = [0.48445001244544983, -0.5843449831008911, -0.46867799758911133, 0.45186901092529297]
        for actual, expected in zip(point['rotations'][0], fallback):
            self.assertAlmostEqual(actual, expected, delta=2e-06)
        self.assertEqual(point['bone'], 6029891)
        self.assertEqual(point['positions'][0], [-148.0, -15.5, 23.671875])
        self.assertEqual(point['translation_keys'][0][0], 0)
        self.assertEqual(point['rotation_keys'][0][0], 1)
        self.assertEqual(point['rotations'][0], point['rotations'][1])

    def test_shortest_arc_and_endpoints(self):
        q = [0, 0, 0, 1]
        for t in [0, 0.5, 1, 10]:
            self.assertEqual(abs(spherical([(0, q), (1, [0, 0, 0, -1])], t)[3]), 1)
        with self.assertRaises(ValueError):
            animated_point(b'bad', 0)

    def test_composition(self):
        identity = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
        parent = [0, 0, -1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 100, 200, 300, 1]
        local = identity.copy()
        local[12:15] = [10, 20, 30]
        self.assertEqual(compose(parent, identity), parent)
        self.assertEqual(compose(parent, local)[12:16], [130, 220, 290, 1])

    def test_rejection(self):
        with self.assertRaises(ValueError):
            compose([0] * 15, [0] * 16)
        with self.assertRaises(ValueError):
            compose([float('nan')] * 16, [0] * 16)

    def test_point_float(self):
        self.assertEqual(original_point_float(48274), -146.25)
        self.assertEqual(original_point_float(45596), -24.4375)
        self.assertEqual(original_point_float(13286), 31.59375)
        self.assertEqual(struct.pack('>f', original_point_float(32768)), b'\x80\x00\x00\x00')
        self.assertEqual(struct.unpack('>I', struct.pack('>f', original_point_float(1)))[0], 8192)

    def test_points_and_rejection(self):
        q = bytearray(struct.pack('>8I', 291, 0, 5, 0, 1, 11, 64, 0))
        q += struct.pack('>4I', 731764, 6029891, 24, 28)
        q += bytes.fromhex('bc92b21c33e60001') + bytes.fromhex('0001000000000000')
        b = bytearray(struct.pack('>I4H5I', 1918989389, 80, 4, 1, 1, 65536, 52, 52, 32, 36))
        b += struct.pack('>I4I', 6029891, 0, 0, 0, len(q)) + q
        result = mtp_points(b)[0]['points'][0]
        self.assertEqual(result['initial_position'], [-146.25, -24.4375, 31.59375])
        self.assertEqual(result['rotation_denominator'], 2047)
        self.assertIsNone(result['decoded_rotation'])
        for offset, value in [(0, 0), (52 + 24, 65), (52 + 36, 1), (52 + 40, 0), (52 + 44, 9999)]:
            bad = bytearray(b)
            struct.pack_into('>I', bad, offset, value)
            with self.assertRaises(ValueError):
                mtp_points(bad)
        with self.assertRaises(ValueError):
            mtp_points(b[:-1])
