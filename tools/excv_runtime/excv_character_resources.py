# Generated converter-only source; historical analysis entry points omitted.
"""Rebuild seven character banks using only selected local originals.

The recipe contains numerical appearance tables/clip identities and hashes.
The legacy serializer is confined to a new generated workspace; no original
payloads, private work paths or external ELF are required at runtime.
"""
from pathlib import Path
import argparse, contextlib, io, json, math, struct, sys
from excv_core import local_root, validate_output, read_package, file_record, save_json, atomic, safe_name
from excv_actor_resources import extract_resources, sha
from title_assets import take, dlz, dld
from prepare_character_slots import entries as slot_entries, trim_dld
from character_motion import load
import convert_character_catalog as catalog

def inventory(root):
    result = []
    for relative in ('stage/r_sna01_n', 'dl/p/stage/r_sna01_n', 'stage/r_onlinelobby', 'dl/p/stage/r_onlinelobby'):
        result.extend((p for p in (root / relative).glob('*') if p.is_file() and p.suffix in (('.dar', '.qar', '.dlz', '.dci') if 'lobby' in relative else ('.dar', '.qar', '.dlz'))))
    for relative in ('slotdat', 'dl/p/slotdat'):
        result.extend((root / relative).glob('slot_online_*.slot'))
    for relative in ('stage/lobby', 'dl/p/stage/lobby'):
        result.extend((root / relative).glob('*.dar'))
    return sorted(result)

def lobby_motions(source, output, resources, profile, progress, cancel):
    wanted = {r['sha256']: r for r in profile['resources']}
    sources = []
    for relative in ('stage/lobby', 'dl/p/stage/lobby'):
        for p in sorted((source / relative).glob('*.dar')):
            if cancel():
                raise InterruptedError('Character conversion cancelled')
            progress('character / ' + relative + '/' + p.name)
            before = file_record(p)
            raw, mode, items = read_package(p, 'lobby')
            recovered = []
            for entry in items:
                if not entry['name'].endswith('.mtar'):
                    continue
                offset = entry['offset']
                offset = int(offset, 0) if isinstance(offset, str) else offset
                data = take(raw, offset, entry['size'])
                digest = sha(data)
                if digest not in wanted:
                    continue
                if len(data) != wanted[digest]['size']:
                    raise ValueError('Lobby motion extent differs')
                path = output / (digest + '.mtar')
                atomic(path, data)
                resources[digest] = path
                recovered.append(digest)
            if file_record(p) != before:
                raise ValueError('Original lobby motion package changed')
            sources.append({'source': before, 'mode': mode, 'decoded_sha256': sha(raw), 'recovered': recovered})
    return sources

def prepare_appearance(source, workspace, resources, profile, progress, cancel):
    sources = []
    slots = []
    for priority, relative in enumerate(('stage/r_onlinelobby', 'dl/p/stage/r_onlinelobby')):
        folder = workspace / 'work/characters' / ('patch_source' if priority else 'source')
        folder.mkdir(parents=True, exist_ok=True)
        for p in sorted((source / relative).glob('*')):
            if cancel():
                raise InterruptedError('Character conversion cancelled')
            if not p.is_file() or p.suffix not in ('.dar', '.qar', '.dlz', '.dci'):
                continue
            progress('character / ' + relative + '/' + p.name)
            before = file_record(p)
            raw, mode, items = read_package(p, 'r_onlinelobby')
            atomic(folder / safe_name(p.name), raw)
            if p.suffix == '.dar':
                for entry in items:
                    name = safe_name(entry['name'])
                    if Path(name).suffix not in ('.mdn', '.mtar'):
                        continue
                    offset = entry['offset']
                    offset = int(offset, 0) if isinstance(offset, str) else offset
                    atomic(folder / 'entries' / name, take(raw, offset, entry['size']))
            elif p.suffix == '.dlz':
                plain, _ = dlz(raw)
                dld(plain)
                atomic(folder / (p.name + '.dld'), plain)
            if before != file_record(p):
                raise ValueError('Original lobby package changed during conversion')
            sources.append({'source': before, 'mode': mode, 'decoded_sha256': sha(raw)})
    for priority, relative in enumerate(('slotdat', 'dl/p/slotdat')):
        folder = workspace / 'work/characters' / f'slots{priority}'
        folder.mkdir(parents=True, exist_ok=True)
        for p in sorted((source / relative).glob('slot_online_*.slot')):
            if cancel():
                raise InterruptedError('Character conversion cancelled')
            progress('character / ' + relative + '/' + p.name)
            before = file_record(p)
            raw = p.read_bytes()
            rows = []
            for page, kind, key, offset, data in slot_entries(raw):
                ext = {3: 'txn', 13: 'mdn', 33: 'dld'}[kind]
                if kind == 33:
                    if data[:4] == b'segs':
                        data, _ = dlz(data)
                    data = trim_dld(data)
                path = folder / f'{p.stem}_{page}_{key:06x}.{ext}'
                atomic(path, data)
                rows.append({'page': page, 'kind': kind, 'key': key, 'offset': offset, 'output': file_record(path)})
            if before != file_record(p):
                raise ValueError('Original SLOT package changed during conversion')
            sources.append({'source': before, 'mode': 'plain', 'decoded_sha256': sha(raw)})
            slots.append({'source': before, 'entries': rows})
    save_json(workspace / 'work/characters/slots.json', {'sources': slots})
    save_json(workspace / 'outputs/character_appearance/tables.json', profile['tables'])
    save_json(workspace / 'outputs/character_appearance/tables_provenance.json', {'elf_sha256': profile['elf_sha256'], 'scope': 'Reviewed numeric ID/color tables, bundled without ELF bytes', 'consumers': [9251312, 9251776, 9235720]})
    atomic(workspace / 'work/characters/motion_source/online_lobbyplayer.mtar', resources[profile['appearance_motion_sha256']].read_bytes())
    (workspace / 'work/characters/catalog').mkdir(parents=True, exist_ok=True)
    return sources

def motion_bytes(resource, row):
    m = load(resource.read_bytes(), row['index'])
    h = [row[k] for k in ('action', 'key', 'index', 'frames', 'fps', 'loop', 'tracks', 'root_bone')]
    if (m['name'], m['frames'], m['fps'], len(m['tracks']), m['root_bone']) != (h[1], h[3], h[4], h[6], h[7]):
        raise ValueError('Original character motion identity changed')
    result = bytearray(struct.pack('<8I', *h))
    for xyz in m['roots']:
        if not all((math.isfinite(v) for v in xyz)):
            raise ValueError('Nonfinite motion root')
        result += struct.pack('<3f', *xyz)
    for key, track in sorted(m['tracks'].items()):
        result += struct.pack('<I', key)
        for q in track:
            if not all((math.isfinite(v) for v in q)) or abs(sum((v * v for v in q)) - 1) > 1e-05:
                raise ValueError('Invalid original rotation')
            result += struct.pack('<4f', *q)
    return result

def verify_character_resources(output, source_root=None, profile_path=None):
    output = Path(output).resolve()
    errors = []
    assets_checked = 0
    sources_checked = 0
    try:
        report = json.loads((output / 'character-result.json').read_text(encoding='utf-8'))
        path = Path(profile_path) if profile_path else Path(__file__).with_name('excv_character_profile.json')
        profile = json.loads(path.read_text(encoding='utf-8'))
        if report.get('format') != 'MGO2MTEXCV.CHARACTER_RESULT.1' or not report.get('banks_complete') or report.get('source_errors'):
            raise ValueError('Only completed, error-free character results can be resumed')
        if file_record(path)['sha256'] != report['profile']['sha256']:
            raise ValueError('Character recipe changed')
        expected = {r['path']: r for r in [profile['appearance'], *profile['banks']]}
        if {r['path'] for r in report['assets']} != set(expected) or len(report['assets']) != len(expected):
            raise ValueError('Required character asset inventory differs')
        if {p.relative_to(output / 'data').as_posix() for p in (output / 'data').rglob('*') if p.is_file()} != set(expected):
            raise ValueError('Character data inventory differs')
        for row in report['assets']:
            actual = file_record(output / 'data' / row['path'])
            r = expected[row['path']]
            if (actual['size'], actual['sha256']) != (row['size'], row['sha256']) or (actual['size'], actual['sha256']) != (r['size'], r['sha256']):
                raise ValueError('Character bank changed: ' + row['path'])
            assets_checked += 1
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
        if seen != {p.relative_to(root).as_posix() for p in inventory(root)}:
            raise ValueError('Original character source inventory changed')
    except (OSError, ValueError, KeyError, TypeError) as exc:
        errors.append(str(exc))
    return {'passed': not errors, 'assets_checked': assets_checked, 'sources_checked': sources_checked, 'errors': errors}

def build_character_resources(source_root, output, profile_path=None, progress=lambda _: None, cancel=lambda: False, resume=False):
    source = local_root(source_root)
    output = validate_output(source, output)
    if output.exists() and any(output.iterdir()):
        if not resume:
            raise ValueError('Character output must be a new empty folder')
        checked = verify_character_resources(output, source, profile_path)
        if not checked['passed']:
            raise ValueError('Cannot resume character output: ' + '; '.join(checked['errors']))
        report = json.loads((output / 'character-result.json').read_text(encoding='utf-8'))
        report['resumed'] = True
        report['verification'] = checked
        return report
    output.mkdir(parents=True, exist_ok=True)
    path = Path(profile_path) if profile_path else Path(__file__).with_name('excv_character_profile.json')
    profile = json.loads(path.read_text(encoding='utf-8'))
    if profile.get('schema') != 'MGO2MTEXCV.CHARACTER_RECIPE.1':
        raise ValueError('Character recipe version')
    initial = {p.relative_to(source).as_posix(): file_record(p) for p in inventory(source)}
    resources, sources, errors = extract_resources(source, output / 'source', profile, progress, cancel)
    sources.extend(lobby_motions(source, output / 'source', resources, profile, progress, cancel))
    missing = [{'kind': 'original_resource', **r} for r in profile['resources'] if r['sha256'] not in resources]
    appearance = None
    for bank in profile['banks']:
        if cancel():
            raise InterruptedError('Character conversion cancelled')
        progress('character / ' + bank['path'])
        try:
            raw = bytearray(bank['magic'].encode() + struct.pack('<2I', 1, len(bank['clips'])))
            for row in bank['clips']:
                clip = motion_bytes(resources[row['source_sha256']], row)
                if bank['magic'] == 'GCV1':
                    clip = b'GWT1' + struct.pack('<2I', 1, 1) + clip
                    raw += struct.pack('<2I', row['cover_action'], len(clip))
                raw += clip
            if (len(raw), sha(raw)) != (bank['size'], bank['sha256']):
                raise ValueError('Regenerated motion bank differs from reviewed recipe')
            atomic(output / 'data' / bank['path'], raw)
        except (OSError, ValueError, KeyError, struct.error) as exc:
            missing.append({'kind': bank['path'], 'reason': str(exc)})
    try:
        workspace = output / 'working'
        sources.extend(prepare_appearance(source, workspace, resources, profile, progress, cancel))
        progress('character / appearance geometry and original images')
        previous_root, previous_record = (catalog.ROOT, catalog.record)

        def runtime_record(p):
            return previous_record(p if p.is_file() else Path(sys.executable))
        try:
            catalog.ROOT = workspace
            catalog.record = runtime_record
            with contextlib.redirect_stdout(io.StringIO()):
                catalog.main()
        finally:
            catalog.ROOT = previous_root
            catalog.record = previous_record
        generated = workspace / 'work/characters/catalog/appearance.gwc'
        raw = generated.read_bytes()
        r = profile['appearance']
        if (len(raw), sha(raw)) != (r['size'], r['sha256']):
            raise ValueError('Regenerated appearance differs from reviewed recipe')
        atomic(output / 'data' / r['path'], raw)
        appearance = json.loads((generated.parent / 'conversion.json').read_text(encoding='utf-8'))
    except (OSError, ValueError, KeyError, struct.error) as exc:
        missing.append({'kind': 'character/appearance.gwc', 'reason': str(exc)})
    final = {p.relative_to(source).as_posix(): file_record(p) for p in inventory(source)}
    if initial != final:
        errors.append({'error': 'Original character input inventory or bytes changed during conversion'})
    assets = [{'path': p.relative_to(output / 'data').as_posix(), **{k: v for k, v in file_record(p).items() if k != 'path'}} for p in sorted((output / 'data').rglob('*')) if p.is_file()]
    report = {'format': 'MGO2MTEXCV.CHARACTER_RESULT.1', 'source': str(source), 'profile': file_record(path), 'banks_complete': not missing and (not errors) and (len(assets) == 7), 'assets': assets, 'sources': sources, 'source_errors': errors, 'missing': missing, 'appearance_counts': appearance['counts'] if appearance else None, 'unsupported_models': appearance['missing_models'] if appearance else [], 'missing_textures': appearance['missing_textures'] if appearance else {}, 'native_boundaries': ['Existing native action selectors, loop policy and 60 Hz motion sampling are preserved; full original action dispatcher is not claimed', 'Appearance shader 0x14 pattern/spec response remains the existing native approximation', 'Appearance unsupported original model records remain explicit; no substitute model or image is introduced']}
    save_json(output / 'character-result.json', report)
    return report
