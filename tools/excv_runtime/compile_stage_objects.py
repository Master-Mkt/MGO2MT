# Generated converter-only source; historical analysis entry points omitted.
"""Bind the reviewed n022a/TDM registry to original scene components.

Private asset compilation only. GEOM walking surfaces and GM_HIT targets are
kept separate; a bottle hit box must never become a player obstacle.
"""
import json, struct, hashlib, math
from pathlib import Path
from stage_scene import ROOT, scripts
from stage_geometry import polygons
from compile_stage_scene import collision_materials, write_collision
from convert_character_model import header
from gwp import record

def write_hit_box(path, center, half_extent):
    if not all((math.isfinite(x) and abs(x) < 100000 for x in (*center, *half_extent))) or any((x <= 0 for x in half_extent)):
        raise ValueError('Invalid reviewed GM_HIT box')
    lo = [x - h for x, h in zip(center, half_extent)]
    hi = [x + h for x, h in zip(center, half_extent)]
    vertices = [(hi[0] if i & 1 else lo[0], hi[1] if i & 2 else lo[1], hi[2] if i & 4 else lo[2]) for i in range(8)]
    faces = [(0, 2, 3), (0, 3, 1), (4, 5, 7), (4, 7, 6), (0, 1, 5), (0, 5, 4), (2, 6, 7), (2, 7, 3), (0, 4, 6), (0, 6, 2), (1, 3, 7), (1, 7, 5)]
    with path.open('w', encoding='ascii') as out:
        out.write('MGO2MT.STAGE_COLLISION 1 8 12\n')
        for v in vertices:
            out.write(' '.join((format(x, '.9g') for x in v)) + '\n')
        for f in faces:
            out.write(' '.join(map(str, (*f, 0, 0))) + '\n')
