# Generated converter-only source; historical analysis entry points omitted.
"""Opt-in original TXN/DLD layer pairing for the portable stage converter.

Image indices are local to each TXN revision. A saved TXN must never index a
base DLD merely because its archive hash and numeric index happen to match.
"""
from pathlib import Path
import hashlib, re, struct
from archive_inventory import dar
from title_assets import qar, txn, dld, take

def digest(b):
    return hashlib.sha256(b).hexdigest()

class TextureContexts:

    def __init__(self, folders, payload_names=None):
        self.layers = []
        active_txn = {}
        active_mdn = {}
        for folder in map(Path, folders):
            from excv_ui_icon import priority_remaps
            remaps = {}
            for path in sorted(folder.glob('*.dci')):
                raw = path.read_bytes()
                remaps[path.stem] = (priority_remaps(raw), {'path': str(path.resolve()), 'size': len(raw), 'sha256': digest(raw)})
            for path in sorted(folder.glob('*.dar')):
                data = path.read_bytes()
                for e in dar(data)[0]:
                    if e['name'].endswith('.mdn'):
                        active_mdn[e['name']] = digest(take(data, int(e['offset'], 0), e['size']))
            for path in sorted(folder.glob('*.qar')):
                data = path.read_bytes()
                package = {'path': str(path.resolve()), 'size': len(data), 'sha256': digest(data)}
                for e in qar(data):
                    if e['name'].endswith('.txn'):
                        raw = take(data, e['offset'], e['size'])
                        active_txn[e['name']] = (e, txn(raw), package, digest(raw))
            payloads = []
            for path in sorted(folder.glob('*.dlz.dld')):
                if payload_names is not None and path.name not in payload_names:
                    continue
                data = path.read_bytes()
                source = {'path': str(path.resolve()), 'size': len(data), 'sha256': digest(data)}
                for r in dld(data):
                    if r['parent_size']:
                        continue
                    family = re.sub('_\\d{6}$', '', path.name.removesuffix('.dlz.dld'))
                    mapping, proof = remaps.get(family, ({}, None))
                    mapped = mapping.get((r['key'], r['priority']))
                    if mapped is not None:
                        index = mapped['mapping'].get(r['index'], 65535)
                        if index == 65535:
                            continue
                        r = {**r, 'source_index': r['index'], 'index': index, 'dci': proof, 'dci_entry': {k: v for k, v in mapped.items() if k != 'mapping'}}
                    payloads.append((data, r, source))
            self.layers.append((folder.name, dict(active_txn), dict(active_mdn), payloads))

    def resolve(self, key, model, mdn_sha256, strict=False):
        attempts = []
        for name, archives, models, payloads in reversed(self.layers):
            if models.get(model + '.mdn') != mdn_sha256:
                attempts.append({'layer': name, 'reason': 'model bytes differ or absent'})
                continue
            candidates = []
            for entry, t, package, txn_sha in archives.values():
                for x in t['textures']:
                    if x['key'] != key:
                        continue
                    im = t['images'][x['image_index']]
                    if im['codec'] not in (9, 11, 16) or not 0 < im['width'] <= 4096 or (not 0 < im['height'] <= 4096):
                        continue
                    if strict and im['flags'] & 15 != 1:
                        continue
                    if (x['width'], x['height']) != (im['width'], im['height']) or x['x'] or x['y']:
                        continue
                    if x['uv_scale'] != [1, 1] or x['uv_offset'] != [0, 0]:
                        continue
                    size = (im['width'] + 3) // 4 * ((im['height'] + 3) // 4) * {9: 8, 11: 16, 16: 16}[im['codec']]
                    matches = [v for v in payloads if v[1]['key'] == x['archive'] and v[1]['index'] == x['index'] and (v[1]['size'] >= size)]
                    if not matches:
                        continue
                    priority = min((v[1]['priority'] for v in matches))
                    matches = [v for v in matches if v[1]['priority'] == priority]
                    raw = {take(b, r['offset'] + 32, size) for b, r, _ in matches}
                    if len(raw) != 1:
                        raise ValueError(f'Ambiguous original DLD context {name} {model} {key:06x}')
                    payload = next(iter(raw))
                    b, r, source = matches[0]
                    image = struct.pack('<4I', im['width'], im['height'], im['codec'], size) + payload
                    candidates.append((entry['name'], image, {'status': 'resolved_original_bytes', 'resolution_policy': 'same revision TXN/DLD; identical original MDN; newest layer with payload', 'layer': name, 'model': model, 'mdn_sha256': mdn_sha256, 'texture_key': key, 'qar': package, 'txn_entry': entry, 'txn_sha256': txn_sha, 'txn_texture': x, 'txn_image': im, 'image_shape_status': 'original_shape_nibble_1_2D' if im['flags'] & 15 == 1 else 'legacy_diffuse_top_image; original sampler dimension flag unresolved', 'dld': source, 'dld_record': r, 'payload_sha256': digest(payload), 'embedded_image_sha256': digest(image)}))
            primary = [v for v in candidates if v[0] == model + '.txn']
            if primary:
                candidates = primary
            elif not strict:
                common = [v for v in candidates if v[0] == 'common.txn']
                if common:
                    candidates = common
            if not candidates:
                attempts.append({'layer': name, 'reason': 'no supported exact TXN/DLD pair'})
                continue
            if len({v[1] for v in candidates}) != 1:
                raise ValueError(f'Ambiguous original texture context {name} {model} {key:06x}')
            _, image, evidence = candidates[0]
            evidence['preceding_attempts'] = attempts
            evidence['equivalent_txn_entries'] = [v[0] for v in candidates]
            return (image, evidence)
        return (None, {'status': 'unresolved', 'texture_key': key, 'model': model, 'fallback_reason': 'No exact supported TXN/DLD revision pair for identical MDN', 'context_attempts': attempts})
