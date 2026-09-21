# Generated converter-only source; historical analysis entry points omitted.
"""Compile reviewed n022a stage data into bounded native sidecars.

GCX/GEOM/LT3 remain private inputs; this is not a general GCX interpreter.
Unresolved branches, actors and region logic are explicit audit records.
"""
import json, math, argparse
from stage_scene import ROOT, geometry, lighting, scripts, foreach_rows, property_children
from stage_geometry import polygons
from gcx_inspect import walk
from gwp import record

def collision_materials(procs):
    """Reviewed n022a proc83 / GM_COM_MaterialSetMtr, not a GCX interpreter.

    Current MGO2 9BFC0 keeps a zero -data word unchanged. Words8/9 write
    material+20/+24; 20B530 consumes signed words at 0.01 (null: 0.5).
    """
    result = {}
    for p in procs:
        if p['procedure'] != 83:
            continue
        for top in p['nodes']:
            for n in walk(top):
                if n['kind'] != 'command' or not n.get('arguments') or n['arguments'][0].get('value') != 5456642:
                    continue
                args = n['arguments']
                opts = {x['code']: x['children'] for x in n['children'] if x['kind'] == 'option'}
                data = opts.get('0x927bc4', [])
                if len(args) != 2 or args[1]['kind'] != 'hash' or len(data) != 11 or any((x['kind'] != 'integer' for x in data)):
                    raise ValueError('Unresolved material command')
                values = [x['value'] for x in data]
                if not 0 < values[8] <= 1000 or not 0 < values[9] <= 100:
                    raise ValueError('Unresolved sparse physics material')
                key = args[1]['value']
                old = result.get(key, {})
                resistance = old.get('resistance', 1000)
                resistance_verified = old.get('resistance_verified', False)
                if values[10] != 0:
                    resistance = values[10]
                    resistance_verified = True
                explicit = opts.get('0x597f1f')
                if explicit is not None:
                    if len(explicit) != 1 or explicit[0]['kind'] != 'integer':
                        raise ValueError('Unresolved named material resistance')
                    resistance = explicit[0]['value']
                    resistance_verified = True
                if not -(1 << 31) <= resistance < 1 << 31:
                    raise ValueError('Material resistance signed32 extent')
                result[key] = {'friction': values[8] * 0.01, 'restitution': values[9] * 0.01, 'command': n['offset'], 'resistance': resistance, 'resistance_verified': resistance_verified}
    if not result:
        raise ValueError('Missing reviewed material definitions')
    return result

def write_collision(path, geo, raw, definitions, owner='world'):
    triangles = [t for t in geo['triangles'] if t['owner'] == owner]
    used = sorted({i for t in triangles for i in t['vertices']})
    remap = {old: i for i, old in enumerate(used)}
    materials = []
    indexes = {}
    refs = []
    for t in triangles:
        attribute = t['polygon_attribute']
        header = t.get('material_header', 0)
        key = 0
        verified = bool(attribute & 1)
        if not attribute & 1 and header:
            from title_assets import take, read
            offset, count = take(raw, header, 2)
            slot = attribute >> 6 & 31
            if count > 33 or slot >= count - 1:
                raise ValueError('GEOM material slot outside bounded table')
            at = header + 4 * (offset + 1)
            take(raw, at, count * 4)
            if read(raw, at + 4 * (count - 1), 'I')[0] != 0:
                raise ValueError('GEOM material terminator')
            key = read(raw, at + slot * 4, 'I')[0]
            verified = key in definitions
        definition = definitions.get(key, {})
        value = (key, definition.get('friction', 0.5), definition.get('restitution', 0.5), int(verified), definition.get('resistance', 1000), int(definition.get('resistance_verified', False)))
        if value not in indexes:
            indexes[value] = len(materials)
            materials.append(value)
        refs.append(indexes[value])

    def line(values):
        return ' '.join((format(v, '.9g') if isinstance(v, float) else str(v) for v in values)) + '\n'
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('w', encoding='ascii') as out:
        out.write(f'MGO2MT.STAGE_COLLISION 3 {len(used)} {len(triangles)} {len(materials)}\n')
        for m in materials:
            out.write(line(m))
        for i in used:
            out.write(line(geo['vertices'][i]))
        for t, material in zip(triangles, refs):
            out.write(line([*[remap[i] for i in t['vertices']], t['attribute'], t['polygon_attribute'], material]))
    return {'output': record(path), 'triangles': len(triangles), 'materials': materials, 'unverified_triangles': sum((not materials[i][3] for i in refs)), 'resistance_unverified_triangles': sum((not materials[i][5] for i in refs))}

def compile_collision_only():
    folder = ROOT / 'work/stages/n022a'
    raw = (folder / 'n022a.geom').read_bytes()
    geo = polygons(raw)
    procs, _ = scripts((folder / 'scenerio.gcx').read_bytes())
    definitions = collision_materials(procs)
    result = write_collision(ROOT / 'work/stages/native/n022a.collision.cfg', geo, raw, definitions)
    result.update({'sources': [record(folder / 'n022a.geom'), record(folder / 'scenerio.gcx')], 'definitions': definitions, 'current_mgo2_functions': ['0x9BFC0', '0x184628', '0x185960', '0x184480', '0x1955F0', '0x20B530', '0x21EEB8'], 'world_material_priority': 'block header, then group header; local unknown headers retain unverified default'})
    (ROOT / 'outputs/stage_terrain_materials_20260913.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print('collision', result['triangles'], 'material records', len(result['materials']), 'unverified triangles', result['unverified_triangles'])

def cbox_layout(procs, geo):
    from audit_object_registry import expand
    calls = [c for c in expand({p['procedure']: p for p in procs}, 87) if c['arguments'] and c['arguments'][0].get('value') == 11892685]
    if len(calls) != 1:
        raise ValueError('expected one reviewed n022a CBOX command')
    c = calls[0]
    if c['procedure'] != 100 or set(c['options']) != {'0x3aca7a', '0x69623a'}:
        raise ValueError('unreviewed CBOX options')

    def literal(option, kind):
        values = c['options'][option]
        if len(values) != 1 or values[0]['kind'] != kind:
            raise ValueError('unresolved CBOX argument')
        return values[0]['value']
    key = literal('0x3aca7a', 'hash')
    count = literal('0x69623a', 'integer')
    children = property_children(geo, key)
    candidates = children[:64]
    if not 0 <= count <= len(candidates):
        raise ValueError('CBOX count exceeds unique candidates')
    rows = []
    for p in candidates:
        if 'position' not in p or any((not math.isfinite(v) or abs(v) >= 1000000 for v in p['position'][:3])):
            raise ValueError('unresolved CBOX position')
        rows.append([p['offset'], p['hash'], *p['position'][:3]])
    text = f'MGO2MT.STAGE_CBOX 1 {count} {len(rows)}\n'
    text += ''.join((' '.join((format(v, '.9g') if isinstance(v, float) else str(v) for v in row)) + '\n' for row in rows))
    return (text, {'command': c, 'parent_hash': key, 'requested': count, 'children': len(children), 'retained': len(rows), 'identity': 'GEOM node offset; child hashes may repeat', 'seed': 'global object 0 + 0xA1 (host generation)', 'complete_registry': False, 'snapshot_indexes_assigned': False})

def compile_cbox_only():
    folder = ROOT / 'work/stages/n022a'
    p, a = scripts((folder / 'scenerio.gcx').read_bytes())
    text, evidence = cbox_layout(p, geometry((folder / 'n022a.geom').read_bytes()))
    target = ROOT / 'work/stages/native/n022a.cbox.cfg'
    target.write_text(text, encoding='ascii')
    evidence['sources'] = [record(folder / 'scenerio.gcx'), record(folder / 'n022a.geom')]
    evidence['output'] = record(target)
    (ROOT / 'outputs/cbox_layout_20260912.json').write_text(json.dumps(evidence, indent=2), encoding='utf-8')
    print(json.dumps({'candidates': evidence['retained'], 'requested': evidence['requested'], 'output': evidence['output']}))
