# Generated converter-only source; historical analysis entry points omitted.
"""Portable stage conversion using original GCX model references and GEOM."""
from pathlib import Path
import json, struct
from excv_core import atomic, save_json, file_record, sha
from archive_inventory import dar
from title_assets import take
from stage_scene import scripts, foreach_rows, geometry, lighting
from gcx_inspect import walk
from convert_character_model import header
from convert_stage_model import main as convert_model
from stage_geometry import polygons
from compile_stage_scene import write_collision, collision_materials
REVIEWED_STATIC = {'n022a': 84, 'n001a': 84, 'n004a': 85, 'n023a': 88, 'n003a': 86, 'n018a': 85}
SPECIAL_GEOM = {'sm_dd': 'n021a.geom', 'sm_ll': 'n019a.geom'}

def unconditional_nodes(proc):

    def visit(node):
        if node.get('kind') == 'block':
            for child in node.get('children', []):
                yield from visit(child)
        else:
            yield node
    for node in proc['nodes']:
        yield from visit(node)

def select_special_static(stage, procs, table, candidates):
    by_id = {p['procedure']: p for p in procs}
    parent = 87 if stage == 'sm_dd' else 86
    selected_procs = [93, 94, 95, 96] if stage == 'sm_dd' else [89, 90, 88, 112]

    def calls(proc):
        return {n['procedure_id'] for n in unconditional_nodes(by_id[proc]) if n.get('kind') == 'proc_call'}
    if parent not in calls('main') or not set(selected_procs) <= calls(parent):
        raise ValueError('Reviewed simultaneous static call chain changed: ' + stage)
    names = []
    offsets = []
    excluded = []
    authored = []
    for pid in selected_procs:
        if stage == 'sm_ll' and pid != 112:
            candidate = next((r for r in candidates if r['procedure'] == pid), None)
            expected = {89: 3, 90: 9, 88: 4}[pid]
            direct_offsets = {n.get('offset') for n in unconditional_nodes(by_id[pid])}
            if not candidate or len(candidate['models']) != expected or (not set(candidate['calls']) <= direct_offsets):
                raise ValueError('Reviewed simultaneous static model set changed: ' + stage)
            names.extend(candidate['models'])
            offsets.extend(candidate['calls'])
            excluded.extend(candidate['excluded_flagged_models'])
            continue
        for node in unconditional_nodes(by_id[pid]):
            if node.get('code') != '0x6592a7' or not node.get('arguments') or node['arguments'][0].get('value') != 8283167:
                continue
            options = {o['code']: o['children'] for o in node.get('children', []) if o.get('kind') == 'option'}
            model = options.get('0x91d13', [])
            flags = options.get('0x34bc87', [])
            if len(model) != 1 or model[0].get('kind') != 'hash' or model[0]['value'] not in table:
                continue
            name = table[model[0]['value']]
            if stage == 'sm_ll' and name not in {'s05a11a_ladder', 's05a12a_ladder'}:
                continue
            if flags and (len(flags) != 1 or flags[0].get('kind') != 'integer'):
                raise ValueError('Nonliteral static flags')
            flag = flags[0]['value'] if flags else 0
            if stage == 'sm_ll' and flag != 8:
                raise ValueError('Reviewed ladder flags changed')
            if set(options) - {'0x91d13', '0xf6297a', '0xc7932a', '0x7c7362', '0x34bc87'}:
                raise ValueError('Unreviewed static actor option: ' + name)
            scalar = options.get('0x7c7362', [])
            if len(scalar) != 1 or scalar[0].get('kind') != 'integer' or scalar[0]['value'] != 1000:
                raise ValueError('Reviewed static scalar changed: ' + name)
            record = dict(model=name, flags=flag, command=node['offset'], procedure=pid, options=options)
            if stage == 'sm_dd' and flag:
                excluded.append(record)
                continue
            names.append(name)
            offsets.append(node['offset'])
            authored.append(record)
    names = list(dict.fromkeys(names))
    if len(names) != (26 if stage == 'sm_dd' else 18):
        raise ValueError('Reviewed special static model count changed: ' + stage)
    return dict(procedure=parent, procedures=selected_procs, models=names, calls=offsets, excluded_flagged_models=excluded, authored_direct_calls=authored, reachability=dict(main_calls=parent, unconditional_calls=selected_procs), notes=['sm_ll ladder flag 8 is retained explicitly; its original runtime flag semantics are not reproduced.'] if stage == 'sm_ll' else [])

def resolve_geom(stage, source):
    source = Path(source)
    if stage in SPECIAL_GEOM:
        path = source / SPECIAL_GEOM[stage]
        if not path.exists():
            raise ValueError('Reviewed special GEOM is missing: ' + path.name)
        return path
    path = source / (stage + '.geom')
    if path.exists():
        return path
    matches = list(source.glob('*.geom'))
    if len(matches) != 1:
        raise ValueError('Stage GEOM not uniquely identified')
    return matches[0]

def write_runtime_lighting(stage, source, destination, procs, bounds):
    """Portable version-4 writer for previously reviewed baseline presets.

    Preset switching remains separate from compilation; other stage LT3 files
    remain lossless JSON until their system-light selection is reviewed.
    """
    profiles = {'n022a': (107, 'n022a', (0.85, 0.927, 0.834)), 'n001a': (110, 'n001a', (1.0, 1.0, 1.0)), 'n004a': (116, 'n004a_all', (1.0, 1.0, 1.0)), 'n023a': (121, 's02a20a', (1.0, 1.0, 1.0)), 'n007a': (100, 'n007a', (1.0, 1.0, 1.0))}
    if stage not in profiles:
        return dict(status='pending_system_light_selection', output=None)
    pid, name, ambient = profiles[stage]
    keys = ('0x19d92', '0x693e58', '0xd97162', '0x7834e3')
    found = []
    for proc in procs:
        if proc['procedure'] != pid:
            continue
        for node in unconditional_nodes(proc):
            if node.get('kind') != 'command':
                continue
            opts = {o['code']: o['children'] for o in node.get('children', []) if o.get('kind') == 'option'}
            if all((k in opts for k in keys)):
                found.append((node, opts))
    if len(found) != 1:
        raise ValueError('Reviewed runtime system-light preset changed: ' + stage)
    node, opts = found[0]
    for k in keys:
        if len(opts[k]) < 3 or any((v.get('kind') != 'integer' for v in opts[k][:3])):
            raise ValueError('Unresolved runtime system-light values')
    path = source / (name + '.lt3')
    lt = lighting(path.read_bytes())
    hemis = [r for r in lt['records'] if r['kind'] == 64]
    points = [r for r in lt['records'] if r['kind'] == 1]
    authored = [r for r in lt['records'] if r['kind'] in (2, 4)] if stage == 'n007a' else []
    line = lambda values: ' '.join((format(x, '.9g') if isinstance(x, float) else str(x) for x in values)) + '\n'

    def identity(r):
        g = lt['groups'][r['group']]
        return [r['type_bits'], r['light_id'], r['light_key'], *g['minimum'][:3], *g['maximum'][:3]]
    text = f'MGO2MT.STAGE_LIGHTS 4 {len(hemis)}\n'
    for k in keys:
        text += line([v['value'] / 1000 for v in opts[k][:3]])
    text += line(lt['direction'][:3]) + line(ambient) + line(bounds)
    for r in hemis:
        values = []
        for k in ('max', 'min', 'center', 'extent', 'fade_positive', 'fade_negative'):
            values += r[k][:3]
        values += list(r['quaternion']) + list(r['direction'][:3])
        for k in ('front', 'back'):
            values += [c * r[k][3] / 255 for c in r[k][:3]]
        text += line(values + list(r['rotation']) + [r['flags']] + identity(r))
    text += f'POINTS {len(points)}\n'
    for r in points:
        text += line([*r['position'][:3], *[c * r['color'][3] / 255 for c in r['color'][:3]], r['range'], r['extended_range'], r['position'][3], r['flags'], *identity(r)])
    text += f'AUTHORED {len(authored)}\n'
    for r in authored:
        raw76 = struct.unpack_from('>f', bytes.fromhex(r['raw']), 76)[0]
        text += line([r['kind'], *r['min'][:3], *r['max'][:3], *r['position'], *r['direction'], *[c * r['color'][3] / 255 for c in r['color'][:3]], r['parameter0'], r['parameter1'], raw76, r['flags'], *identity(r)])
    output = destination / (stage + '.lighting.cfg')
    atomic(output, text.encode('ascii'))
    return dict(status='reviewed_native_baseline', output=output.name, source=file_record(path), procedure=pid, command=node['offset'], options=opts, counts={'hemisphere': len(hemis), 'point': len(points), 'authored': len(authored)}, unsupported_kinds=sorted({r['kind'] for r in lt['records']} - ({1, 2, 4, 64} if stage == 'n007a' else {1, 64})), policy='Original GCX/LT3 values; existing native light evaluator and baseline choice. Bounds framing and ambient adapter retained. Runtime preset transitions and full original shader parity are not claimed.')

def parse_scripts(data):
    count = next((i for i in range(4097) if struct.unpack('<I', take(data, 4 + 4 * i, 4))[0] == 4294967295))
    parsed = []
    errors = []
    for proc in [*range(1, count + 1), 'main']:
        try:
            rows, anomalies = scripts(data, {proc})
            parsed.extend(rows)
            if anomalies:
                errors.append(dict(procedure=proc, kind='bounded_length_discrepancy', details=anomalies))
        except (ValueError, IndexError, KeyError, struct.error) as e:
            errors.append(dict(procedure=proc, kind='unparsed', error=str(e)))
    return (parsed, errors)

def select_static(stage, procs, entries):
    table = {int(e['stem_hash24'], 16): e['name'][:-4] for e in entries if e['name'].endswith('.mdn')}
    candidates = []
    for proc in procs:
        names = []
        calls = []
        excluded = []
        for top in proc['nodes']:
            for node in walk(top):
                if node.get('kind') != 'command' or not node.get('arguments'):
                    continue
                if node['arguments'][0].get('value') == 8283167:
                    options = {o['code']: o['children'] for o in node.get('children', []) if o.get('kind') == 'option'}
                    model = options.get('0x91d13', [])
                    flags = options.get('0x34bc87', [])
                    if len(model) == 1 and model[0].get('kind') == 'hash' and (model[0].get('value') in table):
                        if flags and (len(flags) != 1 or flags[0].get('kind') != 'integer'):
                            continue
                        value = flags[0]['value'] if flags else 0
                        name = table[model[0]['value']]
                        if value:
                            excluded.append(dict(model=name, flags=value, command=node['offset']))
                        else:
                            names.append(name)
                            calls.append(node['offset'])
                    continue
                if node['arguments'][0].get('value') != 5516077:
                    continue
                try:
                    rows = foreach_rows(node)
                except ValueError:
                    continue
                if not rows or any((len(r) not in (2, 3) or r[1].get('kind') != 'hash' or r[1].get('value') not in table or (len(r) == 3 and r[2].get('kind') != 'integer') for r in rows)):
                    continue
                for row in rows:
                    name = table[row[1]['value']]
                    flags = row[2]['value'] if len(row) == 3 else 0
                    if flags:
                        excluded.append(dict(model=name, flags=flags, command=node['offset']))
                    else:
                        names.append(name)
                calls.append(node['offset'])
        if names:
            candidates.append(dict(procedure=proc['procedure'], models=list(dict.fromkeys(names)), calls=calls, excluded_flagged_models=excluded))
    if stage in SPECIAL_GEOM:
        chosen = select_special_static(stage, procs, table, candidates)
        policy = 'reviewed main-reachable simultaneous static procedures; dynamic actors excluded'
    elif stage in REVIEWED_STATIC:
        chosen = next((r for r in candidates if r['procedure'] == REVIEWED_STATIC[stage]), None)
        if not chosen:
            raise ValueError('Reviewed static procedure no longer matches: ' + stage)
        policy = 'reviewed original static procedure'
    elif len(candidates) == 1:
        chosen = candidates[0]
        policy = 'single structural NewForeach model set; runtime reachability unverified'
    elif candidates:
        chosen = max(candidates, key=lambda r: len(r['models']))
        policy = 'native largest structural model set; alternatives retained, runtime branch unverified'
    else:
        raise ValueError('No structurally valid static GCX model set: ' + stage)
    return (chosen, dict(policy=policy, selected=chosen, candidates=candidates))

def convert_stage(stage, source, destination, progress=lambda _: None, cancel=lambda: False):
    source = Path(source)
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    gcx = source / 'scenerio.gcx'
    data = gcx.read_bytes()
    procs, errors = parse_scripts(data)
    audit = destination / (stage + '.script.json')
    save_json(audit, dict(schema='MGO2MTEXCV.GCX_IR.1', source_sha256=sha(data), procedures=procs, parse_notes=errors, execution_performed=False))
    archive = (source / 'cache.dar').read_bytes()
    entries = dar(archive)[0]
    selected, selection = select_static(stage, procs, entries)
    save_json(destination / 'selection.json', selection)
    progress(stage + ' / 地形とテクスチャ')
    overrides = {}
    dependencies = []
    extraction_path = source.parent / 'extraction.json'
    if stage == 'n022a' and extraction_path.exists():
        from excv_profiles_b import recover_qq_diffuse
        extraction = json.loads(extraction_path.read_text(encoding='utf-8'))
        overrides, dependency = recover_qq_diffuse(extraction['source'], source.parent)
        dependencies = dependency.get('source_files', [])
        save_json(destination / 'diffuse-dependency.json', dependency)
    elif stage in ('n023a', 'n024a') and extraction_path.exists():
        from excv_profiles_b import recover_shared_diffuse
        extraction = json.loads(extraction_path.read_text(encoding='utf-8'))
        overrides, dependency = recover_shared_diffuse(stage, extraction['source'], source.parent)
        dependencies = dependency.get('source_files', [])
        save_json(destination / 'diffuse-dependency.json', dependency)
    chunks = []

    def make_chunk(names, label):
        path = destination / (stage + label + '.gwm')
        report = destination / (stage + label + '.model.json')
        try:
            convert_model(selected=names, output=path, report=report, texture_overrides=overrides, stage_folder=source, scene_audit=audit, texture_context_folders=sorted(source.parent.glob('layer*')), payload_names={'cache.dlz.dld', 'cache_d.dlz.dld', 'cache_nodld.dlz.dld'} if stage == 'n023a' else None)
        except ValueError as e:
            if str(e) != 'GWM3 exceeds runtime 64 MiB file limit' or len(names) < 2:
                raise
            middle = len(names) // 2
            make_chunk(names[:middle], label + 'a')
            make_chunk(names[middle:], label + 'b')
            return
        item = json.loads(report.read_text(encoding='utf-8'))
        chunks.append((path, item))
    make_chunk(selected['models'], '')
    if cancel():
        raise InterruptedError('中止しました。')
    model = dict(vertices=sum((r['vertices'] for _, r in chunks)), triangles=sum((r['triangles'] for _, r in chunks)), missing_textures=[row for _, r in chunks for row in r['missing_textures']], unsupported_meshes=[row for _, r in chunks for row in r['unsupported_meshes']])
    save_json(destination / (stage + '.scene.json'), dict(schema='MGO2MTEXCV.SCENE.1', stage=stage, chunks=[dict(file=p.name, sha256=sha(p.read_bytes()), bounds=r['bounds']) for p, r in chunks], transform='original world coordinates; no baked handedness conversion'))
    geom_path = resolve_geom(stage, source)
    raw = geom_path.read_bytes()
    progress(stage + ' / 当たり判定とライト')
    geo = polygons(raw, allow_stale_size=True, world_only=True)
    definitions = {}
    for proc in procs:
        try:
            definitions.update(collision_materials([{**proc, 'procedure': 83}]))
        except ValueError as e:
            if str(e) != 'Missing reviewed material definitions':
                raise
    collision = write_collision(destination / (stage + '.collision.cfg'), geo, raw, definitions)
    save_json(destination / (stage + '.geom.json'), dict(schema='MGO2MTEXCV.GEOM.1', source_sha256=sha(raw), geometry=geometry(raw, allow_stale_size=True)))
    lights = []
    for path in sorted(source.glob('*.lt3')):
        output = destination / 'lights' / (path.stem + '.json')
        save_json(output, dict(schema='MGO2MTEXCV.LT3.1', source=file_record(path), lighting=lighting(path.read_bytes())))
        lights.append(output.relative_to(destination).as_posix())
    bounds = [min((r['bounds'][i] for _, r in chunks)) for i in range(3)] + [max((r['bounds'][i] for _, r in chunks)) for i in range(3, 6)]
    runtime_lighting = write_runtime_lighting(stage, source, destination, procs, bounds)
    from weapon_connect_points import parse as read_cnp
    for entry in entries:
        if entry['name'].endswith('.cnp'):
            b = take(archive, int(entry['offset'], 0), entry['size'])
            save_json(destination / 'connect_points' / (entry['name'] + '.json'), dict(schema='MGO2MTEXCV.CNP.1', source_sha256=sha(b), points=read_cnp(b)))
    summary = dict(stage=stage, format='MGO2MTEXCV.STAGE.1', selection=selection, vertices=model['vertices'], triangles=model['triangles'], missing_textures=model['missing_textures'], unsupported_meshes=model['unsupported_meshes'], collision=collision, lights=lights, source_script_sha256=sha(data), source_geom_sha256=sha(raw), source_archive_sha256=sha(archive), dependencies=dependencies, runtime_lighting=runtime_lighting, limits=['Converted local assets; additional maps are not automatically activated in the game.', 'Alternative GCX branches, dynamic actors and material shader parity require separate runtime integration.'])
    save_json(destination / 'conversion.json', summary)
    return summary
