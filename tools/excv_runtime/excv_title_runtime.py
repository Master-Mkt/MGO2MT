# Generated converter-only source; historical analysis entry points omitted.
"""Portable, SHA-scoped native title adapter compiler; no asset I/O on import.

Inputs are user-selected original archives. No converted workspace assets,
original payloads, downloaded files, or GCX execution are dependencies.
The reviewed BASE profile deliberately matches the existing native runtime;
newer patch layouts require a separate review of hard-coded actor/node roles.
"""
from pathlib import Path
import argparse, copy, json, struct
from title_assets import read, take
from la2_inspect import Layout
from export_title_preview import state, apply, geometry_from_states, export
from export_title_animation import values, FIELDS
from excv_ui_resources import Sources, load_package, dds_bytes
from excv_ui_profile import REVIEWED_SOURCES
from excv_core import atomic, save_json, sha, file_record, local_root, validate_output
LAYOUTS = {'maintitle_mgo.la2': '98324aa1a2a3ff2b21f6462f7db3ba354cb60ad86b6b346a46e020dda6f543c8', 'loading_MGO.la2': '29e094e6a449a579498c64de0872f50da42b10f02c5db577622260a4c99c1027', 'l_shib_y03_01_bg_login.la2': 'bfae70504106efba814bd2991b401a6046e1e7ceeac4f82423496e16b8bbe4c6', 'l_free_2_bg.la2': '75e98285f850c67448c246712167618537cf46b17be7c55d5b2774d96dbf7078', 'bg_anime.la2': '5b28817552328445dd06687719409f7180be81b9524bed8f1c262dbcbbeb0523'}
LOOPS = set(range(8928486, 8928492)) | set(range(8934630, 8934637))
LIMITATIONS = ['Reviewed base UI profile only: patch layouts are not substituted into fixed native actor/node roles.', 'No original GCX interpreter or complete original boot/menu transitions; LA2 event subset is compiled for the native runtime.', 'Login/agreement dynamic text and input remain native; original placeholder version text is hidden.', 'Type-9 primitives in lobby frames use the existing textured vertex-color quad approximation; no original shader parity claim.', 'Original font metrics and static ASCII glyphs are retained; arbitrary dynamic text is not baked into these files.', 'Runtime event clock, original draw composition, and complete actor transitions are not newly recovered by this converter.', 'Audio, title.gwp launch routing, movies and character/game resources are outside this module.']

def quad_bytes(quads):
    data = bytearray()
    for q in quads:
        data.extend(struct.pack('<iiI', q['atlas'], q['blend'], q['node']))
        for point, uv, color in zip(q['points'], q['uv'], q['color']):
            data.extend(struct.pack('<8f', *point, *uv, *(x / 255 for x in color[:3]), min(1, color[3] / 128)))
    return data

def animation(layout, manifest, states, version, loops=()):
    quads, omitted = geometry_from_states(layout, manifest, states, local=True)
    if omitted:
        raise ValueError(('Unsupported animation primitives', omitted))
    used = sorted({q['atlas'] for q in quads if q['atlas'] >= 0}) if version == 3 else list(range(len(manifest['txn']['images'])))
    remap = {old: new for new, old in enumerate(used)}
    for q in quads:
        if q['atlas'] >= 0:
            q['atlas'] = remap[q['atlas']]
    data = bytearray(b'M2AN' + struct.pack('<5I', version, len(layout.nodes), len(quads), len(layout.events), len(used)))
    for n in layout.nodes:
        data.extend(struct.pack('<3I30f', n['parent'], n['type'], n['flags'], *values(states[n['id']])))
    data.extend(quad_bytes(quads))
    events = []
    for event in layout.events:
        if event['flags']:
            raise ValueError('Unsupported original event flags')
        data.extend(struct.pack('<3I', event['name'], int(event['name'] in loops), len(event['tracks'])))
        durations = []
        for track in event['tracks']:
            node = track['node'] or (642 if version == 1 else 0)
            if node not in states:
                raise ValueError('Unreviewed event node')
            data.extend(struct.pack('<2I', track['node'], len(track['commands'])))
            total = 0
            for command in track['commands']:
                op = command['opcode']
                b = bytes.fromhex(command['data'])
                mask = duration = 0
                v = [0] * 30
                if op in (7, 8):
                    duration = read(b, 0, 'I')[0] if op == 8 else 0
                    props = layout.properties[read(b, 4 if op == 8 else 0, 'I')[0]]
                    target = copy.deepcopy(states[node])
                    apply(layout, target, props)
                    v = values(target)
                    if (target['texture'], target['uv']) != (states[node]['texture'], states[node]['uv']):
                        raise ValueError('Animated UV/texture is unsupported')
                    for p in props['commands']:
                        code = p['opcode']
                        if code == 0 or (version != 2 and code in (7, 8)):
                            continue
                        if code not in FIELDS:
                            raise ValueError('Unreviewed animated property ' + str(code))
                        for field in FIELDS[code]:
                            mask |= 1 << field
                elif op == 4 and version == 3:
                    duration = read(b, 0, 'I')[0]
                    op = 8
                elif op not in (0, 3):
                    raise ValueError('Unreviewed event command ' + str(op))
                total += duration
                data.extend(struct.pack('<3I30f', op, duration, mask, *v))
            durations.append(total)
        events.append(dict(id=event['name'], loop=event['name'] in loops, duration_ticks=max(durations, default=0)))
    return (bytes(data), used, dict(nodes=len(layout.nodes), quads=len(quads), events=events))

def base_layout(sources, name):
    path = sources.root / 'stage' / sources.group / 'cache.dar'
    raw, rows, proof = load_package(sources.root, path, sources.group, REVIEWED_SOURCES)
    entries = [e for e in rows if e['name'] == name]
    if len(entries) != 1:
        raise ValueError('Missing/ambiguous reviewed layout ' + name)
    entry = entries[0]
    data = take(raw, int(entry['offset'], 0), entry['size'])
    if sha(data) != LAYOUTS[name]:
        raise ValueError('Reviewed layout SHA-256 mismatch: ' + name)
    return (Layout(data), dict(archive=proof, entry=entry, sha256=sha(data)))

def base_ref(sources, key):
    rows = [r for r in sources.texture_refs.get(key, []) if r[0] == 0 and r[1].name == 'cache.qar']
    if len(rows) != 1:
        raise ValueError(f'Missing/ambiguous base texture {key:06x}')
    return rows[0]

def bind(sources, layout, bank=None):
    """Keep original title/loading atlas numbering; compact other layouts later."""
    images = []
    textures = []
    refs = []
    mapping = {}
    if bank:
        candidates = [r for rows in sources.texture_refs.values() for r in rows if r[0] == 0 and r[1].name == 'cache.qar' and (r[2]['name'] == bank)]
        if not candidates:
            raise ValueError('Missing original TXN ' + bank)
        first = candidates[0]
        table = first[4]
        images = copy.deepcopy(table['images'])
        textures = copy.deepcopy(table['textures'])
        for img in images:
            candidates2 = [r for r in candidates if r[5]['image_index'] == img['index']]
            if not candidates2:
                raise ValueError('Unreferenced required atlas')
            refs.append(candidates2[0])
        if bank == 'maintitle_mgo_0.txn':
            fallback = base_ref(sources, 5842336)
            tex = copy.deepcopy(fallback[5])
            tex['image_index'] = len(images)
            images.append(copy.deepcopy(fallback[4]['images'][fallback[5]['image_index']]))
            textures.append(tex)
            refs.append(fallback)
    else:
        for ref in layout.textures:
            row = base_ref(sources, ref['texture'])
            tex = copy.deepcopy(row[5])
            identity = (row[2]['key'] & 16777215, tex['image_index'])
            if identity not in mapping:
                mapping[identity] = len(images)
                images.append(copy.deepcopy(row[4]['images'][tex['image_index']]))
                refs.append(row)
            tex['image_index'] = mapping[identity]
            textures.append(tex)
    return ({'txn': {'images': images, 'textures': textures}}, refs)

def image_bytes(sources, row, mip_count):
    layer, path, entry, raw, table, tex = row
    img = table['images'][tex['image_index']]
    block = {9: 8, 11: 16}.get(img['codec'])
    if block is None:
        raise ValueError('Unsupported original image codec')
    size = (img['width'] + 3) // 4 * ((img['height'] + 3) // 4) * block
    proof = dict(txn=sources.use(path), entry=entry, txn_sha256=sha(raw), image=img, texture=tex)
    if img['flags'] >> 4 == 15:
        found = sources.images.get((0, tex['archive'], tex['image_index']))
        if not found or found[3]['size'] < size:
            raise ValueError('Missing original base image')
        _, p, data, record, family = found
        payload = take(data, record['offset'] + 32, size)
        proof.update(dld=sources.use(p), record=record, family=family)
    else:
        payload = take(raw, img['data_offset'], size)
        proof['inline'] = True
    data = bytearray(dds_bytes(img, payload))
    struct.pack_into('<I', data, 28, mip_count)
    proof['payload_sha256'] = sha(payload)
    return (bytes(data), proof)

def states_for(layout):
    states = {n['id']: state() for n in layout.nodes}
    for n in layout.nodes:
        apply(layout, states[n['id']], layout.properties[n['property']])
    return states

def endpoints(layout, states, event_ids):
    for key in event_ids:
        event = next((e for e in layout.events if e['name'] == key))
        for track in event['tracks']:
            if track['node'] not in states:
                continue
            for command in track['commands']:
                op = command['opcode']
                if not op:
                    continue
                if op not in (7, 8):
                    raise ValueError('Unsupported static event command')
                pointer = read(bytes.fromhex(command['data']), 4 if op == 8 else 0, 'I')[0]
                apply(layout, states[track['node']], layout.properties[pointer])

def verify_title_runtime(destination, source_root=None):
    destination = Path(destination).resolve(strict=True)
    report = json.loads((destination / 'manifest.json').read_text(encoding='utf-8'))
    if report.get('format') != 'MGO2MTEXCV.TITLE_RUNTIME.1' or not report.get('complete'):
        raise ValueError('Incomplete title conversion')
    for row in report['files']:
        path = destination / row['path']
        if destination not in path.resolve().parents or path.is_symlink():
            raise ValueError('Unsafe output path')
        actual = file_record(path)
        if (actual['size'], actual['sha256']) != (row['size'], row['sha256']):
            raise ValueError('Output changed: ' + row['path'])
    if source_root is not None:
        root = local_root(source_root)
        for group in report['groups']:
            for row in group['sources']:
                path = root / row['relative']
                if root not in path.resolve().parents:
                    raise ValueError('Unsafe original source path')
                actual = file_record(path)
                if (actual['size'], actual['sha256']) != (row['source']['size'], row['source']['sha256']):
                    raise ValueError('Original source changed')
    return report

def extract_title_runtime(source_root, destination, *, compare_data=None, progress=lambda _: None, cancel=lambda: False, resume=False):
    """Own a NEW directory containing data/, evidence/, manifest.json.

    resume=True verifies a completed result; partial outputs are never reused.
    compare_data is an optional existing distribution data directory, read only.
    """
    selected = Path(source_root).resolve(strict=True)
    root = local_root(selected)
    destination = validate_output(selected, Path(destination))
    validate_output(root, destination)
    compare = Path(compare_data).resolve(strict=True) if compare_data is not None else None
    if destination.exists():
        if resume:
            return verify_title_runtime(destination, root)
        raise ValueError('Select a new title conversion destination')
    destination.mkdir(parents=True)
    report = dict(format='MGO2MTEXCV.TITLE_RUNTIME.1', complete=False, profile='reviewed-original-base-native-adapter', source_root=str(root), groups=[], assets=[], files=[], limitations=LIMITATIONS)
    try:
        for group, names in [('nttitle', ['maintitle_mgo.la2', 'loading_MGO.la2']), ('lobby', ['l_shib_y03_01_bg_login.la2', 'l_free_2_bg.la2', 'bg_anime.la2'])]:
            if cancel():
                raise InterruptedError('Conversion cancelled')
            sources = Sources(root, group, destination / 'evidence' / group, REVIEWED_SOURCES, progress, cancel)
            report['groups'].append(dict(name=group, sources=sources.records, unused_source_parse_notes=sources.errors))
            for name in names:
                if cancel():
                    raise InterruptedError('Conversion cancelled')
                progress('Runtime UI / ' + name)
                layout, proof = base_layout(sources, name)
                role = {'maintitle_mgo.la2': 'title', 'loading_MGO.la2': 'loading', 'l_shib_y03_01_bg_login.la2': 'login', 'l_free_2_bg.la2': 'agreement', 'bg_anime.la2': 'motion'}[name]
                manifest, refs = bind(sources, layout, {'title': 'maintitle_mgo_0.txn', 'loading': 'loading_MGO_0.txn'}.get(role))
                approximated = []
                if role in ('login', 'agreement'):
                    layout.nodes = [n for n in layout.nodes if n['id'] <= (121 if role == 'login' else 123)]
                states = states_for(layout)
                if role in ('login', 'agreement', 'motion'):
                    for n in layout.nodes:
                        if n['type'] == 9:
                            approximated.append(n['id'])
                            n['type'] = 4
                    for s in states.values():
                        if s['text']:
                            s['text'] = s['text'].rstrip('\r\n')
                if role in ('login', 'agreement'):
                    states[95 if role == 'login' else 97]['visible'] = False
                    endpoints(layout, states, (6960767, 13137062) if role == 'login' else (12844271,))
                    quads, omitted = geometry_from_states(layout, manifest, states)
                    if omitted:
                        raise ValueError(('Unsupported frame primitives', omitted))
                    used = sorted({q['atlas'] for q in quads if q['atlas'] >= 0})
                    remap = {old: new for new, old in enumerate(used)}
                    for q in quads:
                        if q['atlas'] >= 0:
                            q['atlas'] = remap[q['atlas']]
                    data = b'M2PV' + struct.pack('<3I', 1, len(quads), len(used)) + quad_bytes(quads)
                    filename = 'frame.m2pv'
                    detail = dict(quads=len(quads))
                else:
                    if role == 'loading':
                        layout.events = [e for e in layout.events if e['name'] == 10265338]
                    if role == 'motion':
                        layout.events = [e for e in layout.events if e['name'] in LOOPS | {9759398, 10136826, 4409229, 2904445}]
                    data, used, detail = animation(layout, manifest, states, {'title': 1, 'loading': 2, 'motion': 3}[role], LOOPS if role == 'motion' else ())
                    filename = 'loading.m2an' if role == 'loading' else 'animated.m2an'
                output = destination / 'data' / role
                atomic(output / filename, data)
                asset = dict(role=role, layout=proof, output_sha256=sha(data), detail=detail, special_shader_approximated_nodes=approximated, images=[])
                for new, old in enumerate(used):
                    if cancel():
                        raise InterruptedError('Conversion cancelled')
                    image, p = image_bytes(sources, refs[old], 0 if role == 'title' else 1)
                    atomic(output / 'images' / f'{new}.dds', image)
                    asset['images'].append(dict(index=new, sha256=sha(image), source=p))
                report['assets'].append(asset)
                if role == 'title':
                    quads, omitted = export(layout, manifest, True)
                    if omitted:
                        raise ValueError(('Unsupported title endpoint', omitted))
                    atomic(output / 'frame.m2pv', b'M2PV' + struct.pack('<3I', 1, len(quads), len(refs)) + quad_bytes(quads))
            if group == 'nttitle':
                path = root / 'stage/nttitle/scenerio.gcx'
                raw, _, proof = load_package(root, path, group, {})
                if sha(raw) != '8abcc651c0f38729f3308eb241d35c3ac57b50479361d4afc493fad378756abc' or take(raw, 1386048, 3) != b'\x01PF':
                    raise ValueError('Original title GCX/timeout evidence changed')
                report['groups'][-1]['sources'].append(proof)
                report['title_actor_evidence'] = dict(gcx=proof, timeout_ticks=18000, timeout_value_offset=1386048, selected_procedure=20, completed_procedure=19, executed=False)
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
