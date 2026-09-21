# Generated converter-only source; historical analysis entry points omitted.
"""Compile validated LA2 attributes/events, not pre-rendered animation frames.
Native runtime evaluates the commands and scene hierarchy at run time.
"""
import json, copy, struct, hashlib
from title_assets import ROOT, read
from la2_inspect import Layout
from export_title_preview import state, apply, geometry_from_states
FIELDS = {3: range(0, 8), 5: range(8, 10), 6: range(10, 26), 10: [26], 11: [27], 1: [28], 2: [28], 9: [29]}

def values(s):
    points = sum(s['position'], [])
    colors = sum(s['color'] * 4 if len(s['color']) == 1 else s['color'], [])
    if len(points) > 8 or len(colors) != 16:
        raise ValueError('state dimensions')
    return points + [0] * (8 - len(points)) + s['size'] + colors + [s['scale'], s['angle'], int(s['visible']), s['blend']]

def compile_asset():
    l = Layout((ROOT / 'outputs/stages/nttitle/layouts/maintitle_mgo.la2').read_bytes())
    m = json.loads((ROOT / 'outputs/title/asset_manifest.json').read_text())
    gcx = (ROOT / 'outputs/stages/nttitle/scenerio.gcx').read_bytes()
    if gcx[1386048:1386051] != b'\x01PF':
        raise ValueError('normal title timeout source changed')
    states = {n['id']: state() for n in l.nodes}
    for n in l.nodes:
        apply(l, states[n['id']], l.properties[n['property']])
    quads, omitted = geometry_from_states(l, m, states, local=True)
    if omitted:
        raise ValueError(omitted)
    data = bytearray(b'M2AN' + struct.pack('<5I', 1, len(l.nodes), len(quads), len(l.events), 10))
    for n in l.nodes:
        data.extend(struct.pack('<3I30f', n['parent'], n['type'], n['flags'], *values(states[n['id']])))
    for q in quads:
        data.extend(struct.pack('<iiI', q['atlas'], q['blend'], q['node']))
        for point, uv, col in zip(q['points'], q['uv'], q['color']):
            data.extend(struct.pack('<8f', *point, *uv, *(x / 255 for x in col[:3]), min(1, col[3] / 128)))
    report = []
    for event in l.events:
        if event['flags']:
            raise ValueError('repeating event not reviewed')
        data.extend(struct.pack('<3I', event['name'], event['flags'], len(event['tracks'])))
        durations = []
        for t in event['tracks']:
            node = t['node'] or 642
            duration = 0
            data.extend(struct.pack('<2I', t['node'], len(t['commands'])))
            for c in t['commands']:
                op = c['opcode']
                payload = bytes.fromhex(c['data'])
                mask = 0
                v = [0] * 30
                d = 0
                if op in [7, 8]:
                    d = read(payload, 0, 'I')[0] if op == 8 else 0
                    duration += d
                    p = read(payload, 4 if op == 8 else 0, 'I')[0]
                    target = copy.deepcopy(states[node])
                    apply(l, target, l.properties[p])
                    v = values(target)
                    for prop in l.properties[p]['commands']:
                        code = prop['opcode']
                        if code in [0, 7, 8]:
                            if (target['texture'], target['uv']) != (states[node]['texture'], states[node]['uv']):
                                raise ValueError('animated UV/texture unsupported')
                        elif code in FIELDS:
                            for field in FIELDS[code]:
                                mask |= 1 << field
                        else:
                            raise ValueError('unreviewed animated property ' + str(code))
                elif op not in [0, 3]:
                    raise ValueError('unreviewed event opcode ' + str(op))
                data.extend(struct.pack('<3I30f', op, d, mask, *v))
            durations.append(duration)
        report.append({'event': hex(event['name']), 'tracks': len(event['tracks']), 'duration_ticks': max(durations)})
    path = ROOT / 'work/title/animated.m2an'
    path.write_bytes(data)
    result = {'source_sha256': hashlib.sha256(l.b).hexdigest(), 'compiled_sha256': hashlib.sha256(data).hexdigest(), 'nodes': len(l.nodes), 'quads': len(quads), 'events': report, 'actor_profile': {'layout': hex(12560735), 'timeout_ticks': 18000, 'gcx_command_offset': hex(1386004), 'timeout_value_offset': hex(1386048), 'selected_procedure': 20, 'completed_procedure': 19, 'gcx_sha256': hashlib.sha256(gcx).hexdigest()}, 'clock': '5 ticks per 1001/60000 second; normal speed only', 'scope': 'Forward event 7/8 evaluation and normal-layout actor; no GCX boot'}
    (ROOT / 'outputs/title/animation_manifest.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))
    return result
