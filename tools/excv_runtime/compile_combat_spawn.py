# Generated converter-only source; historical analysis entry points omitted.
"""Export the actual DM/TDM GCX spawn arrays and GEOM transforms for one stage."""
import argparse, hashlib, json, math, re
from pathlib import Path
from gcx_inspect import walk
from stage_scene import geometry, scripts
from compile_tdm_spawn import array_hashes, ELF_SHA, OPTIONS
from elf_image import ElfImage
ROOT = Path(__file__).resolve().parents[1]
OPTIONS3 = {**OPTIONS, 'sne_init': 2750948, 'sne_resp': 3036960}

def check(value, why):
    if not value:
        raise ValueError(why)

def compile_stage(folder, map_id, out, elf_path):
    elf = ElfImage(elf_path)
    check(elf.sha256 == ELF_SHA, 'ELF not reviewed')
    check(elf.u32(18999332) == 5173744 and elf.u32(elf.u32(18999332 + 4)) == 7705944, 'spawn factory dispatch')
    check(elf.u32(18639528) == 1145569280, 'initial +800')
    stage = folder.name
    check(re.fullmatch('n\\d{3}a', stage), 'stage name')
    gcx = (folder / 'scenerio.gcx').read_bytes()
    geom = (folder / (stage + '.geom')).read_bytes()
    procs, anomalies = scripts(gcx, selected_procedures={20})
    proc = next((p for p in procs if p['procedure'] == 20))
    check(not any((proc['offset'] <= a['offset'] < proc['end'] for a in anomalies)), 'unresolved procedure')
    nodes = [n for root in proc['nodes'] for n in walk(root)]
    switches = [n for n in nodes if n.get('kind') == 'command' and n.get('code') == '0xa65db5' and any((q.get('value') == 16086727 for a in n.get('arguments', []) for q in walk(a)))]
    check(len(switches) == 1, 'rule getter switch')
    cases = {n['children'][0]['value']: n for n in switches[0]['children'] if n.get('kind') == 'option' and n.get('letter') == 'c' and n.get('children') and (n['children'][0]['kind'] == 'integer')}
    by_hash = {}
    stale = stage == 'n023a' and hashlib.sha256(geom).hexdigest() == 'f6d80f76569938c365ec5d7c2842e8890ff4ac918ad232f6e93153d784b1e0b8'
    for prop in geometry(geom, allow_stale_size=stale)['properties']:
        by_hash.setdefault(prop['hash'], []).append(prop)
    results = []
    out.mkdir(parents=True, exist_ok=True)
    for rule in (0, 1):
        check(rule in cases, 'missing rule case')
        sub = list(walk(cases[rule]))
        check(sum((n.get('kind') == 'hash' and n.get('value') == 4711826 for n in sub)) == 1, 'unreviewed mini condition')
        commands = [n for n in sub if n.get('kind') == 'command' and n.get('arguments', [{}])[0].get('value') == 5173744]
        check(len(commands) == 2, 'normal/mini branch shape')
        rows = []
        arrays = []
        for variant, command in enumerate(commands):
            opts = {int(o['code'], 16): o['children'] for o in command['children'] if o.get('kind') == 'option'}
            for name, code in OPTIONS3.items():
                if code not in opts:
                    continue
                ref = opts[code]
                check(len(ref) == 1 and ref[0]['kind'] == 'resource', 'spawn array operand')
                at, hashes = array_hashes(gcx, ref[0]['value'])
                check(0 < len(hashes) <= 48, 'spawn array count')
                team = {'off': 0, 'def': 1, 'sne': 2}[name[:3]]
                kind = 0 if name.endswith('init') else 1
                arrays.append({'variant': variant, 'kind': kind, 'team': team, 'resource': ref[0]['value'], 'offset': at, 'hashes': hashes, 'command': command['offset']})
                for index, h in enumerate(hashes):
                    found = by_hash.get(h, [])
                    check(len(found) == 1, 'ambiguous/missing GEOM hash ' + hex(h))
                    p = found[0]
                    position = p.get('position')
                    rotation = p.get('rotation_units', [0, 0, 0])
                    check(position and len(position) == 4 and (position[3] == 1) and all((math.isfinite(v) and abs(v) < 1000000 for v in position)), 'GEOM position')
                    check(len(rotation) == 3 and all((-32768 <= v <= 32767 for v in rotation)), 'GEOM rotation')
                    rows.append({'variant': variant, 'kind': kind, 'team': team, 'index': index, 'hash': h, 'position': position[:3], 'yaw': rotation[1], 'geometry_offset': p['offset']})
        text = [f'MGO2MT_COMBAT_SPAWNS 1 {stage} {map_id} {rule} {len(rows)}']
        for r in rows:
            text.append(' '.join(['spawn', str(r['variant']), str(r['kind']), str(r['team']), str(r['index']), f"0x{r['hash']:06x}"] + [format(v, '.17g') for v in r['position']] + [str(r['yaw'])]))
        path = out / (stage + ('.dm-spawns.cfg' if rule == 0 else '.tdm-spawns-v2.cfg'))
        path.write_text('\n'.join(text) + '\n', encoding='utf-8')
        result = {'stage': stage, 'map': map_id, 'rule': rule, 'gcx_sha256': hashlib.sha256(gcx).hexdigest(), 'geom_sha256': hashlib.sha256(geom).hexdigest(), 'elf_sha256': elf.sha256, 'case_offset': cases[rule]['offset'], 'arrays': arrays, 'rows': rows, 'output': {'path': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}}
        (out / (stage + f'.rule{rule}.json')).write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
        results.append({'map': map_id, 'rule': rule, 'rows': len(rows), 'file': str(path)})
    return results
