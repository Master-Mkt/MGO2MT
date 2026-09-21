# Generated converter-only source; historical analysis entry points omitted.
"""Recover original RSX CMP words without rewriting a delivered GWM or texture."""
from pathlib import Path
import sys, json, struct, math, hashlib
from title_assets import read, take
from archive_inventory import dar
from convert_character_model import header

def normal(word):
    values = [word >> shift & (1 << bits) - 1 for shift, bits in [(0, 11), (11, 11), (22, 10)]]
    values = [(v - (1 << bits) if v & 1 << bits - 1 else v) * scale for v, bits, scale in zip(values, [11, 11, 10], [32, 32, 64])]
    length = math.sqrt(sum((v * v for v in values)))
    return [v / length for v in values] if length else [0.0, 1.0, 0.0]

def extract(gwm, archive, conversion, output):
    data = gwm.read_bytes()
    version, nv = struct.unpack_from('<2I', data, 4)
    stride = {1: 32, 2: 48, 3: 56}[version]
    raw = archive.read_bytes()
    entries = {e['name']: e for e in dar(raw)[0]}
    models = json.loads(conversion.read_text(encoding='utf-8'))['models']
    records = []
    sources = []
    offset = 0
    floor = []
    for model in models:
        name = model['name'] + '.mdn'
        entry = entries[name]
        b = take(raw, int(entry['offset'], 0), entry['size'])
        h = header(b)
        sources.append(dict(name=name, archive_offset=entry['offset'], size=len(b), sha256=hashlib.sha256(b).hexdigest()))
        for mi in range(h[4]):
            m = read(b, h[12] + mi * 80, '8I12f')
            decl = h[14] + 48 * m[4]
            vd = read(b, decl, '4I')
            defs = take(b, decl + 16, 16)
            positions = take(b, decl + 32, 16)
            sem = {defs[i] & 15: (defs[i] >> 4, positions[i]) for i in range(vd[1])}
            if sem.get(0, (None,))[0] != 1 or sem.get(8, (None,))[0] != 7:
                continue
            for vi in range(m[6]):
                vb = h[18] + vd[3] + vi * vd[2]
                xyz = read(b, vb + sem[0][1], '3f')
                uv = read(b, vb + sem[8][1], '2e')
                actual = struct.unpack_from('<8f', data, 48 + offset * stride)
                assert actual[:3] == xyz and actual[6:8] == uv, (name, mi, vi, offset, 'GWM vertex order differs')
                if sem.get(2, (None,))[0] == 10:
                    word = read(b, vb + sem[2][1], 'I')[0]
                    legacy = [word >> s & 1023 for s in (0, 10, 20)]
                    legacy = [v - 1024 if v & 512 else v for v in legacy]
                    length = math.sqrt(sum((v * v for v in legacy)))
                    legacy = [v / length for v in legacy] if length else [0, 1, 0]
                    assert max((abs(x - y) for x, y in zip(legacy, actual[3:6]))) < 1e-05, (name, mi, vi, 'not legacy DEC3N')
                    records.append((offset, word))
                    if xyz == (-45000.0, 0.0, 31125.0):
                        floor.append(dict(mdn=name, mesh=mi, vertex=vi, gwm_vertex=offset, mdn_offset=vb + sem[2][1], packed_hex=f'{word:08x}', old_normal=actual[3:6], normal=normal(word)))
                offset += 1
    assert offset == nv and records
    digest = hashlib.sha256(data).digest()
    result = b'GWN1' + struct.pack('<2I', nv, len(records)) + digest + b''.join((struct.pack('<2I', *r) for r in records))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(result)
    report = dict(gwm=str(gwm), gwm_sha256=digest.hex(), archive=str(archive), archive_sha256=hashlib.sha256(raw).hexdigest(), conversion=str(conversion), vertices=nv, restored=len(records), sources=sources, floor_evidence=floor, sidecar=str(output), sidecar_sha256=hashlib.sha256(result).hexdigest())
    output.with_suffix('.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps({k: report[k] for k in ['vertices', 'restored', 'floor_evidence', 'sidecar']}))
