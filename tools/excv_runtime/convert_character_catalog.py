# Generated converter-only source; historical analysis entry points omitted.
"""Local GWC1 appearance catalog. ID/texture swaps checked at 8D29F0/8D2BC0.
Unsupported source data is recorded, never replaced by another equipment ID.
"""
import json, struct, math
from pathlib import Path
from title_assets import ROOT, read, take, qar, txn, dld
from archive_inventory import strcode
from convert_character_model import header
from character_motion import load as motion_load
from gwp import record
from character_dci import remaps

def bone_rows(b, h):
    return [(read(b, h[10] + 80 * i, 'I')[0], read(b, h[10] + 80 * i + 8, 'i')[0], read(b, h[10] + 80 * i + 32, '3f')) for i in range(h[2])]

def geometry(p, master):
    b = p.read_bytes()
    h = header(b)
    bones = bone_rows(b, h)
    mapping = {v[0]: i for i, v in enumerate(master)}
    deltas = [tuple((master[mapping[k]][2][i] - v[i] for i in range(3))) for k, _, v in bones if k in mapping]
    if not deltas:
        raise ValueError('No shared bind bones')
    delta = deltas[0]
    vertices = []
    indices = []
    parts = []
    keys = set()
    lost = []
    bone_map = []
    for i, (key, parent, _) in enumerate(bones):
        at = i
        seen = set()
        while bones[at][0] not in mapping:
            if at in seen or bones[at][1] < 0:
                raise ValueError('Unknown weighted root')
            seen.add(at)
            at = bones[at][1]
        bone_map.append(mapping[bones[at][0]])
        if at != i:
            lost.append(hex(key))
    for mi in range(h[4]):
        m = read(b, h[12] + 80 * mi, '8I12f')
        if m[4] >= h[6] or m[5] >= h[9] or m[3] + m[2] > h[5]:
            raise ValueError('Mesh references')
        vd = read(b, h[14] + 48 * m[4], '4I')
        defs = take(b, h[14] + 48 * m[4] + 16, 16)
        pos = take(b, h[14] + 48 * m[4] + 32, 16)
        if vd[1] > 16 or vd[3] + m[6] * vd[2] > h[19]:
            raise ValueError('Vertex extent')
        sem = {defs[i] & 15: (defs[i] >> 4, pos[i]) for i in range(vd[1])}
        skin = take(b, h[17] + 40 * m[5] + 8, 32)
        if sem[0][0] != 1 or sem[8][0] != 7:
            raise ValueError('Vertex encoding')
        vb = take(b, h[18] + vd[3], m[6] * vd[2])
        base = len(vertices)
        local = []
        ns = [[0.0, 0.0, 0.0] for _ in range(m[6])]
        for vi in range(m[6]):
            at = vi * vd[2]
            xyz = read(vb, at + sem[0][1], '3f')
            uv = read(vb, at + sem[8][1], '2e')
            uv2 = read(vb, at + sem.get(10, sem[8])[1], '2e')
            weights = take(vb, at + sem[1][1], 4)
            ids = take(vb, at + sem[7][1], 4)
            total = sum(weights)
            if not total:
                raise ValueError('Zero skin weights')
            joints = []
            bindOffsets = []
            for j, w in zip(ids, weights):
                if w and (j >= 32 or skin[j] >= len(bone_map)):
                    raise ValueError('Skin index')
                if w:
                    sourceBone = skin[j]
                    targetBone = bone_map[sourceBone]
                    if bones[sourceBone][0] not in mapping:
                        raise ValueError('Unknown weighted attachment bone')
                    bindOffsets.extend((master[targetBone][2][axis] - bones[sourceBone][2][axis] - delta[axis] for axis in range(3)))
                else:
                    bindOffsets.extend([0, 0, 0])
                joints.append(bone_map[skin[j]] if w else 0)
            local.append([*(xyz[i] + delta[i] for i in range(3)), *uv, *uv2, *joints, *(w / total for w in weights), *bindOffsets])
        for fi in range(m[3], m[3] + m[2]):
            f = read(b, h[13] + 16 * fi, 'HHIIHH')
            ids = read(b, h[20] + f[2], str(f[1]) + 'H')
            if f[1] % 3 or max(ids) >= m[6] or f[3] >= h[7]:
                raise ValueError('Triangle indices')
            mat = read(b, h[15] + 112 * f[3], '12I')
            if not 0 < mat[2] <= 8 or mat[4] >= h[8]:
                raise ValueError('Material texture')
            diffuse = read(b, h[16] + 32 * mat[4], 'I')[0]
            pattern = read(b, h[16] + 32 * mat[6], 'I')[0] if mat[0] & 255 == 20 and mat[2] >= 3 else 0
            parts.append((len(indices), len(ids), diffuse, pattern, mat[0]))
            indices.extend((base + i for i in ids))
            keys.add(diffuse)
            if pattern:
                keys.add(pattern)
            for k in range(0, len(ids), 3):
                a, c, d = [local[ids[k + j]] for j in range(3)]
                u = [c[j] - a[j] for j in range(3)]
                v = [d[j] - a[j] for j in range(3)]
                n = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]]
                for j in ids[k:k + 3]:
                    for ax in range(3):
                        ns[j][ax] += n[ax]
        for v, n in zip(local, ns):
            length = math.sqrt(sum((x * x for x in n)))
            n = [x / length for x in n] if length > 1e-08 else [0, 1, 0]
            vertices.append((*v[:3], *n, *v[3:7], *v[7:]))
    return (vertices, indices, parts, keys, {'source': record(p), 'translation': delta, 'ancestor_bone_fallback': lost})

def main():
    tables = json.loads((ROOT / 'outputs/character_appearance/tables.json').read_text())
    models = {}
    textureRefs = {}
    records = {}
    sources = []
    patch = ROOT / 'work/characters/patch_source'
    maps = {family: remaps((patch / (family + '.dci')).read_bytes()) for family in ['resident', 'resident_d', 'resident_nodld']}
    for priority, folder in enumerate([ROOT / 'work/characters/source', ROOT / 'work/characters/patch_source', ROOT / 'work/characters/slots0', ROOT / 'work/characters/slots1']):
        for p in sorted((folder / 'entries').glob('*.mdn')):
            models[strcode(p.stem)] = p
        for p in sorted(folder.glob('*.mdn')):
            models.setdefault(int(p.stem.rsplit('_', 1)[1], 16), p)
        txnSources = []
        p = folder / 'resident.qar'
        if p.exists():
            q = p.read_bytes()
            sources.append(record(p))
            for e in qar(q):
                if e['name'].endswith('.txn'):
                    txnSources.append((take(q, e['offset'], e['size']), e, p))
        for p in sorted(folder.glob('*.txn')):
            txnSources.append((p.read_bytes(), {'name': p.name, 'offset': 0, 'size': p.stat().st_size}, p))
            sources.append(record(p))
        for raw, e, p in txnSources:
            t = txn(raw)
            context = (priority, p.stem.rsplit('_', 1)[0] if priority >= 2 else 'resident')
            for x in t['textures']:
                textureRefs.setdefault(x['key'], []).append((t, x, raw, e, p, context))
        for p in sorted(folder.glob('*.dld')):
            if p.name.startswith('sm_'):
                continue
            context = (priority, p.stem.rsplit('_', 1)[0] if priority >= 2 else 'resident')
            b = p.read_bytes()
            sources.append(record(p))
            for r in dld(b):
                if not r['parent_size']:
                    order = (priority, -r['priority'], p.name)
                    candidates = []
                    if priority != 1:
                        candidates.append((context, r))
                    if priority < 2:
                        family = 'resident_nodld' if p.name.startswith('resident_nodld') else 'resident_d' if p.name.startswith('resident_d.') or p.name.startswith('resident_d_') else 'resident'
                        mapping = maps[family].get(r['key'])
                        new = r['index'] if mapping is None else mapping.get(r['index'], 65535)
                        if new != 65535:
                            candidates.append(((1, 'resident'), dict(r, source_index=r['index'], index=new)))
                    for recordContext, rr in candidates:
                        key = (recordContext, rr['key'], rr['index'])
                        if key not in records or order > records[key][0]:
                            records[key] = (order, p, b, rr)
    imageCache = {}
    imageEvidence = []
    missingTextures = {}

    def image(key):
        if not key or key in imageCache:
            return
        if key in missingTextures:
            return
        try:
            candidates = textureRefs[key]
            candidates = sorted(candidates, key=lambda ref: ref[5], reverse=True)
            chosen = None
            for ref in candidates:
                tt, xx, rr, ee, pp, pri = ref
                ii = tt['images'][xx['image_index']]
                size0 = (xx['width'] + 3) // 4 * ((xx['height'] + 3) // 4) * {9: 8, 11: 16}.get(ii['codec'], 99999)
                found = records.get((pri, xx['archive'], xx['image_index']))
                if ii['flags'] >> 4 != 15 or (found and found[3]['size'] >= size0):
                    chosen = ref
                    break
            if chosen is None:
                raise ValueError('No complete mip for texture key')
            t, x, raw, e, path, priority = chosen
            im = t['images'][x['image_index']]
            w, h = (x['width'], x['height'])
            block = {9: 8, 11: 16}[im['codec']]
            size = (w + 3) // 4 * ((h + 3) // 4) * block
            if x['x'] or x['y'] or im['width'] != w or (im['height'] != h):
                raise ValueError('Atlas subimage')
            if im['flags'] >> 4 == 15:
                _, p, b, r = records[priority, x['archive'], x['image_index']]
                if r['size'] < size:
                    raise ValueError('DLD top mip extent')
                payload = take(b, r['offset'] + 32, size)
                evidence = {'dld': str(p), 'offset': r['offset'], 'size': size, 'source_index': r.get('source_index', r['index']), 'resolved_index': r['index']}
            else:
                payload = take(raw, im['data_offset'], size)
                evidence = {'qar': str(path), 'entry': e, 'offset': im['data_offset']}
            imageCache[key] = struct.pack('<5I', key, w, h, im['codec'], size) + payload
            imageEvidence.append({'key': key, 'source': evidence})
        except (KeyError, ValueError) as ex:
            missingTextures[key] = str(ex)
    masters = []
    nativeModels = []
    modelIndex = {}
    modelEvidence = []
    missingModels = []
    for gender, name in enumerate(['mgo_base_bounding', 'mgo_baseF_bounding']):
        p = models[strcode(name)]
        b = p.read_bytes()
        masters.append(bone_rows(b, header(b)))
        for id, key, kind, _, mode in tables['male' if gender == 0 else 'female']['rows']:
            if (gender, key) in modelIndex:
                continue
            try:
                v, i, parts, keys, evidence = geometry(models[key], masters[gender])
                modelIndex[gender, key] = len(nativeModels)
                nativeModels.append((gender, key, v, i, parts, keys))
                modelEvidence.append(evidence)
                for k in keys:
                    image(k)
            except (KeyError, ValueError) as ex:
                missingModels.append({'gender': gender, 'id': id, 'key': key, 'reason': str(ex)})
    rules = []
    materialRules = []
    for gender, name in enumerate(['male', 'female']):
        colors = {}
        for r in tables[name + '_colors']['rows']:
            colors.setdefault(r[0], []).append(r)
        for id, key, kind, _, mode in tables[name]['rows']:
            if (gender, key) not in modelIndex:
                continue
            model = modelIndex[gender, key]
            keys = nativeModels[model][5]
            for color in colors.get(id, [(id, 0, 0, 0, 0, 0, 0, 0, 0, 0)]):
                flags = 0
                pairs = []
                if mode == 1:
                    for old, new in zip(color[4::2], color[5::2]):
                        if old and new and (old in keys):
                            image(new)
                            pairs.extend([old, new])
                elif mode == 3:
                    flags |= 1
                resolved = {k: k for k in keys}
                resolved.update(dict(zip(pairs[::2], pairs[1::2])))
                if any((k not in imageCache for k in resolved.values())):
                    flags |= 2
                tint = tuple((struct.unpack('<f', struct.pack('<f', v * struct.unpack('<f', bytes.fromhex('0ad7233c'))[0]))[0] for v in color[5::2])) if mode == 2 else (1.0, 1.0, 1.0)
                if any((not 0 <= v <= 4 for v in tint)):
                    raise ValueError('Material RGB outside reviewed range')
                pairs += (6 - len(pairs)) * [0]
                rules.append((gender, id, kind, color[1], model, flags, *pairs))
                materialRules.append((mode, *tint))
    for gender in range(2):
        rule = next((r for r in rules if r[0] == gender and r[1] == 46 and (r[3] == 0)), None)
        if rule:
            model = rule[4]
            diffuse = nativeModels[model][4][0][2]
            for group, name in enumerate(['black', 'asian', 'latino'], 1):
                key = strcode(('mgo_skinF_hand_' if gender else 'mgo_skin_hand_') + name)
                image(key)
                rules.append((gender, 46, 400, group, model, 0 if key in imageCache else 2, diffuse, key, 0, 0, 0, 0))
                materialRules.append((0, 1.0, 1.0, 1.0))
    motionPath = ROOT / 'work/characters/motion_source/online_lobbyplayer.mtar'
    motion = motion_load(motionPath.read_bytes())
    output = bytearray(b'GWC1' + struct.pack('<7I', 3, len(nativeModels), len(imageCache), len(rules), motion['frames'], 60, motion['name']))
    for bones in masters:
        output += struct.pack('<I', len(bones))
        for key, parent, xyz in bones:
            output += struct.pack('<Ii3f', key, parent, *xyz)
            frames = motion['tracks'].get(key, [[0, 0, 0, 1]] * (motion['frames'] + 1))
            output += b''.join((struct.pack('<4f', *q) for q in frames))
    for xyz in motion['roots']:
        output += struct.pack('<3f', *(xyz[i] - motion['roots'][0][i] for i in range(3)))
    for gender, key, vertices, indices, parts, _ in nativeModels:
        output += struct.pack('<5I', gender, key, len(vertices), len(indices), len(parts))
        output += b''.join((struct.pack('<10f4H16f', *v) for v in vertices))
        output += struct.pack('<' + str(len(indices)) + 'I', *indices)
        output += b''.join((struct.pack('<5I', *p) for p in parts))
    for r, material in zip(rules, materialRules):
        output += struct.pack('<12I', *r) + struct.pack('<I3f', *material)
    for _, payload in sorted(imageCache.items()):
        output += payload
    out = ROOT / 'work/characters/catalog'
    out.mkdir(exist_ok=True)
    (out / 'appearance.gwc').write_bytes(output)
    report = {'output': record(out / 'appearance.gwc'), 'converter': record(Path(__file__)), 'tables': record(ROOT / 'outputs/character_appearance/tables.json'), 'models': modelEvidence, 'images': imageEvidence, 'missing_models': missingModels, 'missing_textures': missingTextures, 'source_archives': sources, 'motion': {'source': record(motionPath), 'decoder': record(Path(__file__).with_name('character_motion.py')), 'name': motion['name'], 'frames': motion['frames'], 'fps': 60, 'scope': 'original first lobby clip; exact PC-selection idle choice not yet verified'}, 'dci_sources': [record(patch / (f + '.dci')) for f in maps], 'slot_sources': record(ROOT / 'work/characters/slots.json'), 'table_provenance': record(ROOT / 'outputs/character_appearance/tables_provenance.json'), 'counts': {'models': len(nativeModels), 'rules': len(rules), 'images': len(imageCache), 'missing_models': len(missingModels), 'missing_textures': len(missingTextures)}}
    (out / 'conversion.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'counts': report['counts'], 'output': report['output']}, indent=2))
