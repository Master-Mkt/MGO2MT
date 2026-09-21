# Generated converter-only source; historical analysis entry points omitted.
"""Portable local-original weapon/model/hand recipe. No workspace inputs.

The bundled profile contains numeric selectors and SHA identities only. All
model, texture and motion payloads must be recovered from the selected source.
Complete character appearance/slot reconstruction remains explicitly missing.
"""
from pathlib import Path
import argparse, hashlib, json, struct
from excv_core import local_root, validate_output, read_package, file_record, save_json, atomic
from title_assets import take, dlz, dld, txn, read
from character_motion import load as motion_load
from weapon_attachment import animated_point
import weapon_attachment
from weapon_connect_points import parse, hash24
from prepare_gekko_model import dds_top

def sha(b):
    return hashlib.sha256(b).hexdigest()

def extract_resources(source, output, profile, progress, cancel=lambda: False):
    wanted = {r['sha256']: r for r in profile['resources']}
    found = {}
    sources = []
    errors = []
    for relative in ('stage/r_sna01_n', 'dl/p/stage/r_sna01_n'):
        folder = source / relative
        if not folder.is_dir():
            continue
        for path in sorted(folder.iterdir()):
            if cancel():
                raise InterruptedError('Actor conversion cancelled')
            if not path.is_file() or path.suffix not in ('.dar', '.qar', '.dlz'):
                continue
            progress('actor / ' + relative + '/' + path.name)
            before = file_record(path)
            try:
                data, mode, entries = read_package(path, 'r_sna01_n')
                candidates = [(data, path.suffix)]
                if entries is not None:
                    candidates = []
                    for entry in entries:
                        suffix = Path(entry['name']).suffix
                        if suffix not in ('.mdn', '.cnp', '.txn', '.mtar', '.mtsq'):
                            continue
                        offset = entry['offset']
                        offset = int(offset, 0) if isinstance(offset, str) else offset
                        candidates.append((take(data, offset, entry['size']), suffix))
                recovered = []
                for raw, suffix in candidates:
                    digest = sha(raw)
                    if digest not in wanted:
                        continue
                    expected = wanted[digest]
                    if len(raw) != expected['size'] or suffix != expected['suffix']:
                        raise ValueError('Profile resource type/extent mismatch')
                    target = output / (digest + suffix)
                    if not target.exists():
                        atomic(target, raw)
                    if sha(target.read_bytes()) != digest:
                        raise ValueError('Generated source object changed')
                    found[digest] = target
                    recovered.append(digest)
                if file_record(path) != before:
                    raise ValueError('Original resource changed during conversion')
                sources.append({'source': before, 'decoded_sha256': sha(data), 'mode': mode, 'recovered': recovered})
            except (ValueError, OSError, KeyError, IndexError, struct.error) as exc:
                errors.append({'source': before, 'error': str(exc)})
    return (found, sources, errors)

def build_hands(resources, profile, destination):
    decoded = {}
    points = {}

    def source(digest):
        return resources[digest].read_bytes()

    def body(digest, index):
        key = (digest, index)
        if key not in decoded:
            decoded[key] = motion_load(source(digest), index)
        return decoded[key]

    def point(digest, index, key=731764):
        cache = (digest, index, key)
        if cache not in points:
            points[cache] = animated_point(source(digest), index, key)
        return points[cache]
    fallback = point(profile['point_fallback_source_sha256'], profile['point_fallback_index'])

    def plain(row, index, stored):
        digest = row['source_sha256']
        m = body(digest, index)
        frames = m['frames']
        try:
            p = point(digest, index)
        except ValueError:
            p = {'bone': fallback['bone'], 'positions': [fallback['positions'][0]] * (frames + 1), 'rotations': [fallback['rotations'][0]] * (frames + 1)}
        try:
            mag = point(digest, index, 731381)
        except ValueError:
            mag = None
        samples = [(*m['roots'][f], *p['positions'][f], *p['rotations'][f]) for f in range(frames + 1)]
        return ([row['weapon'], stored, m['name'], frames, 60, m['root_bone'], p['bone'], len(m['tracks'])], samples, m['tracks'], mag)
    chunks = []
    for row in sorted(profile['clips'], key=lambda r: (r['weapon'], r['index'])):
        if row.get('source_indices'):
            parts = [plain(row, i, i) for i in row['source_indices']]
            h = list(parts[0][0])
            h[1] = row['index']
            h[3] = sum((p[0][3] for p in parts))
            if any((p[0][5:] != parts[0][0][5:] or p[2].keys() != parts[0][2].keys() for p in parts)):
                raise ValueError('Concatenation source contract changed')
            samples = []
            tracks = {k: [] for k in parts[0][2]}
            for n, p in enumerate(parts):
                final = n == len(parts) - 1
                samples.extend(p[1] if final else p[1][:-1])
                for key in tracks:
                    tracks[key].extend(p[2][key] if final else p[2][key][:-1])
            mag = None
        else:
            h, samples, tracks, mag = plain(row, row['source_index'], row['index'])
        if (h[2], h[3], bool(mag)) != (row['key'], row['frames'], row['magazine_track']):
            raise ValueError('Original hand clip profile mismatch')
        b = bytearray(struct.pack('<8I', *h))
        for frame in samples:
            b += struct.pack('<10f', *frame)
        for key, tr in sorted(tracks.items()):
            b += struct.pack('<I', key)
            for q in tr:
                b += struct.pack('<4f', *q)
        b += struct.pack('<I', mag['bone'] if mag else 0)
        if mag:
            for p, q in zip(mag['positions'], mag['rotations']):
                b += struct.pack('<7f', *p, *q)
        chunks.append(b)
    b = bytearray(b'GWH1' + struct.pack('<2I', 3, len(chunks)))
    for chunk in chunks:
        b += chunk
    b += struct.pack('<I', len(profile['selections']))
    for r in profile['selections']:
        b += struct.pack('<16I', r['weapon'], *r['hold'], *r['aim'], *r['reload'], *r['fire'], *r['cqc'])
    if len(b) != profile['expected_hands_size'] or sha(b) != profile['expected_hands_sha256']:
        raise ValueError('Regenerated GWH differs from reviewed recipe output')
    atomic(destination, b)
    return file_record(destination)

def dds(width, height, codec, payload, count):
    if codec not in (9, 11):
        raise ValueError('Actor DDS codec unsupported')
    top = (width + 3) // 4 * ((height + 3) // 4) * (8 if codec == 9 else 16)
    h = bytearray(128)
    h[:4] = b'DDS '
    struct.pack_into('<7I', h, 4, 124, 528391 | (131072 if count > 1 else 0), height, width, top, 0, count)
    struct.pack_into('<2I4s', h, 76, 32, 4, b'DXT1' if codec == 9 else b'DXT5')
    struct.pack_into('<I', h, 108, 4096 | (4194312 if count > 1 else 0))
    return bytes(h) + payload

def build_model(resources, row, workspace, destination, containers):
    workspace.mkdir(parents=True, exist_ok=True)
    texture_folder = workspace / 'textures'
    texture_folder.mkdir(exist_ok=True)
    raw = resources[row['mdn_sha256']].read_bytes()
    mdn = workspace / (row['mdn_sha256'] + '.mdn')
    atomic(mdn, raw)
    textures = []
    sources = []
    for t in row['textures']:
        txn_raw = resources[t['txn_sha256']].read_bytes()
        tx = txn(txn_raw)
        if t['txn_texture'] not in tx['textures'] or tx['images'][t['txn_texture']['image_index']] != t['txn_image']:
            raise ValueError('Original actor TXN descriptor changed')
        txn_path = workspace / (t['txn_sha256'] + '.txn')
        atomic(txn_path, txn_raw)
        sources.append(file_record(txn_path))
        parts = []
        for s in t['payloads']:
            data = resources[s['source_sha256']].read_bytes()
            if 'record' in s:
                digest = s['source_sha256']
                if digest not in containers:
                    plain, _ = dlz(data) if data[:4] == b'segs' else (data, [])
                    containers[digest] = (plain, dld(plain))
                plain, records = containers[digest]
                r = s['record']
                if r not in records:
                    raise ValueError('Original actor DLD record changed')
                payload = take(plain, r['offset'] + 32, r['size'])
                offset = r['parent_size']
            else:
                payload = take(data, s['offset'], s['size'])
                offset = 0
            if sha(payload) != s['payload_sha256']:
                raise ValueError('Original actor texture payload changed')
            parts.append((offset, payload))
        payload = bytearray()
        for offset, part in sorted(parts):
            if offset != len(payload):
                raise ValueError('Original actor mip payload gap/overlap')
            payload += part
        image = dds(t['width'], t['height'], t['codec'], payload, t['mip_count'])
        if sha(image) != t['dds_sha256'] or sha(dds_top(image)[3]) != t['top_sha256']:
            raise ValueError('Actor DDS recipe mismatch')
        target = texture_folder / f"{t['key']:06x}.dds"
        atomic(target, image)
        textures.append({'texture_key': t['key'], 'source_txn': str(txn_path.resolve()), 'txn_texture': t['txn_texture'], 'txn_image': t['txn_image'], 'dds': file_record(target)})
    h = read(raw, 0, '24I')
    by_key = {t['texture_key']: t for t in textures}
    slots = []
    for i in range(h[8]):
        key = read(raw, h[16] + 32 * i, 'I')[0]
        t = by_key.get(key)
        slots.append({'mdn_texture_index': i, 'texture_key': key, 'status': 'resolved_same_folder' if t else 'missing_same_folder', 'dds_path': t['dds']['path'] if t else None, 'dds_sha256': t['dds']['sha256'] if t else None, 'texture_source_txn': t['source_txn'] if t else None})
    manifest = {'source_folder': str(workspace.resolve()), 'sources': sources, 'textures': textures, 'models': [{'source_mdn': file_record(mdn), 'texture_slots': slots}]}
    save_json(texture_folder / 'texture_manifest.json', manifest)
    previous = weapon_attachment.TEXTURES
    try:
        weapon_attachment.TEXTURES = texture_folder
        result = weapon_attachment.model(mdn, destination)
    finally:
        weapon_attachment.TEXTURES = previous
    if (result['vertices'], result['triangles']) != (row['vertices'], row['triangles']):
        raise ValueError('Actor model geometry differs from recipe')
    return result

def verify_actor_resources(output, source_root=None, profile_path=None):
    output = Path(output).resolve()
    errors = []
    assets_checked = 0
    sources_checked = 0
    try:
        report = json.loads((output / 'actor-result.json').read_text(encoding='utf-8'))
        path = Path(profile_path) if profile_path else Path(__file__).with_name('excv_actor_profile.json')
        profile = json.loads(path.read_text(encoding='utf-8'))
        if report.get('format') != 'MGO2MTEXCV.ACTOR_RESULT.1' or not report.get('weapon_complete') or report.get('source_errors'):
            raise ValueError('Only completed, error-free weapon results can be resumed')
        if file_record(path)['sha256'] != report['profile']['sha256']:
            raise ValueError('Actor recipe changed')
        expected = {'weapons/hands.gwh', 'weapons/models.gwi'}
        for r in profile['models']:
            expected.add(f"weapons/id_{r['weapon']:03d}.gwm")
            if r.get('secondary'):
                expected.add(f"weapons/id_{r['weapon']:03d}_secondary.gwm")
        if {r['path'] for r in report['assets']} != expected or len(report['assets']) != len(expected):
            raise ValueError('Required actor asset inventory differs')
        if {p.relative_to(output / 'data').as_posix() for p in (output / 'data').rglob('*') if p.is_file()} != expected:
            raise ValueError('Actor data directory inventory differs')
        for r in report['assets']:
            actual = file_record(output / 'data' / r['path'])
            if (actual['size'], actual['sha256']) != (r['size'], r['sha256']):
                raise ValueError('Actor asset changed: ' + r['path'])
            assets_checked += 1
        hands = file_record(output / 'data/weapons/hands.gwh')
        if (hands['size'], hands['sha256']) != (profile['expected_hands_size'], profile['expected_hands_sha256']):
            raise ValueError('Hand bank no longer matches source recipe')
        root = local_root(source_root if source_root is not None else report['source'])
        old = Path(report['source'])
        seen = set()
        for row in report['sources']:
            r = row['source']
            relative = Path(r['path']).relative_to(old)
            path = (root / relative).resolve()
            if not path.is_relative_to(root):
                raise ValueError('Source path leaves selected root')
            actual = file_record(path)
            if (actual['size'], actual['sha256']) != (r['size'], r['sha256']):
                raise ValueError('Original source changed: ' + relative.as_posix())
            seen.add(relative.as_posix())
            sources_checked += 1
        current = {p.relative_to(root).as_posix() for relative in ('stage/r_sna01_n', 'dl/p/stage/r_sna01_n') for p in (root / relative).glob('*') if p.is_file() and p.suffix in ('.dar', '.qar', '.dlz')}
        if current != seen:
            raise ValueError('Original source inventory changed')
    except (OSError, ValueError, KeyError, TypeError) as exc:
        errors.append(str(exc))
    return {'passed': not errors, 'assets_checked': assets_checked, 'sources_checked': sources_checked, 'errors': errors}

def build_actor_resources(source_root, output, profile_path=None, progress=lambda _: None, cancel=lambda: False, resume=False):
    source = local_root(source_root)
    output = validate_output(source, output)
    if output.exists() and any(output.iterdir()):
        if not resume:
            raise ValueError('Actor output must be a new empty folder')
        checked = verify_actor_resources(output, source, profile_path)
        if not checked['passed']:
            raise ValueError('Cannot resume actor output: ' + '; '.join(checked['errors']))
        result = json.loads((output / 'actor-result.json').read_text(encoding='utf-8'))
        result['resumed'] = True
        result['verification'] = checked
        return result
    output.mkdir(parents=True, exist_ok=True)
    path = Path(profile_path) if profile_path else Path(__file__).with_name('excv_actor_profile.json')
    profile = json.loads(path.read_text(encoding='utf-8'))
    if profile.get('schema') != 'MGO2MTEXCV.ACTOR_RECIPE.1':
        raise ValueError('Actor recipe version')
    resources, sources, errors = extract_resources(source, output / 'source', profile, progress, cancel)
    missing = [{'kind': 'original_resource', **r} for r in profile['resources'] if r['sha256'] not in resources]
    models = []
    hands = None
    target = output / 'data/weapons'
    target.mkdir(parents=True, exist_ok=True)
    containers = {}
    try:
        hands = build_hands(resources, profile, target / 'hands.gwh')
    except (KeyError, ValueError, OSError) as exc:
        missing.append({'kind': 'hands.gwh', 'reason': str(exc)})
    for r in profile['models']:
        if cancel():
            raise InterruptedError('Actor conversion cancelled')
        progress('weapon / ' + str(r['weapon']))
        try:
            primary = build_model(resources, r['primary'], output / 'working' / str(r['weapon']) / 'primary', target / f"id_{r['weapon']:03d}.gwm", containers)
            secondary = build_model(resources, r['secondary'], output / 'working' / str(r['weapon']) / 'secondary', target / f"id_{r['weapon']:03d}_secondary.gwm", containers) if r.get('secondary') else None
            points = parse(resources[r['cnp_sha256']].read_bytes()) if r.get('cnp_sha256') else []
            points = {p['id']: p for p in points}
            muzzle = points.get(hash24('CNP_mzf_def'))
            magazine = points.get(hash24('CNP_amp_def'))
            if secondary and (not magazine):
                raise ValueError('Original secondary CNP is absent')
            models.append({'weapon': r['weapon'], 'primary': primary, 'secondary': secondary, 'muzzle': muzzle, 'magazine': magazine})
        except (KeyError, ValueError, OSError, struct.error) as exc:
            missing.append({'kind': 'weapon_model', 'weapon': r['weapon'], 'reason': str(exc)})
    if models:
        b = bytearray(b'GWI1' + struct.pack('<2I', 1, len(models)))
        for r in models:
            m, g = (r['muzzle'], r['magazine'])
            b += struct.pack('<2I10f', r['weapon'], int(bool(m)) | 2 * int(bool(r['secondary'])), *(m['position'][:3] if m else [0, 0, 0]), *(g['position'][:3] if g else [0, 0, 0]), *(g['quaternion'] if g else [0, 0, 0, 1]))
        atomic(target / 'models.gwi', b)
    weapon_complete = not missing and hands is not None and (len(models) == len(profile['models']))
    missing.extend([{'kind': 'character/appearance.gwc', 'reason': 'Full numeric appearance tables, slot package extraction and family-local DCI remap recipe not yet integrated'}, {'kind': 'character/player.gwmot', 'reason': 'Full locomotion/action bank selection recipe not yet integrated'}])
    missing.extend(({'kind': 'character/' + name, 'reason': 'Separate character action/selection bank recipe not yet integrated'} for name in ('cover.gwmot', 'evade.gwmot', 'selection0.gwmot', 'selection1.gwmot', 'special_male.gwmot')))
    report = {'format': 'MGO2MTEXCV.ACTOR_RESULT.1', 'source': str(source), 'profile': file_record(path), 'weapon_complete': weapon_complete, 'complete_actor_set': False, 'hands': hands, 'model_count': len(models), 'models': models, 'missing': missing, 'source_errors': errors, 'sources': sources, 'assets': [{'path': p.relative_to(output / 'data').as_posix(), **{k: v for k, v in file_record(p).items() if k != 'path'}} for p in sorted((output / 'data').rglob('*')) if p.is_file()], 'native_boundaries': ['Source clips and hashes are verified, but original complete action dispatcher is not reproduced', 'CQC/HG family/normal aim selection and M4/M870/RPG reload presentation remain native adapters', 'Unknown extra material sampler dimensions remain unresolved; original source bytes are preserved']}
    save_json(output / 'actor-result.json', report)
    return report
