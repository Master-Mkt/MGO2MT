# Generated converter-only source; historical analysis entry points omitted.
"""Extract one reviewed, finite original MDN layer to native GWS1.

This preserves authored coordinates; it does not recover the original actor
transform/shader or infer water volume, depth, swimming, or movement speed.
"""
import argparse
import hashlib
import json
import math
import struct
from pathlib import Path
from archive_inventory import dar as read_dar
from convert_character_model import header
from title_assets import read, take
ROOT = Path(__file__).resolve().parents[1]
SELECTIONS = {'n001a': ('n001a_water_cover2.mdn', 'e1fd12aa6ce71ea2f29fbec28d5d84db9169f84731079cdb9733362ed2e0c43e', 3145735), 'n023a': ('water.mdn', '92122dc2d049b432c38023426bf9fb6e2437c460ee4dbcdd8692e992e79db2fa', 3145731)}

def record(path):
    b = path.read_bytes()
    return {'path': str(path.resolve()), 'bytes': len(b), 'sha256': hashlib.sha256(b).hexdigest()}

def decode_mesh(b, shader):
    h = header(b)
    if h[2] != 0:
        raise ValueError('Skeletal model requires a recovered pose transform')
    triangles, parts, vertices = ([], [], [])
    for mi in range(h[4]):
        m = read(b, h[12] + 80 * mi, '8I12f')
        if m[0] >= h[3] or m[4] >= h[6] or m[3] + m[2] > h[5]:
            raise ValueError('MDN mesh table extent')
        vd = read(b, h[14] + 48 * m[4], '4I')
        if not 0 < vd[1] <= 16 or not 0 < vd[2] <= 4096 or m[6] > 1000000:
            raise ValueError('Vertex declaration extent')
        defs = take(b, h[14] + 48 * m[4] + 16, 16)
        offsets = take(b, h[14] + 48 * m[4] + 32, 16)
        sem = {defs[k] & 15: (defs[k] >> 4, offsets[k]) for k in range(vd[1])}
        if sem.get(0, (None,))[0] != 1 or sem[0][1] + 12 > vd[2]:
            raise ValueError('Unsupported position declaration')
        if vd[3] + m[6] * vd[2] > h[19]:
            raise ValueError('Vertex buffer extent')
        local = [read(b, h[18] + vd[3] + vi * vd[2] + sem[0][1], '3f') for vi in range(m[6])]
        if any((not math.isfinite(v) or abs(v) > 10000000.0 for xyz in local for v in xyz)):
            raise ValueError('Invalid position')
        vertices.extend(local)
        for fi in range(m[3], m[3] + m[2]):
            f = read(b, h[13] + 16 * fi, 'HHIIHH')
            if f[3] >= h[7] or not f[1] or f[1] % 3 or (f[2] + 2 * f[1] > h[21]):
                raise ValueError('Face/index extent')
            ids = read(b, h[20] + f[2], str(f[1]) + 'H')
            if max(ids) >= len(local):
                raise ValueError('Vertex index outside mesh')
            mat = read(b, h[15] + 112 * f[3], '12I')
            if mat[0] != shader:
                raise ValueError('Unreviewed shader layer')
            parts.append({'mesh': mi, 'face': fi, 'mesh_flags': m[1], 'packet_flags': f[0], 'material_index': f[3], 'shader': hex(mat[0]), 'material_name_hash': hex(mat[1]), 'mesh_tail_float12': list(m[8:]), 'indices': list(ids)})
            for at in range(0, len(ids), 3):
                tri = [local[i] for i in ids[at:at + 3]]
                a, c, d = tri
                u = [c[i] - a[i] for i in range(3)]
                v = [d[i] - a[i] for i in range(3)]
                cross = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
                if not any(cross):
                    raise ValueError('Degenerate authored water triangle')
                triangles.append(tri)
    if not 0 < len(triangles) <= 4096:
        raise ValueError('GWS1 triangle limit')
    bounds = [min((v[i] for v in vertices)) for i in range(3)] + [max((v[i] for v in vertices)) for i in range(3)]
    return {'bone_count': h[2], 'vertex_count': len(vertices), 'vertices': vertices, 'triangle_count': len(triangles), 'triangles': triangles, 'parts': parts, 'bounds': bounds}

def make_gws(mesh):
    return b'GWS1' + struct.pack('<II', 1, mesh['triangle_count']) + b''.join((struct.pack('<9f', *(value for xyz in tri for value in xyz)) for tri in mesh['triangles']))
