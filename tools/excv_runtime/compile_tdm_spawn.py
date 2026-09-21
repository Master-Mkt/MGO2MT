# Generated converter-only source; historical analysis entry points omitted.
"""Compile reviewed n022a TDM spawn tables; this does not authorize deployment.

The original GCX hash arrays determine entry order. GEOM labels alone do not.
Only these exact local inputs and the reviewed rule-1 branch are accepted.
"""
import argparse
import hashlib
import json
import math
import struct
from pathlib import Path
from elf_image import ElfImage
from gcx_inspect import walk
from stage_scene import geometry, hash24, scripts
ROOT = Path(__file__).resolve().parents[1]
ELF_SHA = '1a55a41ee5afdebd89075bd205e412c8311f569ba9dc022f629bb63fb4bfd13a'
SOURCE_SHA = {'scenerio.gcx': 'a5601bf0c8020316a05c84c04a560228a392d0aee36c57433a721876fb0a3489', 'n022a.geom': '215cf0e003be714ad0af2d4946eafe013cec5796f9e3f68a558e9936aa6f60d3'}
OPTIONS = {'off_init': 2742246, 'def_init': 2719654, 'off_resp': 3028258, 'def_resp': 3005666}
EXPECTED = {'normal': (1415359, {'off_init': 31250, 'def_init': 31251, 'off_resp': 31252, 'def_resp': 31253}), 'mini': (1415455, {'off_init': 31254, 'def_init': 31255, 'off_resp': 31256, 'def_resp': 31257})}

def demand(condition, message):
    if not condition:
        raise ValueError(message)

def array_hashes(data, resource_id):

    def u32(at):
        demand(0 <= at <= len(data) - 4, 'GCX offset extent')
        return struct.unpack_from('<I', data, at)[0]
    at = 4
    while u32(at) != 4294967295:
        at += 4
        demand(at < 16388, 'GCX procedure table bound')
    base = at + 4
    resource = base + u32(base + 4)
    strings = base + u32(base + 8)
    code = u32(resource + 4 * resource_id)
    at = strings + (code & 2147483647)
    start = at
    values = []
    for _ in range(49):
        demand(0 <= at <= len(data) - 4, 'GCX array extent')
        value = struct.unpack_from('>I', data, at)[0]
        at += 4
        if not value:
            demand(len(values) <= 48, 'original spawn capacity')
            return (start, values)
        demand(value <= 16777215, 'GEOM hash width')
        values.append(value)
    raise ValueError('GCX array has no bounded terminator')

def compile_profile(folder, elf):
    demand(elf.sha256 == ELF_SHA, 'unreviewed ELF')
    inputs = {name: (folder / name).read_bytes() for name in SOURCE_SHA}
    for name, data in inputs.items():
        demand(hashlib.sha256(data).hexdigest() == SOURCE_SHA[name], 'unreviewed ' + name)
    demand(elf.u32(18999332) == 5173744 and elf.u32(elf.u32(18999332 + 4)) == 7705944, 'spawn command dispatch')
    demand([elf.u32(16629520 + i * 4) for i in range(6)] == [3028258, 3005666, 3036960, 2742246, 2719654, 2750948], 'spawn option table')
    demand([elf.u32(16629440 + i * 4) for i in range(8)] == [0, 1, 2, 255, 1, 0, 2, 255], 'team normalization table')
    demand(elf.u32(18639528) == 1145569280, 'ordinary initial spawn Y lift')
    demand(elf.read(19128752, 24) == bytes.fromhex('00000000159a55e5000000001f123bb50000000005491333'), 'original cold-start RNG tail')
    procs, anomalies = scripts(inputs['scenerio.gcx'])
    proc = next((p for p in procs if p['procedure'] == 20))
    demand(not any((proc['offset'] <= a['offset'] < proc['end'] for a in anomalies)), 'spawn procedure has unresolved extent')
    nodes = {int(n['offset'], 16): n for r in proc['nodes'] for n in walk(r)}
    case = nodes[1415325]
    demand(case['kind'] == 'option' and case['letter'] == 'c' and (case['children'][0]['kind'] == 'integer') and (case['children'][0]['value'] == 1), 'rule-1 case')
    demand(nodes[1414653]['arguments'][0]['value'] == 16086727, 'switch source is not reviewed rule getter')
    condition = nodes[1415340]['children']
    demand([n['kind'] for n in condition] == ['integer', 'block', 'operator', 'operator'] and condition[0]['value'] == 0 and (condition[2]['value'] == 11) and (nodes[1415344]['arguments'][0]['value'] == 4711826), 'small-room branch changed')
    geo = geometry(inputs['n022a.geom'])
    by_hash = {}
    for p in geo['properties']:
        by_hash.setdefault(p['hash'], []).append(p)
    rows = []
    arrays = []
    for variant, (command_at, ids) in EXPECTED.items():
        n = nodes[command_at]
        demand(n['kind'] == 'command' and n['code'] == '0x82bc9' and (n['arguments'][0]['value'] == 5173744), 'spawn setup command')
        actual = {int(o['code'], 16): o['children'] for o in n['children'] if o['kind'] == 'option'}
        for option, resource_id in ids.items():
            v = actual[OPTIONS[option]]
            demand(len(v) == 1 and v[0]['kind'] == 'resource' and (v[0]['value'] == resource_id), 'spawn resource binding')
            resource_at, hashes = array_hashes(inputs['scenerio.gcx'], resource_id)
            team = 0 if option.startswith('off') else 1
            kind = 'initial' if option.endswith('init') else 'respawn'
            prefix = 'PRP_' + ('RES_' if kind == 'respawn' else '')
            prefix += 'MINI_' if variant == 'mini' else ''
            prefix += 'TEAM_DEATHMATCH_' + ('A' if team == 0 else 'B')
            unique_count = 8 if variant == 'mini' else 16
            names = [prefix + f'{i % unique_count + 1:02}' for i in range(16)]
            demand(hashes == [hash24(name) for name in names], 'spawn hash sequence')
            arrays.append({'variant': variant, 'kind': kind, 'team_index': team, 'option': option, 'resource_id': resource_id, 'resource_offset': resource_at, 'command_offset': command_at, 'entry_count': len(hashes), 'unique_hash_count': len(set(hashes))})
            for index, (key, name) in enumerate(zip(hashes, names)):
                matches = by_hash.get(key, [])
                demand(len(matches) == 1, 'missing or ambiguous active GEOM spawn hash')
                p = matches[0]
                position = p.get('position')
                demand(position is not None and len(position) == 4 and all((math.isfinite(v) for v in position)) and (position[3] == 1), 'spawn world position')
                raw = p.get('rotation_units')
                rotation = raw if raw is not None else [0, 0, 0]
                demand(len(rotation) == 3 and all((-32768 <= v <= 32767 for v in rotation)), 'spawn rotation width')
                rows.append({'variant': variant, 'kind': kind, 'team_index': team, 'array_index': index, 'hash': key, 'name': name, 'geometry_offset': p['offset'], 'parent_hash': p['parent'], 'world_position': list(position[:3]), 'ordinary_caller_position': [position[0], position[1] + (800 if kind == 'initial' else 0), position[2]], 'rotation_units_raw': list(raw) if raw is not None else None, 'rotation_units_effective': list(rotation), 'yaw_units': rotation[1], 'yaw_degrees': rotation[1] * 360 / 65536})
    demand(len(rows) == 128, 'complete spawn profile count')
    return {'schema': 'mgo2mt-tdm-spawns-1', 'stage': 'n022a', 'map': 20, 'rule': 1, 'sources': [{'path': str(folder / name), 'sha256': SOURCE_SHA[name], 'size': len(data)} for name, data in inputs.items()], 'elf_sha256': elf.sha256, 'arrays': arrays, 'spawns': rows, 'selection': {'initial': 'per-normalized-team counter++ modulo 16', 'respawn': 'original shared xorshift next low32 modulo 16', 'host_rng_setup': 'word0=10351*localMemberId+93467*cacheByte161+123456789; preserve other3; advance32 for initial permutations', 'cold_rng_tail': [362436069, 521288629, 88675123], 'small_room': '(gameInfo[69] - ((NT_flags >> 2) & 1)) <= 8', 'team_normalization': 'NT team byte 0/1 XOR (global cache byte164 & 1)', 'actor_slot': 'not an initial-array index in ordinary rule1', 'world_transform': 'selector uses direct GEOM float position; no origin or Z change', 'ordinary_initial_caller_y_lift': 800, 'ordinary_respawn_caller_y_lift': 0}, 'limits': ['ordinary TDM standard controller with zero spawn override callbacks', 'source profile only; not proof of READY, weapon grant, phase, or live admission', 'host must own/synchronize selected spawn and round/player generation', 'actor+2044 same-team count is serialized by 80E8C8 and restored by 80E760/803DD0; no lateral transform in reviewed creation chain', 'source position plus initial Y800 is control creation position, not proof of completed landing or player separation', 'special characters and other rules require their own selection review']}

def write_profile(profile, out):
    out.mkdir(parents=True, exist_ok=True)
    (out / 'tdm_respawn_candidates.json').write_text(json.dumps(profile, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    text = ['MGO2MT_TDM_SPAWNS 1', 'stage n022a', 'map 20', 'rule 1', '# Evidence profile only. Runtime must separately validate admission and authority.', '# XYZ are raw GEOM. Ordinary initial caller adds Y=800; respawn caller does not.', '# spawn variant kind normalized_team array_index hash world_x world_y world_z yaw_i16']
    for r in profile['spawns']:
        fields = ['spawn', r['variant'], r['kind'], str(r['team_index']), str(r['array_index']), f"0x{r['hash']:06x}"]
        fields += [format(v, '.17g') for v in r['world_position']]
        fields += [str(r['yaw_units'])]
        text.append(' '.join(fields))
    (out / 'n022a.tdm-spawns.cfg').write_text('\n'.join(text) + '\n', encoding='utf-8')
