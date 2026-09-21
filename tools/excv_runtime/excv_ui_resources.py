# Generated converter-only source; historical analysis entry points omitted.
"""Portable, local-only UI extraction; importing this module reads no assets.

Original archives/fonts are selected by the caller, never embedded/downloaded.
Unknown LA2 semantics and runtime title composition are reported separately.
"""
from pathlib import Path
import argparse, io, json, os, re, struct
from PIL import Image
from PIL import DdsImagePlugin, PngImagePlugin
from archive_inventory import dar, strcode
from title_assets import qar, txn, dld, dlz, take, read
from la2_inspect import Layout
from excv_core import atomic, save_json, file_record, sha, safe_name, local_root, validate_output, read_package, decode
from excv_ui_icon import priority_remaps, texture_png, render, source_icon
from excv_ui_profile import REVIEWED_SOURCES, ICONS
UI_GROUPS = ('nttitle', 'init_n', 'nt_mgsetup', 'lobby', 'ota_chat', 'r_onlinelobby')
FONT_FILES = ('SCE-PS3-NR-R-JPN.TTF', 'SCE-PS3-NR-L-JPN.TTF', 'SCE-PS3-NR-B-JPN.TTF', 'SCE-PS3-SR-R-JPN.TTF', 'SCE-PS3-NR-R-EXT.TTF', 'SCE-PS3-SR-R-EXT.TTF', 'SCE-PS3-CP-R-KANA.TTF')
LIMIT = 128 * 1024 * 1024

def family(name):
    stem = Path(name).stem
    for value in ('resident_nodld', 'resident_d', 'resident', 'cache_nodld', 'cache_d', 'cache'):
        if stem == value or stem.startswith(value + '_'):
            return value
    return re.sub('_[0-9]{6}$', '', stem)

def checked_path(root, path):
    path = Path(path)
    if path.is_symlink() or (hasattr(path, 'is_junction') and path.is_junction()):
        raise ValueError('Source link is unsupported')
    if root not in path.resolve().parents:
        raise ValueError('Source path escapes selected root')
    if not path.is_file() or path.stat().st_size > LIMIT:
        raise ValueError('Source file extent')
    return path

def load_package(root, path, group, expected):
    checked_path(root, path)
    before = file_record(path)
    relative = path.relative_to(root).as_posix()
    baseline = expected.get(relative)
    if baseline and before['sha256'] != baseline:
        raise ValueError('Reviewed source SHA-256 differs: ' + relative)
    if path.suffix.lower() == '.dci':
        raw = path.read_bytes()
        try:
            priority_remaps(raw)
            data, mode = (raw, 'plain')
        except (ValueError, struct.error):
            data, mode = (decode(raw, group), 'stage-key')
            priority_remaps(data)
        info = None
    else:
        data, mode, info = read_package(path, group)
    if path.suffix.lower() == '.dlz':
        total = 0
        for start in range(0, len(data), 131072):
            total += read(data, start + 8, 'I')[0]
            if total > 256 * 1024 * 1024:
                raise ValueError('Decoded UI texture pool exceeds 256 MiB')
        data, segments = dlz(data)
        info = dld(data)
    if file_record(path) != before:
        raise ValueError('Source changed during extraction: ' + relative)
    proof = dict(source=before, relative=relative, source_sha_verified=bool(baseline), mode=mode, decoded_sha256=sha(data), decoded_size=len(data))
    return (data, info, proof)

def dds_bytes(image, payload):
    width, height, codec = (image['width'], image['height'], image['codec'])
    if not 0 < width <= 8192 or not 0 < height <= 8192 or codec not in (9, 11):
        raise ValueError('Unsupported UI image dimensions/codec')
    size = (width + 3) // 4 * ((height + 3) // 4) * {9: 8, 11: 16}[codec]
    if len(payload) != size:
        raise ValueError('Top mip extent')
    header = bytearray(128)
    header[:4] = b'DDS '
    struct.pack_into('<7I', header, 4, 124, 528391, height, width, size, 0, 1)
    struct.pack_into('<2I4s', header, 76, 32, 4, b'DXT1' if codec == 9 else b'DXT5')
    struct.pack_into('<I', header, 108, 4096)
    return bytes(header) + payload

def png_bytes(image):
    out = io.BytesIO()
    image.save(out, format='PNG')
    data = out.getvalue()
    if Image.open(io.BytesIO(data)).convert('RGBA').tobytes() != image.tobytes():
        raise ValueError('Lossless PNG roundtrip failed')
    return data

class Sources:

    def __init__(self, root, group, destination, expected, progress, cancel):
        self.root = root
        self.group = group
        self.destination = destination
        self.records = []
        self.layouts = {}
        self.texture_refs = {}
        self.images = {}
        self.files = {}
        self.errors = []
        self.cache = {}
        self.decoded_images = {}
        self.image_bytes = 0
        packages = []
        maps = {}
        package_bytes = 0
        for layer, folder in enumerate((root / 'stage' / group, root / 'dl/p/stage' / group)):
            if not folder.is_dir():
                continue
            for path in sorted(folder.iterdir()):
                if cancel():
                    raise InterruptedError('中止しました。')
                if not path.is_file() or path.suffix.lower() not in ('.dar', '.qar', '.dlz', '.dci') or path.name.startswith('sm_'):
                    continue
                safe_name(path.name)
                progress(group + ' / ' + path.name)
                try:
                    data, info, proof = load_package(root, path, group, expected)
                    if package_bytes + len(data) > 512 * 1024 * 1024:
                        raise ValueError('UI group package budget exceeds 512 MiB')
                    package_bytes += len(data)
                    proof['layer'] = layer
                    self.records.append(proof)
                    self.files[path] = proof
                    packages.append((layer, path, data, info))
                    if path.suffix == '.dci':
                        maps[family(path.name)] = priority_remaps(data)
                except InterruptedError:
                    raise
                except Exception as error:
                    self.errors.append(dict(file=path.relative_to(root).as_posix(), error=str(error)))
        for layer, path, data, info in packages:
            if path.suffix == '.dar':
                for entry in info:
                    if not entry['name'].endswith('.la2'):
                        continue
                    name = safe_name(entry['name'])
                    raw = take(data, int(entry['offset'], 0), entry['size'])
                    key = strcode(Path(name).stem)
                    self.layouts[key] = (path, entry, raw)
            elif path.suffix == '.qar':
                for entry in info:
                    if not entry['name'].endswith('.txn'):
                        continue
                    name = safe_name(entry['name'])
                    raw = take(data, entry['offset'], entry['size'])
                    atomic(destination / 'txn' / f'layer{layer}' / name, raw)
                    try:
                        textures = txn(raw)
                        for ref in textures['textures']:
                            self.texture_refs.setdefault(ref['key'], []).append((layer, path, entry, raw, textures, ref))
                    except (ValueError, struct.error) as error:
                        self.errors.append(dict(file=path.name, entry=name, error=str(error)))
            elif path.suffix == '.dlz':
                fam = family(path.name)
                for original in info:
                    if original['parent_size']:
                        continue
                    remap = maps.get(fam, {}).get((original['key'], original['priority']))
                    index = original['index'] if remap is None else remap['mapping'].get(original['index'], 65535)
                    variants = [(0, original)] if layer == 0 else []
                    if index != 65535:
                        variants.append((1, dict(original, index=index, source_index=original['index'], dci={k: v for k, v in remap.items() if k != 'mapping'} if remap else None)))
                    for context, row in variants:
                        key = (context, row['key'], row['index'])
                        order = (layer, -original['priority'], path.name)
                        if key not in self.images or order > self.images[key][0]:
                            self.images[key] = (order, path, data, row, fam)

    def use(self, path):
        return self.files[path]

    def resolve(self, key):
        """Verified key lookup; pixel fingerprints gate reviewed runtime icons.

        LA2 archive hashes can name an older shared bank. Original 1106E8 scans
        other loaded TXNs by texture key. Only this explicit UI stage context is
        considered; unresolved keys are never replaced by similar images.
        """
        if key in self.cache:
            return self.cache[key]
        for layer, path, entry, raw, textures, tex in sorted(self.texture_refs.get(key, []), key=lambda r: r[0], reverse=True):
            image = textures['images'][tex['image_index']]
            if image['codec'] not in (9, 11):
                continue
            if not 0 < image['width'] <= 8192 or not 0 < image['height'] <= 8192:
                continue
            size = (image['width'] + 3) // 4 * ((image['height'] + 3) // 4) * {9: 8, 11: 16}[image['codec']]
            proof = dict(qar=self.use(path), entry=entry, txn_sha256=sha(raw), texture=tex, image=image, metadata_priority=layer)
            if image['flags'] >> 4 == 15:
                found = self.images.get((layer, tex['archive'], tex['image_index']))
                if not found or found[3]['size'] < size:
                    continue
                _, dpath, data, row, fam = found
                payload = take(data, row['offset'] + 32, size)
                proof.update(source=self.use(dpath), record=row, family=fam)
            else:
                payload = take(raw, image['data_offset'], size)
                proof['inline'] = True
            proof.update(payload_sha256=sha(payload), top_mip_size=size)
            original = dds_bytes(image, payload)
            identity = (image['width'], image['height'], image['codec'], sha(payload))
            decoded = self.decoded_images.get(identity)
            if decoded is None:
                extent = image['width'] * image['height'] * 4
                if self.image_bytes + extent > 256 * 1024 * 1024:
                    raise ValueError('UI group decoded image budget exceeds 256 MiB')
                decoded = Image.open(io.BytesIO(original)).convert('RGBA')
                self.decoded_images[identity] = decoded
                self.image_bytes += extent
            proof['rgba_sha256'] = sha(decoded.tobytes())
            stem = f"{tex['archive']:06x}_{tex['image_index']:04x}_{sha(payload)[:16]}"
            atlas = self.destination / 'images' / stem
            if not atlas.with_suffix('.dds').exists():
                atomic(atlas.with_suffix('.dds'), original)
                atomic(atlas.with_suffix('.png'), png_bytes(decoded))
            proof['atlas'] = str(atlas.relative_to(self.destination).with_suffix('.dds')).replace('\\', '/')
            self.cache[key] = (decoded, proof)
            return self.cache[key]
        raise ValueError(f'No exact supported TXN/DLD image for texture {key:06x}')

def font_metadata(data):
    if data[:4] not in (b'\x00\x01\x00\x00', b'OTTO'):
        raise ValueError('Unsupported standalone sfnt font')
    count = read(data, 4, 'H')[0]
    if not 0 < count <= 100:
        raise ValueError('Font table count')
    tables = []
    for i in range(count):
        tag = take(data, 12 + i * 16, 4).decode('ascii')
        checksum, offset, size = read(data, 16 + i * 16, '3I')
        take(data, offset, size)
        if offset < 12 + count * 16:
            raise ValueError('Font table overlaps directory')
        tables.append(dict(tag=tag, offset=offset, size=size, checksum=checksum))
    if not {'cmap', 'head'} <= {r['tag'] for r in tables}:
        raise ValueError('Required font tables missing')
    return tables

def copy_fonts(source, destination):
    report = dict(requested=source is not None, files=[], missing=list(FONT_FILES), errors=[], installed=False)
    if source is None:
        return report
    root = Path(source).resolve(strict=True)
    if not root.is_dir():
        raise ValueError('Font source must be a selected directory')
    validate_output(root, destination)
    for name in FONT_FILES:
        path = root / name
        if not path.exists():
            continue
        try:
            checked_path(root, path)
            before = file_record(path)
            data = path.read_bytes()
            tables = font_metadata(data)
            if file_record(path) != before:
                raise ValueError('Font source changed while reading')
            target = destination / name
            atomic(target, data)
            if target.read_bytes() != data:
                raise ValueError('Font copy mismatch')
            report['files'].append(dict(source=before, output=file_record(target), tables=tables))
            report['missing'].remove(name)
        except Exception as error:
            report['errors'].append(dict(file=name, error=str(error)))
    return report

def discover_fonts(source):
    """Search only inside the user-selected source, never installed OS fonts."""
    root = Path(source).resolve(strict=True)
    found = {}
    visited = 0
    for folder, dirs, files in os.walk(root, followlinks=False):
        folder = Path(folder)
        visited += 1
        if visited > 4096:
            break
        dirs[:] = [n for n in sorted(dirs) if not (folder / n).is_symlink() and (not (hasattr(folder / n, 'is_junction') and (folder / n).is_junction()))]
        if len(folder.relative_to(root).parts) >= 6:
            dirs[:] = []
        for name in files:
            if name in FONT_FILES:
                found.setdefault(folder, set()).add(name)
    if not found:
        return (None, [])
    maximum = max(map(len, found.values()))
    best = [p for p, names in found.items() if len(names) == maximum]
    return (best[0] if len(best) == 1 else None, [str(p) for p in sorted(found)])

def verify_ui_resources(destination, source_root=None):
    destination = Path(destination).resolve(strict=True)
    report = json.loads((destination / 'manifest.json').read_text(encoding='utf-8'))
    if report.get('format') != 'MGO2MTEXCV.LOCAL_UI.1' or not report.get('complete'):
        raise ValueError('UI extraction is incomplete or unsupported')
    if source_root is not None and local_root(source_root) != Path(report['source']).resolve():
        raise ValueError('UI source root changed')
    for row in report['files']:
        path = destination / row['path']
        if destination not in path.resolve().parents or path.is_symlink():
            raise ValueError('UI manifest path escape')
        actual = file_record(path)
        if (actual['sha256'], actual['size']) != (row['sha256'], row['size']):
            raise ValueError('UI output changed: ' + row['path'])
    for group in report['groups']:
        if group['status'] == 'missing':
            continue
        detail = json.loads((destination / group['group'] / 'report.json').read_text(encoding='utf-8'))
        for row in detail['sources']:
            if file_record(Path(row['source']['path'])) != row['source']:
                raise ValueError('UI source changed since extraction')
    for row in report['fonts'].get('files', []):
        if file_record(Path(row['source']['path'])) != row['source']:
            raise ValueError('Original font changed since extraction')
    return report

def extract_ui_resources(source_root, destination, fonts_source=None, groups=UI_GROUPS, progress=lambda _: None, cancel=lambda: False, expected_hashes=None, resume=False):
    root = local_root(source_root)
    destination = validate_output(root, destination)
    groups = tuple(groups)
    validate_output(Path(source_root).resolve(), destination)
    if not groups or len(set(groups)) != len(groups) or any((g not in UI_GROUPS for g in groups)):
        raise ValueError('Unsupported UI group')
    if destination.exists():
        if not resume:
            raise ValueError('Existing UI extraction folder is not overwritten')
        report = verify_ui_resources(destination, source_root)
        if tuple((r['group'] for r in report['groups'])) != groups:
            raise ValueError('UI group selection changed; choose a new output directory')
        if report.get('requested_fonts') != (str(Path(fonts_source).resolve()) if fonts_source else None):
            raise ValueError('Font selection changed; choose a new output directory')
        return report
    destination.mkdir(parents=True)
    expected = REVIEWED_SOURCES if expected_hashes is None else expected_hashes
    report = dict(format='MGO2MTEXCV.LOCAL_UI.1', source=str(root), groups=[], fonts={}, requested_fonts=str(Path(fonts_source).resolve()) if fonts_source else None, complete=False, successful_sources_unchanged=True, network_used=False, original_assets_in_executable=False, unsupported=['frame.m2pv and complete title animation generation', 'GCX/UI event execution and original screen composition', 'Unreviewed image codecs, arbitrary shader effects, atlas mips below the preserved top level', 'Automatic Japanese font choice or OS font installation', 'Dynamic attachment/selection icon states'])
    try:
        for group in groups:
            if cancel():
                raise InterruptedError('中止しました。')
            target = destination / group
            if not (root / 'stage' / group).is_dir() and (not (root / 'dl/p/stage' / group).is_dir()):
                report['groups'].append(dict(group=group, status='missing'))
                continue
            sources = Sources(root, group, target, expected, progress, cancel)
            item = dict(group=group, status='extracted', sources=sources.records, errors=sources.errors, layouts=[], textures=[], icons=[])
            for key, (path, entry, raw) in sorted(sources.layouts.items()):
                if cancel():
                    raise InterruptedError('中止しました。')
                name = safe_name(entry['name'])
                atomic(target / 'layouts' / name, raw)
                row = dict(name=name, hash=key, sha256=sha(raw), source=sources.use(path), entry=entry)
                try:
                    layout = Layout(raw)
                    save_json(target / 'layout_json' / (name + '.json'), layout.json())
                    row.update(status='parsed', nodes=len(layout.nodes), textures=layout.textures)
                except (ValueError, IndexError, KeyError, UnicodeError, struct.error) as error:
                    row.update(status='raw_only', error=str(error))
                item['layouts'].append(row)
            refs = {(r['archive'], r['texture']) for row in item['layouts'] for r in row.get('textures', [])}
            for archive, key in sorted(refs):
                if cancel():
                    raise InterruptedError('中止しました。')
                row = dict(layout_archive=archive, texture=key)
                try:
                    image, proof = sources.resolve(key)
                    tex = proof['texture']
                    crop = (tex['x'], tex['y'], tex['x'] + tex['width'], tex['y'] + tex['height'])
                    if crop[0] < 0 or crop[1] < 0 or crop[2] > image.width or (crop[3] > image.height) or (crop[0] >= crop[2]) or (crop[1] >= crop[3]):
                        raise ValueError('Texture crop extent')
                    pic = image.crop(crop)
                    name = f'{archive:06x}_{key:06x}.png'
                    atomic(target / 'crops' / name, png_bytes(pic))
                    row.update(status='extracted', crop=list(crop), rgba_sha256=sha(pic.tobytes()), proof=proof, binding='direct' if archive == tex['archive'] else 'same-key shared bank lookup')
                except (ValueError, KeyError, IndexError, struct.error) as error:
                    row.update(status='unsupported', error=str(error))
                item['textures'].append(row)
            if group == 'r_onlinelobby':
                indices = {domain: ['MGO2MT_WEAPON_ICONS\t1'] for domain in ('weapon', 'equipment')}
                for known in ICONS:
                    row = dict(known)
                    try:
                        raw = sources.layouts[known['layout_hash']][2]
                        if sha(raw) != known['layout_sha256']:
                            raise ValueError('Reviewed icon layout SHA differs')
                        if known['domain'] == 'equipment':
                            pic, _, proof = source_icon(sources, known['layout_hash'])
                        else:
                            pic, proof = render(sources, known['layout_hash'])
                        if sha(pic.tobytes()) != known['rgba_sha256']:
                            raise ValueError('Reviewed icon RGBA SHA differs')
                        name = f"{known['domain']}_{known['id']}.png"
                        atomic(target / 'icons' / known['domain'] / name, png_bytes(pic))
                        indices[known['domain']].append(f"ICON\t{known['id']}\t{name}")
                        row.update(status='verified', proof=proof)
                    except (ValueError, KeyError, IndexError, struct.error) as error:
                        row.update(status='unsupported', error=str(error))
                    item['icons'].append(row)
                for domain, rows in indices.items():
                    atomic(target / 'icons' / domain / 'index.tsv', ('\n'.join(rows) + '\n').encode('utf-8'))
            if item['errors'] or any((r['status'] not in ('parsed', 'extracted', 'verified') for k in ('layouts', 'textures', 'icons') for r in item[k])):
                item['status'] = 'extracted_with_notes'
            save_json(target / 'report.json', item)
            report['groups'].append(dict(group=group, status=item['status'], sources=len(sources.records), reviewed_source_hashes=sum((r['source_sha_verified'] for r in sources.records)), layouts=len(item['layouts']), parsed=sum((r['status'] == 'parsed' for r in item['layouts'])), textures=len(item['textures']), images=sum((r['status'] == 'extracted' for r in item['textures'])), icons=sum((r['status'] == 'verified' for r in item['icons'])), errors=len(item['errors'])))
        automatic, candidates = discover_fonts(source_root) if fonts_source is None else (None, [])
        report['fonts'] = copy_fonts(fonts_source or automatic, destination / 'fonts')
        report['fonts'].update(discovery_candidates=candidates, discovery_limits=dict(depth=6, directories=4096), selection='explicit directory' if fonts_source else 'inside selected source' if automatic else 'missing or ambiguous')
        report['complete'] = True
        report['status'] = 'extracted_with_notes' if any((r['status'] != 'extracted' for r in report['groups'])) or report['fonts']['missing'] or report['fonts']['errors'] else 'extracted'
    finally:
        files = []
        for path in sorted(destination.rglob('*')):
            if path.is_file():
                row = file_record(path)
                row['path'] = path.relative_to(destination).as_posix()
                files.append(row)
        report['files'] = files
        save_json(destination / 'manifest.json', report)
    return report
