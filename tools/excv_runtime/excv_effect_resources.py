# Generated converter-only source; historical analysis entry points omitted.
"""Generate reviewed original combat/weather GWFX from user-selected originals.

No asset I/O on import; no old work files, IDA installation, or original payload
embedding. CPEF texture-key occurrences are evidence, not a recovered particle VM.
"""
from pathlib import Path
import argparse, json, struct
from excv_core import atomic, save_json, sha, file_record, local_root, validate_output
from excv_ui_resources import load_package
from title_assets import txn, take
DECODED = {'cache.dar': 'bd1ecdb45cfdb449787d0b76c4e38dc30a51c493ce593bd437badaea2c4e0d1a', 'cache.qar': '1a81ca2e13609955275a2c40fa016bc77b9379556aa069c9eee0d7b2593a1e4b', 'cache.dlz': '357a16df084eb51f9d088b7bbb7e00c4eb9486af78c6e68ae67f1cb5f8c9272e', 'cache_d.dlz': '5cd8f9113804e1590eb2526d2ab0d8d421e1f4eaa710567eb3152e75c4405b62'}
TXN_SHA = 'c644d33be80daf0aa8afeccf629d63062a1f31d1a7876491dbe726a72ec2dc91'
WEATHER = (13868504, 367458, 12113819)
BUNDLES = {'original': (26, 860756, 'd898b652f3e0de90e14e7f69bf7f9612e84aed7b7c3eb89b2c96610ff6d5404c'), 'damage': (10, 346324, 'fc86575cd1e2a6cdfc5b7422483d8c53222fad7f1ddb8e8431a87d07f591b6b8'), 'weather': (3, 21576, '85657a8d32ad2626932518e6e20718df58e682ec78cd0af146f8115478893e47')}
LIMITATIONS = ['Reviewed original n022a base effect profile; patch replacements require a separate profile review.', 'Combat image selection uses exact aligned key occurrences in 71 named original CPEF records, not a complete CPEF disassembly.', 'Original compressed top-mip image bytes are unchanged; CPEF particle VM, emitter sizes/lifetimes/curves and original composition remain unimplemented.', 'Weapon-family effect assignment and weather particle positions, lifetime, motion and tint are existing native adapter behavior.', 'Blood image references come from 12 named original CPEF records. Normal/large blood role assignment, red tint and emission timing are native adapter choices.', 'MDN geometry is not a dependency of these three image bundles. This does not reconstruct effect meshes or original particle simulation.', 'Audio/GWA conversion is outside this module.']

def checked_package(root, name):
    data, info, proof = load_package(root, root / 'stage/n022a' / name, 'n022a', {})
    if sha(data) != DECODED[name]:
        raise ValueError('Reviewed original package differs: ' + name)
    proof['reviewed_decoded_sha_verified'] = True
    return (data, info, proof)

def cpef_keys(data, entries, keys, prefixes=('ef_mzf_', 'ef_bak_')):
    selected = []
    wanted = set()
    for entry in entries:
        if not entry['name'].startswith(prefixes) or not entry['name'].endswith('.cpef'):
            continue
        offset = entry['offset']
        raw = take(data, int(offset, 0) if isinstance(offset, str) else offset, entry['size'])
        refs = [dict(offset=i, key=struct.unpack_from('>I', raw, i)[0]) for i in range(0, len(raw) - 3, 4) if struct.unpack_from('>I', raw, i)[0] in keys]
        wanted.update((r['key'] for r in refs))
        selected.append(dict(entry=entry, sha256=sha(raw), texture_word_matches=refs))
    return (wanted, selected)

def texture_record(key, table):
    candidates = [t for t in table['textures'] if t['key'] == key]
    if len(candidates) != 1:
        raise ValueError('Missing/ambiguous effect texture key')
    tex = candidates[0]
    image = table['images'][tex['image_index']]
    if image['flags'] & 15 != 1 or image['codec'] not in (9, 11) or tex['x'] or tex['y'] or (tex['uv_scale'] != [1, 1]) or (tex['uv_offset'] != [0, 0]):
        raise ValueError('Unsupported reviewed effect image layout')
    if not 0 < image['width'] <= 8192 or not 0 < image['height'] <= 8192:
        raise ValueError('Effect image dimensions')
    if tex['index'] != tex['image_index']:
        raise ValueError('Unreviewed effect image mapping')
    size = (image['width'] + 3) // 4 * ((image['height'] + 3) // 4) * {9: 8, 11: 16}[image['codec']]
    return (tex, image, size)

def select_payload(current, payload, proof, priority):
    if current is None or priority < current[0]:
        return (priority, payload, proof)
    if priority == current[0] and payload != current[1]:
        raise ValueError('Ambiguous original DLD blocks')
    return current

def make_bundle(keys, table, payloads):
    data = bytearray(b'GWFX' + struct.pack('<II', 1, len(keys)))
    images = []
    for key in keys:
        tex, img, size = texture_record(key, table)
        if key not in payloads:
            raise ValueError(f'Missing original DLD image {key:06x}')
        _, payload, proof = payloads[key]
        if len(payload) != size:
            raise ValueError('Original image block extent')
        data.extend(struct.pack('<5I', key, img['width'], img['height'], img['codec'], size))
        data.extend(payload)
        images.append(dict(key=key, txn=tex, image=img, source=proof, payload_sha256=sha(payload)))
    return (bytes(data), images)

def verify_effect_resources(destination, source_root=None):
    destination = Path(destination).resolve(strict=True)
    report = json.loads((destination / 'manifest.json').read_text(encoding='utf-8'))
    if report.get('format') != 'MGO2MTEXCV.EFFECT_RESOURCES.1' or not report.get('complete'):
        raise ValueError('Incomplete effect conversion')
    for row in report['files']:
        path = destination / row['path']
        if destination not in path.resolve().parents or path.is_symlink():
            raise ValueError('Unsafe output path')
        actual = file_record(path)
        if (actual['size'], actual['sha256']) != (row['size'], row['sha256']):
            raise ValueError('Output changed: ' + row['path'])
    if source_root is not None:
        root = local_root(source_root)
        for row in report['sources']:
            path = root / row['relative']
            if root not in path.resolve().parents:
                raise ValueError('Unsafe original source path')
            actual = file_record(path)
            if (actual['size'], actual['sha256']) != (row['source']['size'], row['source']['sha256']):
                raise ValueError('Original source changed')
    return report

def extract_effect_resources(source_root, destination, *, compare_data=None, progress=lambda _: None, cancel=lambda: False, resume=False):
    """Own NEW destination/data/fx plus manifest; completed resume verifies hashes.

    compare_data is an optional read-only existing runtime data directory.
    Partial results remain complete:false and are never silently reused.
    """
    selected = Path(source_root).resolve(strict=True)
    root = local_root(selected)
    destination = validate_output(selected, Path(destination))
    validate_output(root, destination)
    compare = Path(compare_data).resolve(strict=True) if compare_data is not None else None
    if destination.exists():
        if resume:
            report = verify_effect_resources(destination, root)
            if report.get('profile_revision', 1) < 2:
                raise ValueError('Saved effect conversion predates blood resources; select a new destination')
            return report
        raise ValueError('Select a new effects conversion destination')
    destination.mkdir(parents=True)
    report = dict(format='MGO2MTEXCV.EFFECT_RESOURCES.1', profile_revision=2, complete=False, source_root=str(root), sources=[], assets=[], files=[], limitations=LIMITATIONS)
    try:
        if cancel():
            raise InterruptedError('Conversion cancelled')
        progress('Original effects / cache.qar')
        qb, entries, proof = checked_package(root, 'cache.qar')
        report['sources'].append(proof)
        matches = [e for e in entries if e['name'] == 'effect.txn']
        if len(matches) != 1:
            raise ValueError('Missing/ambiguous original effect.txn')
        entry = matches[0]
        tb = take(qb, entry['offset'], entry['size'])
        if sha(tb) != TXN_SHA:
            raise ValueError('Reviewed effect.txn differs')
        table = txn(tb)
        report['txn'] = dict(entry=entry, sha256=sha(tb))
        del qb
        if cancel():
            raise InterruptedError('Conversion cancelled')
        progress('Original effects / cache.dar')
        db, entries, proof = checked_package(root, 'cache.dar')
        report['sources'].append(proof)
        keys, cpef = cpef_keys(db, entries, {t['key'] for t in table['textures']})
        blood_keys, blood_cpef = cpef_keys(db, entries, {t['key'] for t in table['textures']}, ('ef_bld_',))
        del db
        if len(keys) != 26 or len(cpef) != 71:
            raise ValueError('Reviewed CPEF selection differs')
        if len(blood_keys) != 10 or len(blood_cpef) != 12:
            raise ValueError('Reviewed blood CPEF selection differs')
        report['cpef'] = cpef
        report['damage_cpef'] = blood_cpef
        needed = {key: texture_record(key, table) for key in keys | blood_keys | set(WEATHER)}
        payloads = {}
        for name in ('cache.dlz', 'cache_d.dlz'):
            if cancel():
                raise InterruptedError('Conversion cancelled')
            progress('Original effects / ' + name)
            data, records, proof = checked_package(root, name)
            report['sources'].append(proof)
            for key, (tex, img, size) in needed.items():
                for row in records:
                    if row['key'] != tex['archive'] or row['index'] != tex['image_index'] or row['parent_size'] or (row['size'] < size):
                        continue
                    payload = take(data, row['offset'] + 32, size)
                    payloads[key] = select_payload(payloads.get(key), payload, dict(package=proof, record=row), row['priority'])
            del data
        for role, selected_keys in (('original', sorted(keys)), ('weather', WEATHER), ('damage', sorted(blood_keys))):
            if cancel():
                raise InterruptedError('Conversion cancelled')
            data, images = make_bundle(selected_keys, table, payloads)
            count, size, digest = BUNDLES[role]
            if (len(images), len(data), sha(data)) != (count, size, digest):
                raise ValueError('Reviewed bundle differs: ' + role)
            name = f'fx/{role}.gwfx'
            atomic(destination / 'data' / name, data)
            report['assets'].append(dict(path=name, size=len(data), sha256=sha(data), images=images))
        report['complete'] = True
    finally:
        for path in sorted(destination.rglob('*')):
            if not path.is_file() or path.name == 'manifest.json':
                continue
            row = file_record(path)
            row['path'] = path.relative_to(destination).as_posix()
            if compare is not None and row['path'].startswith('data/'):
                other = compare / row['path'][5:]
                row['comparison'] = dict(path=str(other), exists=other.is_file())
                if other.is_file():
                    row['comparison'].update(file_record(other), equal=sha(other.read_bytes()) == row['sha256'])
            report['files'].append(row)
        save_json(destination / 'manifest.json', report)
    return report
