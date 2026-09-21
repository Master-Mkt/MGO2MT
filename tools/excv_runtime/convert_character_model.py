# Generated converter-only source; historical analysis entry points omitted.
"""Local-only MDN/TXN/DLD to GWM1, static male reference assembly.

Format reference: local Jayveer MGS-MDN-Noesis mdn.h, mesh.h, face.h,
bone.h, shader.h. Independently written bounded converter; no Noesis runtime.
This is not an IDA-verified port or account appearance reconstruction.
"""
from pathlib import Path
import math, struct, json
from title_assets import ROOT, read, take, qar, txn, dld
from native_assets import sha
from gwp import record
MODELS = ['mgo_base_chestA', 'mgo_base_legA', 'mgo_base_hand', 'mgo_faceM01_whiteA', 'mgo_mep_leg']

def header(b):
    h = read(b, 0, '24I')
    if b[:4] != b'MDN ' or h[23] != len(b):
        raise ValueError('MDN header')
    for count, off, size in [(h[2], h[10], 80), (h[4], h[12], 80), (h[5], h[13], 16), (h[6], h[14], 48), (h[7], h[15], 112), (h[8], h[16], 32)]:
        if count > 65536:
            raise ValueError('MDN count')
        take(b, off, count * size)
    take(b, h[18], h[19])
    take(b, h[20], h[21])
    return h

def bones(b, h):
    return {read(b, h[10] + 80 * i, 'I')[0]: read(b, h[10] + 80 * i + 32, '3f') for i in range(h[2])}
