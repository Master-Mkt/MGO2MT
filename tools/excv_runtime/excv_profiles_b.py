# Generated converter-only source; historical analysis entry points omitted.
"""Portable, exact-hash QQ diffuse dependency recovery from original n012a.

No workspace-specific paths, imported legacy scripts, network or source writes.
Only the explicit reviewed diffuse keys below may cross stage boundaries.
Other material channels and same-name guesses remain forbidden.
"""
from pathlib import Path
import struct
from excv_core import local_root, validate_output, read_package, file_record, atomic, save_json, sha
from title_assets import qar, txn, take, dlz, dld
BACKGROUND = frozenset({55386, 14888205, 13582263, 5170420})
OBJECTS = frozenset({4738884, 7892918, 12746736, 5122198, 7752189, 14790591, 13188113, 9969174, 5236945, 4621169, 14634767})
REVIEWED = BACKGROUND | OBJECTS

def recover_qq_diffuse(source_root, extracted_directory, wanted=None):
    """Return (int-key->Path overrides, provenance) for an extracted QQ folder.

    Call only for n022a. `wanted` should be the missing diffuse key set; default
    is the four reviewed background keys. Unreviewed keys are explicitly listed
    unresolved and never used to search unrelated sources.
    """
    root = local_root(source_root)
    destination = validate_output(root, Path(extracted_directory) / 'exact_diffuse_aliases')
    requested = set(BACKGROUND if wanted is None else wanted)
    if any((not isinstance(x, int) or isinstance(x, bool) or x < 0 or (x > 16777215) for x in requested)):
        raise ValueError('Invalid diffuse texture key')
    selected = requested & REVIEWED
    report = {'schema': 'MGO2MTEXCV.EXACT_DIFFUSE_DEPENDENCY.1', 'consumer_stage': 'n022a', 'donor_stage': 'n012a', 'requested': sorted(requested), 'unreviewed': sorted(requested - REVIEWED), 'images': {}, 'source_files': [], 'policy': 'Reviewed exact diffuse hash, TXN dimensions/archive/index, minimum DLD priority and byte equality; no geometry or other material channels substituted.'}
    if not selected:
        return ({}, report)
    donor = root / 'stage/n012a'
    if not donor.is_dir():
        report['missing_dependency'] = 'Original base stage/n012a directory is absent'
        return ({}, report)

    def read(name):
        path = donor / name
        before = file_record(path)
        data, mode, _ = read_package(path, 'n012a')
        if before != file_record(path):
            raise ValueError('Original dependency changed during read')
        report['source_files'].append({'source': before, 'mode': mode, 'decoded_sha256': sha(data)})
        return data
    q = read('cache.qar')
    references = {}
    for entry in qar(q):
        if not entry['name'].endswith('.txn'):
            continue
        raw = take(q, entry['offset'], entry['size'])
        table = txn(raw)
        for x in table['textures']:
            key = x['key']
            if key not in selected:
                continue
            if key in references:
                raise ValueError('Ambiguous exact donor TXN hash: ' + hex(key))
            im = table['images'][x['image_index']]
            if (x['width'], x['height']) != (im['width'], im['height']) or x['x'] or x['y'] or (x['uv_scale'] != [1, 1]) or (x['uv_offset'] != [0, 0]) or (im['codec'] not in (9, 11)) or (im['flags'] & 15 != 1):
                raise ValueError('Unsupported reviewed donor texture layout: ' + hex(key))
            references[key] = {'txn_entry': entry, 'txn_sha256': sha(raw), 'texture': x, 'image': im}
    candidates = {k: [] for k in references}
    for name in ('cache.dlz', 'cache_nodld.dlz', 'cache_d.dlz'):
        if not (donor / name).is_file():
            continue
        decoded, _ = dlz(read(name))
        records = dld(decoded)
        for key, ref in references.items():
            x = ref['texture']
            im = ref['image']
            size = (im['width'] + 3) // 4 * ((im['height'] + 3) // 4) * (8 if im['codec'] == 9 else 16)
            for r in records:
                if r['key'] == x['archive'] and r['index'] == x['index'] and (r['parent_size'] == 0) and (r['size'] >= size):
                    candidates[key].append((r['priority'], take(decoded, r['offset'] + 32, size), name, r))
    overrides = {}
    report['unresolved'] = []
    for key in sorted(selected):
        matches = candidates.get(key, [])
        if not matches:
            report['unresolved'].append(key)
            continue
        priority, payload, name, record = min(matches, key=lambda x: x[0])
        if any((p != payload for pr, p, _, _ in matches if pr == priority)):
            raise ValueError('Ambiguous exact donor DLD payload: ' + hex(key))
        ref = references[key]
        im = ref['image']
        path = destination / f'{key:06x}.bin'
        atomic(path, struct.pack('<4I', im['width'], im['height'], im['codec'], len(payload)) + payload)
        overrides[key] = path
        report['images'][str(key)] = {**ref, 'dld_file': name, 'dld_record': record, 'payload_sha256': sha(payload), 'output': file_record(path)}
    report['source_unchanged'] = True
    save_json(destination / 'provenance.json', report)
    return (overrides, report)
SHARED_DIFFUSE = {'n024a': {'n014a': (7301622, 10655348, 10385707)}, 'n023a': {'n014a': (4884873, 10655348, 10385707), 'n024a': (7618551, 13744450)}}

def recover_shared_diffuse(stage, source_root, extracted_directory):
    import re
    from excv_ui_icon import priority_remaps
    root = local_root(source_root)
    dest = validate_output(root, Path(extracted_directory) / 'shared_diffuse_aliases')
    report = dict(schema='MGO2MTEXCV.EXACT_DIFFUSE_DEPENDENCY.1', consumer_stage=stage, images={}, source_files=[], unresolved=[], policy='Explicit native cross-stage dependency recipe using identical original diffuse hash; original cross-stage loading is not claimed.')
    overrides = {}
    for donor, keys in SHARED_DIFFUSE.get(stage, {}).items():
        folder = root / ('dl/p/stage/n014a' if donor == 'n014a' else 'stage/n024a')
        if not folder.is_dir():
            report['unresolved'].extend(keys)
            continue

        def read(path):
            before = file_record(path)
            data, mode, _ = read_package(path, donor)
            if before != file_record(path):
                raise ValueError('Shared original dependency changed')
            report['source_files'].append(dict(source=before, mode=mode, decoded_sha256=sha(data)))
            return data
        raw = read(folder / 'cache.qar')
        refs = {}
        for e in qar(raw):
            if not e['name'].endswith('.txn'):
                continue
            b = take(raw, e['offset'], e['size'])
            t = txn(b)
            for x in t['textures']:
                if x['key'] not in keys:
                    continue
                im = t['images'][x['image_index']]
                if (x['width'], x['height']) != (im['width'], im['height']) or x['x'] or x['y'] or (x['uv_scale'] != [1, 1]) or (x['uv_offset'] != [0, 0]) or (im['codec'] not in (9, 11)) or (im['flags'] & 15 != 1):
                    raise ValueError('Unsupported shared diffuse shape')
                if x['key'] in refs:
                    raise ValueError('Duplicate shared diffuse TXN key')
                refs[x['key']] = dict(donor_stage=donor, txn_entry=e, txn_sha256=sha(b), texture=x, image=im)
        maps = {}
        for p in sorted(folder.glob('*.dci')):
            maps[p.stem] = priority_remaps(read(p))
        candidates = {k: [] for k in keys}
        for p in sorted(folder.glob('cache*.dlz')):
            b, _ = dlz(read(p))
            family = re.sub('_\\d{6}$', '', p.stem)
            for r in dld(b):
                if r['parent_size']:
                    continue
                mapping = maps.get(family, {}).get((r['key'], r['priority']))
                index = mapping['mapping'].get(r['index'], 65535) if mapping else r['index']
                if index == 65535:
                    continue
                for key, ref in refs.items():
                    x = ref['texture']
                    im = ref['image']
                    size = (im['width'] + 3) // 4 * ((im['height'] + 3) // 4) * (8 if im['codec'] == 9 else 16)
                    if r['key'] == x['archive'] and index == x['index'] and (r['size'] >= size):
                        candidates[key].append((r['priority'], take(b, r['offset'] + 32, size), p.name, {**r, 'resolved_index': index, 'dci_family': family if mapping else None}))
        for key in keys:
            found = candidates[key]
            if not found:
                report['unresolved'].append(key)
                continue
            priority, payload, name, r = min(found, key=lambda x: x[0])
            if any((pr == priority and p != payload for pr, p, _, _ in found)):
                raise ValueError('Ambiguous shared diffuse payload: ' + hex(key))
            ref = refs[key]
            im = ref['image']
            path = dest / f'{key:06x}.bin'
            atomic(path, struct.pack('<4I', im['width'], im['height'], im['codec'], len(payload)) + payload)
            overrides[key] = path
            report['images'][str(key)] = {**ref, 'dld_file': name, 'dld_record': r, 'payload_sha256': sha(payload), 'output': file_record(path)}
    report['source_unchanged'] = True
    save_json(dest / 'provenance.json', report)
    return (overrides, report)
