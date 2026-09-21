# Generated converter-only source; historical analysis entry points omitted.
"""Read-only CNP inventory. Current D69220/D69450 verified layout; no runtime rig claims."""
from pathlib import Path
import argparse, hashlib, json, math, re, struct, unittest

def sha(data):
    return hashlib.sha256(data).hexdigest()

def hash24(text):
    value = 0
    for c in text.encode('ascii'):
        value = (value >> 19 | value << 5) + c & 16777215
    return value or 1

def parse(data):
    if len(data) < 16:
        raise ValueError('short CNP header')
    magic, version, count, reserved = struct.unpack_from('>4I', data)
    if magic != 1129205792 or version != 0 or reserved != 0:
        raise ValueError('unsupported CNP header')
    if count > 65536 or len(data) != 16 + count * 64:
        raise ValueError('CNP extent')
    result = []
    for i in range(count):
        at = 16 + 64 * i
        key, parent, child, flag = struct.unpack_from('>4I', data, at)
        pos = struct.unpack_from('>4f', data, at + 16)
        quat = struct.unpack_from('>4f', data, at + 32)
        scale = struct.unpack_from('>3f', data, at + 48)
        if not all((math.isfinite(v) for v in (*pos, *quat, *scale))):
            raise ValueError('nonfinite used vector')
        x, y, z, w = quat
        matrix = [1 - 2 * y * y - 2 * z * z, 2 * x * y + 2 * z * w, 2 * x * z - 2 * y * w, 0, 2 * x * y - 2 * z * w, 1 - 2 * x * x - 2 * z * z, 2 * y * z + 2 * x * w, 0, 2 * x * z + 2 * y * w, 2 * y * z - 2 * x * w, 1 - 2 * x * x - 2 * y * y, 0, *pos]
        result.append(dict(index=i, offset=at, id=key, parent_id=parent, child_id=child, flag=flag, indexed_public=flag < 2147483648, position=pos, quaternion=quat, scale_xyz=scale, unused_scale_w_hex=data[at + 60:at + 64].hex(), local_matrix_storage_order=matrix, usage=None))
    return result

def build(folder, manifest, elf):
    source = json.loads(manifest.read_text(encoding='utf-8'))
    expected = {e['path']: e for e in source['files']}
    raw = elf.read_bytes()
    strings = {}
    for match in re.finditer(b'[A-Za-z_][A-Za-z0-9_]{3,90}\\x00', raw):
        name = match.group()[:-1].decode('ascii')
        if name.startswith('CNP_'):
            strings.setdefault(hash24(name), []).append(dict(name=name, elf_file_offset=match.start()))
    rows = []
    for path in sorted(folder.rglob('*.cnp')):
        data = path.read_bytes()
        relative = path.relative_to(folder).as_posix()
        exp = expected[relative]
        if exp['size'] != len(data) or exp['sha256'] != sha(data):
            raise ValueError('source extraction changed: ' + relative)
        points = parse(data)
        mdn = path.with_suffix('.mdn')
        bone_rows = []
        if mdn.exists():
            from convert_character_model import header
            b = mdn.read_bytes()
            h = header(b)
            bone_rows = [dict(index=i, id=struct.unpack_from('>I', b, h[10] + i * 80)[0], parent_index=struct.unpack_from('>i', b, h[10] + i * 80 + 8)[0]) for i in range(h[2])]
        for p in points:
            p['current_elf_name_matches'] = strings.get(p['id'], [])
            p['parent_mdn_bone_matches'] = [v for v in bone_rows if v['id'] == p['parent_id']]
            p['child_mdn_bone_matches'] = [v for v in bone_rows if v['id'] == p['child_id']]
        rows.append(dict(path=str(path.resolve()), relative=relative, size=len(data), sha256=sha(data), points=points, mdn_bones=bone_rows, weapon_id=None))
    return dict(schema=1, scope='Original extracted CNP local data; weapon identities and world/hand/sight composition unresolved', current_elf=dict(path=str(elf), sha256=sha(raw)), extraction_manifest=dict(path=str(manifest), sha256=sha(manifest.read_bytes()), original_source=source['source'], original_source_sha256=source['source_sha256'], original_archive_reread=False), count_files=len(rows), count_points=sum((len(v['points']) for v in rows)), files=rows, current_functions={'lookup': 'D69220', 'matrix': 'D69450', 'child_lookup': 'D69388', 'flag_nonnegative': 'D68F18', 'flag_negative': 'D68FF8'}, unresolved=['AK102/OPERATOR exact file binding', 'name acronym semantic use', 'hand IK/grip/sight world composition', 'scale use outside matrix getter'])

class Tests(unittest.TestCase):

    def sample(self):
        return b'CNP ' + struct.pack('>3I', 0, 1, 0) + struct.pack('>4I12f', 7, 8, 9, 0, 1, 2, 3, 1, 0, 0, 0, 1, 1, 1, 1, 0)

    def test_layout(self):
        p = parse(self.sample())[0]
        self.assertEqual((p['id'], p['parent_id'], p['child_id']), (7, 8, 9))
        self.assertEqual(p['local_matrix_storage_order'], [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 2, 3, 1])

    def test_unused_lane(self):
        b = bytearray(self.sample())
        b[-4:] = bytes.fromhex('7fc00000')
        self.assertEqual(parse(b)[0]['unused_scale_w_hex'], '7fc00000')

    def test_rejections(self):
        good = self.sample()
        bad = [good[:15], good[:-1], good + b'\x00']
        for offset, value in [(0, 0), (4, 1), (8, 65537), (12, 1), (32, 2143289344), (48, 2139095040)]:
            b = bytearray(good)
            struct.pack_into('>I', b, offset, value)
            bad.append(b)
        for b in bad:
            with self.assertRaises(ValueError):
                parse(b)

    def test_signed_filter(self):
        b = bytearray(self.sample())
        struct.pack_into('>I', b, 28, 2147483648)
        self.assertFalse(parse(b)[0]['indexed_public'])

    def test_hash(self):
        self.assertEqual(hash24('CNP_mzf_def'), 10760790)
