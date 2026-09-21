# Generated converter-only source; historical analysis entry points omitted.
"""Compile reviewed DM/TDM pickups from each stage's own GCX and GEOM.

Does not execute GCX or build the game. Retains the exact source fingerprints,
direct main call and original candidate offsets. Only the n022a-audited
factory/conditional structure is accepted, after normalizing physical offsets
and that stage's factory procedure number. Unrelated unknown procedures are
not parsed or treated as verified.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path
from stage_scene import scripts, geometry, InventoryReader
from gcx_inspect import walk
ROOT = Path(__file__).resolve().parents[1]
PROFILES = [(20, 'n022a', 70, 71, 'work/stages/n022a', 'a5601bf0c8020316a05c84c04a560228a392d0aee36c57433a721876fb0a3489', '215cf0e003be714ad0af2d4946eafe013cec5796f9e3f68a558e9936aa6f60d3'), (1, 'n001a', 70, 71, 'outputs/multistage_20260913/source/n001a', 'e444c773b4703771fe2e65cf430b8d6080c13639253b9dc96c2912733a52135d', '7436701c079198d64ef7b587dc4aa4ff8f7f00170bbcc5aa47582b37846869c0'), (4, 'n004a', 69, 70, 'outputs/multistage_20260913/source/n004a', 'de3b6fd1fcdcbc7b537541cac94233dc57e86b179a40402dd9e59503f85b0c62', 'b75f4da8b1f748332bfd34216378a00aeeed8d008fb2f018a88807e406c57121'), (7, 'n007a', 66, 67, 'outputs/bb_lights_runtime_20260914/import/source/n007a', 'a1c93c7754be72872f0e9f57a5741dad737db8750907d7068a27ff956df4bf5f', '5f4f09f4ea2a297f8ac0d82c97bc2d37fd068c86579d1994624801e6871926c9'), (21, 'n023a', 70, 71, 'outputs/multistage_20260913/source/n023a', 'ccc660fbd81c86f8d63ccdb478ce13f1783348d0e8e2b3cabbcb8ff615a539f0', 'f6d80f76569938c365ec5d7c2842e8890ff4ac918ad232f6e93153d784b1e0b8')]

def require(value, reason):
    if not value:
        raise ValueError(reason)

def normalized(value, factory):
    if isinstance(value, list):
        return [normalized(x, factory) for x in value]
    if isinstance(value, dict):
        return {k: 'factory' if k == 'procedure_id' and v == factory else normalized(v, factory) for k, v in value.items() if k not in ('offset', 'end', 'procedure')}
    return value

def main_call_prefix(blob, target):
    u = lambda at: struct.unpack_from('<I', blob, at)[0]
    at = 4
    while u(at) != 4294967295:
        at += 4
        require(at <= 16388, 'Procedure table extent')
    base = at + 4
    body = base + u(base) + 4
    main = body + u(body - 4) + 4
    end = main + u(main - 4)
    reader = InventoryReader(blob)
    reader.procedure_end = end
    pos, stop = reader.block(main, end)
    while pos < stop:
        node, pos = reader.token(pos, stop, 1)
        if node['kind'] == 'proc_call' and node['procedure_id'] == target:
            require(all((x['kind'] == 'terminator' for x in node['children'])), 'Unexpected pickup call arguments')
            return node['offset']
    raise ValueError('Unconditional main pickup call missing')

def compile_all():
    outputs = {}
    reports = []
    baseline = None
    for map_id, name, pid, fid, folder, gcx_sha, geom_sha in PROFILES:
        source = ROOT / folder
        gcx = (source / 'scenerio.gcx').read_bytes()
        geom = (source / (name + '.geom')).read_bytes()
        require(hashlib.sha256(gcx).hexdigest() == gcx_sha, 'GCX fingerprint changed: ' + name)
        require(hashlib.sha256(geom).hexdigest() == geom_sha, 'GEOM fingerprint changed: ' + name)
        procs, anomalies = scripts(gcx, {pid, fid})
        require(not anomalies and len(procs) == 2, 'Pickup procedure parse anomaly: ' + name)
        canonical = normalized(procs, fid)
        if baseline is None:
            baseline = canonical
        require(canonical == baseline, 'Unreviewed pickup condition/factory structure: ' + name)
        main_call = main_call_prefix(gcx, pid)
        caller = next((p for p in procs if p['procedure'] == pid))
        nodes = [n for r in caller['nodes'] for n in walk(r)]
        randoms = [n for n in nodes if n['kind'] == 'command' and n['code'] == '0x82bc9' and (n['arguments'][0].get('value') == 3838500)]
        require(len(randoms) == 2, 'Expected two original random draws')
        geo = geometry(geom, allow_stale_size=name == 'n023a')
        rows = [f'MGO2MT.GCX_ROUND_ITEMS 1 {map_id} verified', 'groups 4']
        groups = []
        for rule in (0, 1):
            for index, (world, item) in enumerate(((140, 22), (113, 10))):
                calls = [n for n in nodes if n['kind'] == 'proc_call' and n['procedure_id'] == fid and (n['children'][1].get('value') == world)]
                require(len(calls) == 4, 'Expected four original candidates')
                group = int(randoms[index]['offset'], 16)
                anchors = []
                rows.append(f'group {rule} {group} {world} equipment {item} 4')
                for call in calls:
                    key = call['children'][0]['value']
                    matches = [a for a in geo['properties'] if a['hash'] == key]
                    require(len(matches) == 1 and 'position' in matches[0], 'Ambiguous stage property')
                    a = matches[0]
                    x, y, z = a['position'][:3]
                    rows.append(f"anchor {a['offset']} {key} {x:.9g} {y + 250:.9g} {z:.9g} 0")
                    anchors.append({'source_offset': a['offset'], 'property_hash': key, 'gcx_call': call['offset'], 'position': [x, y + 250, z, 0]})
                groups.append({'rule': rule, 'item': item, 'group': group, 'anchors': anchors})
        outputs[name + '.gcx-items.cfg'] = '\n'.join(rows) + '\n'
        reports.append({'stage': name, 'map': map_id, 'source': str(source), 'gcx_sha256': gcx_sha, 'geom_sha256': geom_sha, 'main_direct_call': main_call, 'main_scope': 'parsed prefix through this unconditional call; no full VM execution claim', 'pickup_procedure': pid, 'factory_procedure': fid, 'audited_structure_matches_n022a': True, 'groups': groups})
    old = ROOT / 'outputs/gcx_round_items_20260915/candidate/n022a.gcx-items.cfg'
    require(outputs['n022a.gcx-items.cfg'] == old.read_text(encoding='utf-8'), 'n022a baseline changed')
    return (outputs, reports)
