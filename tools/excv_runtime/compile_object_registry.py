# Generated converter-only source; historical analysis entry points omitted.
"""Compile the reviewed n022a/TDM, successful-constructor object schema.

This profile is not a general GCX interpreter and does not assert that every
visual/collision component is implemented. Raw inputs stay private. A missing
or changed prerequisite rejects the entire profile, never shifts wire indices.
"""
import hashlib
import json
import math
import struct
from pathlib import Path
from audit_object_registry import ELF_SHA, FACTORIES, expand
from convert_character_model import header
from elf_image import ElfImage
from gcx_inspect import walk
from gwp import record
from stage_scene import ROOT, geometry, property_children, scripts
SOURCE_SHA = {'scenerio.gcx': 'a5601bf0c8020316a05c84c04a560228a392d0aee36c57433a721876fb0a3489', 'n022a.geom': '215cf0e003be714ad0af2d4946eafe013cec5796f9e3f68a558e9936aa6f60d3', 'cache.dar': 'bd1ecdb45cfdb449787d0b76c4e38dc30a51c493ce593bd437badaea2c4e0d1a'}
REGISTRATION_SITES = {5807144, 5816664, 5912412, 5976944, 5981644, 6014168, 6037448, 7585036, 7591524, 7594444, 12564332, 12570240}
FACTORY_KEYS = {9672405, 10148454, 11892685, 10417097, 2623703, 15325600, 516601}
OPAQUE = {11975350: (9385104, 'stage initialization; no OLObjMan registration'), 6199150: (9443232, 'environment helper; no OLObjMan registration'), 10988420: (6441144, 'densen helper; no OLObjMan registration'), 12422549: (2814432, 'model/transform helper; no OLObjMan registration'), 5054317: (2817064, 'movable door; separate state path'), 501014: (6365296, 'fixed door/locker; no OLObjMan registration'), 10217654: (7503408, 'resets mounted-actor channel counter'), 12760960: (7504976, 'mounted actor; channels 738+counter, not OLObjMan')}

def active_containers(geo):
    return [p for p in geo['properties'] if p['packed_slots'] & 2147483647 == 0]

def compile_profile(folder, elf, inventory):
    if elf.sha256 != ELF_SHA:
        raise ValueError('unreviewed ELF')
    source = {name: (folder / name).read_bytes() for name in SOURCE_SHA}
    for name, data in source.items():
        if hashlib.sha256(data).hexdigest() != SOURCE_SHA[name]:
            raise ValueError('unreviewed source: ' + name)
    sites = set()
    for base, off, size in elf.segments:
        for at in range(0, size - 3, 4):
            word = int.from_bytes(elf.data[off + at:off + at + 4], 'big')
            if word >> 26 != 18 or word & 2:
                continue
            delta = word & 67108860
            if delta & 33554432:
                delta -= 67108864
            if base + at + delta == 7574272:
                sites.add(base + at)
    if sites != REGISTRATION_SITES:
        raise ValueError('registration call graph changed')
    procs, anomalies = scripts(source['scenerio.gcx'])
    procedures = {p['procedure']: p for p in procs}
    literal = []
    for p in procs:
        for n in (n for root in p['nodes'] for n in walk(root)):
            if n['kind'] != 'command' or n.get('code') not in ('0x82bc9', '0x6592a7'):
                continue
            args = n.get('arguments', [])
            if not args or args[0]['kind'] != 'hash':
                raise ValueError('nonliteral factory dispatch')
            literal.append((p['procedure'], n['offset'], args[0]['value']))
    if len(literal) != 959:
        raise ValueError('factory dispatch count changed')
    target_calls = [(p, o, k) for p, o, k in literal if k in FACTORY_KEYS]
    if any((p not in (93, 94, 99, 100) or k not in FACTORIES for p, _, k in target_calls)):
        raise ValueError('registration outside reviewed literal procedure chain')
    commands = expand(procedures, 87)
    geo = geometry(source['n022a.geom'])
    active = active_containers(geo)
    by_offset = {p['offset']: p for p in geo['properties']}
    entries = inventory['entries']
    resources = {}

    def model(key):
        matches = [e for e in entries if e['name'].endswith('.mdn') and int(e['stem_hash24'], 16) == key]
        if len(matches) != 1:
            raise ValueError('MDN lookup not unique: ' + hex(key))
        e = matches[0]
        start = int(e['offset'], 16)
        payload = source['cache.dar'][start:start + e['size']]
        if hashlib.sha256(payload).hexdigest() != e['sha256']:
            raise ValueError('MDN extent/hash mismatch')
        h = header(payload)
        maximum = struct.unpack_from('>3f', payload, h[12] + 32)
        minimum = struct.unpack_from('>3f', payload, h[12] + 48)
        if not all((math.isfinite(v) for v in (*maximum, *minimum))) or any((a < b for a, b in zip(maximum, minimum))):
            raise ValueError('invalid first mesh bounds')
        resources[e['name']] = {**e, 'first_mesh_bounds': {'maximum': maximum, 'minimum': minimum, 'center': [(a + b) * 0.5 for a, b in zip(maximum, minimum)], 'half_extent': [(a - b) * 0.5 for a, b in zip(maximum, minimum)], 'source_offsets': [h[12] + 32, h[12] + 48], 'evidence': '117DA0 copies raw mesh bounds to RuntimeMesh+140/+150; 588D08 uses first RuntimeMesh for GM_HIT.'}}
        return e['name']

    def children(key):
        parents = [p['offset'] for p in active if p['hash'] == key]
        if not parents:
            raise ValueError('missing active GEOM container')
        return property_children(geo, key, parents)

    def transform(node):
        position = node.get('position', [0, 0, 0])[:3]
        degrees = node.get('rotation_degrees', [0, 0, 0])
        scale = node.get('scale_raw', [1, 1, 1])[:3]
        if not all((math.isfinite(x) and abs(x) < 1000000 for x in (*position, *degrees, *scale))):
            raise ValueError('invalid GEOM transform')
        return {'node_offset': node['offset'], 'hash': node['hash'], 'position': position, 'degrees': degrees, 'scale': scale}
    rows, classified = ([], [])
    for c in commands:
        key = c['arguments'][0]['value']
        if key not in FACTORIES:
            if key not in OPAQUE:
                raise ValueError('unclassified proc87 command')
            fn, meaning = OPAQUE[key]
            classified.append({'command': c, 'factory': hex(fn), 'meaning': meaning})
            continue
        kind, factory, site, width, outer, inner = FACTORIES[key]
        table = elf.u32(elf.u32(19016192 + outer) + inner)
        callback = elf.u32(elf.u32(table + 12))
        initial = elf.u32(elf.u32(table + 16))
        if callback != (7567496 if kind == 'cbox' else 7567456) or initial != 7567424:
            raise ValueError('unreviewed callback')
        o = c['options']
        row = {'kind': kind, 'width': width, 'policy': 'maximum' if kind == 'cbox' else 'bits', 'command': c, 'models': [model(v['value']) for v in o.get('0x91d13', [])], 'stable_component_semantics_verified': True}
        if kind == 'car':
            parent, child = [v['value'] for v in o['0x39d650']]
            first = next((p for p in active if p['hash'] == parent))
            group = property_children(geo, parent, [first['offset']])
            matches = [p for p in group if p['hash'] == child]
            if len(matches) != 1:
                raise ValueError('car parent/child unresolved')
            node = matches[0]
            row['glass_model'] = model(o['0xeed859'][0]['value'])
            row['scene_semantics'] = {'state': 'bit0: destroyed; monotone OR', 'body': 'all n022a car body groups remain visible in states 0 and 1', 'glass_groups': {'state0': [2657066], 'state1': [15090430]}, 'geom': {'key': o['0x30bdb7'][0]['value'], 'actor_offset': 944, 'flags': 5, 'matrix': 'body bone palette index 1 (D4+64)', 'stable_states': [0, 1]}, 'absent_geom': 'option DB8E1C is absent, so actor+940 has no shape', 'transient_unimplemented': '9DD50 sets shape header bit200 at damage, 9DCF8 clears it after timer 0.25 before 1.0; wire bit contains no event time', 'evidence': ['5C2EF8 option30BDB7 -> a10', '5C0CD0 GEOM registration', '5BEBF8 mesh group/state and timer branches', '113118 suppression / 113560 reveal']}
        elif kind == 'blast_drum':
            matches = [p for p in geo['properties'] if p['hash'] == o['0x34ebb8'][0]['value']]
            if len(matches) != 1:
                raise ValueError('drum transform unresolved')
            node = matches[0]
            row['scene_semantics'] = {'state': 'bit0: destroyed; monotone OR', 'model_states': [0], 'geom': {'key': o['0x88dbe4'][0]['value'], 'actor_offset': 352, 'flags': 16384, 'matrix': 'body model+16', 'states': [0]}, 'gm_hit': {'factory': '639A8 -> 19B778', 'matrix': 'body model+16', 'states': [0], 'boxes': [{'center': [0, 750, 0], 'half_extent': [500, 750, 500], 'flags': 4198431}, {'center': [550, 750, 0], 'half_extent': [50, 750, 500], 'flags': 4200479}, {'center': [0, 750, 550], 'half_extent': [500, 750, 50], 'flags': 4200479}, {'center': [-550, 750, 0], 'half_extent': [50, 750, 500], 'flags': 4200479}, {'center': [0, 750, -550], 'half_extent': [500, 750, 50], 'flags': 4200479}, {'center': [0, 1550, 0], 'half_extent': [500, 50, 500], 'flags': 4200479}], 'evidence': '5B3458 first mesh bounds; 5E5A40 five face boxes, thickness 100.0 with +/-0.5 coefficients', 'separate_unimplemented_target': 'actor+280 via 63450; not one of these six boxes'}, 'evidence': ['5B3458 GEOM and GM_HIT factories', '5B5198 deletes model/hit and 9DDA8 GEOM on destruction'], 'light_binding': 'none: option F6297A (-light) supplies resource hash F8CEA7 (n022a.lt3), type6 via5E68C8; 5E6720/1470D8 is vertex prelighting, not11B6B0 key/id toggle. Actual n022a drum has one bone and uses DG_GetLightAsync instead.'}
        elif kind == 'bottle_group':
            key = o['0x34baf7'][0]['value']
            matches = [p for p in active if p['hash'] == key]
            if len(matches) != 1:
                raise ValueError('bottle container unresolved')
            node = matches[0]
            row['children'] = [transform(p) for p in children(key)]
            if len(row['models']) != 3 or len(row['children']) > 8:
                raise ValueError('unreviewed bottle variant or state-bit scope')
            row['scene_semantics'] = {'state': 'bit N removes child N in active GEOM enumeration order; monotone OR', 'initial_model': row['models'][0], 'model_variants': 'single resource triple: random modulo 1 always selects first intact model', 'gm_hit': {'factory': '639A8 -> 19B778/19C680/17FDA0', 'flags': 4198416, 'bounds': 'first MDN mesh max/min, see resources.first_mesh_bounds', 'active_predicate': '(state & (1 << childOrdinal)) == 0'}, 'navigation': 'no GEOM obstacle registered by 588D08; do not substitute GM_HIT boxes as walking obstacles', 'evidence': ['589508 resource triple and active child enumeration', '588D08 initial model/GM_HIT', '58A0C0 quiet initial bit and later destruction'], 'transient_unimplemented': 'falling fragments/effects between hit and stable removed component'}
        else:
            anchors = children(o['0x3aca7a'][0]['value'])[:64]
            count = o['0x69623a'][0]['value']
            if count != 15 or len(anchors) != 46:
                raise ValueError('CBOX layout scope changed')
            row['scene_semantics'] = {'state': 'maximum damage stage 0..3', 'models': {'0': 'cbox_a_sk', '1': 'cbox_a_kuzure_sk', '2': 'cbox_a_kuzure_sk', '3': None}, 'hidden_group_by_state': {'1': 15316809, '2': 15316808}, 'geom': {'name': 'COL_cbox_p', 'key': 713957, 'flags': 5, 'matrix': 'CBOX authored/selected placement', 'states': [0, 1, 2]}, 'evidence': ['73DAD0 COL_cbox_p registration', '73BA28 quiet restoration', '73C178 max delta/model switch', '73B6B8 GEOM/model destructor'], 'transient_unimplemented': 'hit particles/sound and box physical pose'}
            for index in range(count):
                rows.append({**row, 'wire_index': len(rows), 'binding_id': 3221225472 + index, 'layout_ordinal': index, 'anchors': [transform(p) for p in anchors]})
            continue
        rows.append({**row, 'wire_index': len(rows), 'binding_id': node['offset'], **transform(node)})
    if [r['kind'] for r in rows] != ['car'] * 10 + ['blast_drum'] * 2 + ['bottle_group'] * 5 + ['cbox'] * 15:
        raise ValueError('successful registration order changed')
    if len(resources) != 20:
        raise ValueError('resource count changed')
    text = 'MGO2MT.STAGE_OBJECTS 1 n022a_success32 20 1 32\n'
    text += ''.join((f"{r['wire_index']} {r['binding_id']} {r['width']} {r['policy']}\n" for r in rows))
    return (text, {'profile': 'n022a_success32', 'map': 20, 'rule': 1, 'entries': rows, 'resources': list(resources.values()), 'classified_commands': classified, 'literal_factory_dispatches': len(literal), 'registration_sites': sorted(sites), 'complete_schema': True, 'scene_components_complete': False, 'conditions': ['Exact reviewed n022a inputs and successful constructors.', 'Native actors reserve all 32 reviewed state entries atomically.', 'This schema is independent of visual/collision readiness.', 'No claim about item592 replay completion or channels738+ mounted actors.'], 'source_length_anomalies': anomalies})
