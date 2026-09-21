# Generated converter-only source; historical analysis entry points omitted.
"""Optional local stage02.dat Gekko recipe; four original banks + two native sounds.

No workspace paths, runtime asset copying, downloaded inputs or original bytes
are bundled. An explicitly supplied extracted original resource tree is also
accepted, with exact resource hashes and the weaker container provenance noted.
"""
from pathlib import Path
import argparse, io, json, math, struct, sys, wave, zlib
from excv_core import atomic, save_json, file_record, validate_output
from excv_actor_resources import sha, dds
from excv_character_resources import motion_bytes
from title_assets import take, txn, dlz, dld
from archive_inventory import strcode
import prepare_gekko_model as model
KINDS = {3: '.txn', 8: '.mtar', 13: '.mdn', 33: '.dlz'}

def decode_words(raw, a, b):
    result = bytearray(raw)
    for i in range(len(raw) // 4):
        struct.pack_into('<I', result, 4 * i, struct.unpack_from('<I', raw, 4 * i)[0] ^ a)
        a = a * 48828125 + b & 4294967295
    return (bytes(result), a)

def dat_pages(path):
    size = path.stat().st_size
    with path.open('rb') as stream:
        raw = stream.read(16)
        if len(raw) != 16:
            raise ValueError('STAGE header extent')
        stamp = struct.unpack_from('>I', raw)[0]
        header, key = decode_words(raw[4:], stamp, stamp ^ 61680)
        count = struct.unpack_from('>H', header, 4)[0]
        if not 0 < count < 10000:
            raise ValueError('STAGE page count')
        table, _ = decode_words(take(stream.read(count * 20), 0, count * 20), key, stamp ^ 61680)
    rows = []
    for i in range(count):
        name = table[i * 20:i * 20 + 16].split(b'\x00')[0].decode('ascii')
        offset = struct.unpack_from('>I', table, 20 * i + 16)[0] * 2048
        if not name or any((not (c.isalnum() or c == '_') for c in name)) or (not 16 + 20 * count <= offset < size):
            raise ValueError('STAGE page identity/extent')
        rows.append((name, offset))
    if len({r[0] for r in rows}) != count or [r[1] for r in rows] != sorted(set((r[1] for r in rows))):
        raise ValueError('STAGE duplicate/overlapping pages')
    return (stamp, {name: (at, rows[i + 1][1] if i + 1 < count else size) for i, (name, at) in enumerate(rows)})

def extract_page(path, page, stamp, extent, wanted, destination, cancel):
    start, end = extent
    key = strcode(page)
    a = 129 * key + 2810783193 + stamp & 4294967295
    b = 129 * key + 2055797593 & 4294967295
    with path.open('rb') as stream:
        stream.seek(start)
        raw = stream.read(4)
        first, _ = decode_words(raw, a, b)
        count = struct.unpack('>I', first)[0]
        if not 1 < count <= 100000 or 8 + 16 * count > end - start:
            raise ValueError('STAGE CNF tag extent')
        stream.seek(start)
        tags_raw, _ = decode_words(stream.read(8 + 16 * count), a, b)
        tags = [struct.unpack_from('>IIQ', tags_raw, 8 + 16 * i) for i in range(count)]
        cursor = 8 + 16 * count
        section = None
        section_size = 0
        encoded_size = 0
        found = {}
        for i, (ident, size, offset) in enumerate(tags[:-1]):
            if cancel():
                raise InterruptedError('Gekko conversion cancelled')
            kind, key = (ident >> 24, ident & 16777215)
            if kind == 127:
                if key:
                    cursor = cursor + 2047 & ~2047
                    section_size = offset
                    section = None
                    encoded_size = 0
                    if not 0 < section_size <= 512 * 1024 * 1024:
                        raise ValueError('STAGE section extent')
                else:
                    cursor += encoded_size or offset
                    section = None
                    section_size = 0
                    encoded_size = 0
            elif kind == 126:
                if not section_size or not 0 < size <= 512 * 1024 * 1024 or start + cursor + size > end:
                    raise ValueError('STAGE compressed extent')
                stream.seek(start + cursor)
                data = stream.read(size)
                salt = struct.unpack_from('<H', data)[0] ^ 37765
                decoded, _ = decode_words(data, (salt ^ 25974) << 16 | salt, salt * 278)
                decoded = b'x\x9c' + decoded[2:]
                inflater = zlib.decompressobj()
                section = inflater.decompress(decoded, section_size + 1)
                if not inflater.eof or inflater.unconsumed_tail or len(section) != section_size:
                    raise ValueError('STAGE decompression extent/end')
                if len(inflater.unused_data) >= 16:
                    raise ValueError('STAGE compressed trailing extent')
                encoded_size = size
            elif kind in KINDS and (page, key, KINDS[kind]) in wanted:
                length = tags[i + 1][2] - offset
                if length < 0 or offset + length > section_size:
                    raise ValueError('STAGE resource extent')
                if section is not None:
                    data = take(section, offset, length)
                else:
                    if start + cursor + offset + length > end:
                        raise ValueError('STAGE raw resource extent')
                    stream.seek(start + cursor + offset)
                    data = stream.read(length)
                row = wanted[page, key, KINDS[kind]]
                if (len(data), sha(data)) != (row['size'], row['sha256']):
                    raise ValueError('Original Gekko resource differs: ' + str((page, key)))
                target = destination / (row['sha256'] + row['suffix'])
                atomic(target, data)
                found[row['sha256']] = target
        return found

def selected_source(mgs_source):
    if mgs_source is None:
        raise ValueError('Original stage02.dat or explicitly extracted original stage02 resource directory is required')
    path = Path(mgs_source).resolve(strict=True)
    if path.is_file():
        return (path, 'stage_dat')
    if (path / 'stage02.dat').is_file():
        return (path / 'stage02.dat', 'stage_dat')
    for root in (path, path / 'stage', path / 'stage02/stage'):
        if (root / 's02a80l').is_dir() and (root / 's02a85l').is_dir():
            return (root, 'extracted_original_resources')
    raise ValueError('Selected Gekko original source does not contain stage02.dat or reviewed original pages')

def textures(resources, profile, destination):
    destination.mkdir(parents=True, exist_ok=True)
    containers = {}
    rows = []
    for t in profile['textures']:
        raw = resources[t['txn_sha256']].read_bytes()
        tx = txn(raw)
        if t['txn_texture'] not in tx['textures'] or tx['images'][t['txn_texture']['image_index']] != t['txn_image']:
            raise ValueError('Gekko original TXN descriptor changed')
        parts = []
        for s in t['payloads']:
            data = resources[s['source_sha256']].read_bytes()
            if 'record' in s:
                digest = s['source_sha256']
                if digest not in containers:
                    plain, _ = dlz(data) if data[:4] == b'segs' else (data, [])
                    containers[digest] = (plain, dld(plain))
                plain, records = containers[digest]
                r = s['record']
                if r not in records:
                    raise ValueError('Gekko DLD record changed')
                payload = take(plain, r['offset'] + 32, r['size'])
                offset = r['parent_size']
            else:
                payload = take(data, s['offset'], s['size'])
                offset = 0
            if sha(payload) != s['payload_sha256']:
                raise ValueError('Gekko original texture payload changed')
            parts.append((offset, payload))
        payload = bytearray()
        for offset, part in sorted(parts):
            if offset != len(payload):
                raise ValueError('Gekko mip gap/overlap')
            payload += part
        image = dds(t['width'], t['height'], t['codec'], payload, t['mip_count'])
        if sha(image) != t['dds_sha256'] or sha(model.dds_top(image)[3]) != t['top_sha256']:
            raise ValueError('Gekko original image SHA differs')
        p = destination / f"{t['key']:06x}.dds"
        atomic(p, image)
        rows.append({'texture_key': t['key'], 'dds': file_record(p)})
    save_json(destination / 'texture_manifest.json', {'textures': rows})

def native_sound(name):
    pcm = bytearray()
    state = 1196903247
    for i in range(16800 if name == 'gekko_step.wav' else 48000):
        t = i / 48000
        if name == 'gekko_step.wav':
            state = 1664525 * state + 1013904223 & 4294967295
            noise = (state >> 8) / 8388607.5 - 1
            env = min(1, t / 0.003) * math.exp(-t * 19)
            v = max(-1, min(1, 0.52 * env * (0.8 * math.sin(2 * math.pi * (78 * t - 42 * t * t)) + 0.2 * noise)))
        else:
            env = min(1, t / 0.025) * max(0, 1 - t) ** 2
            v = 0.38 * env * (math.sin(2 * math.pi * (145 * t + 30 * t * t)) + 0.25 * math.sin(2 * math.pi * 291 * t))
        pcm += struct.pack('<hh', int(v * 32767), int(v * 32767))
    out = io.BytesIO()
    with wave.open(out, 'wb') as wav:
        wav.setparams((2, 2, 48000, 0, 'NONE', 'not compressed'))
        wav.writeframes(pcm)
    return out.getvalue()

def verify_gekko_resources(output, source_root=None, mgs_source=None, profile_path=None):
    output = Path(output).resolve()
    errors = []
    assets_checked = 0
    sources_checked = 0
    try:
        r = json.loads((output / 'gekko-result.json').read_text(encoding='utf-8'))
        p = Path(profile_path) if profile_path else Path(__file__).with_name('excv_gekko_profile.json')
        profile = json.loads(p.read_text(encoding='utf-8'))
        if not r.get('complete') or r.get('format') != 'MGO2MTEXCV.GEKKO_RESULT.1':
            raise ValueError('Only completed Gekko result may resume')
        if file_record(p)['sha256'] != r['profile']['sha256']:
            raise ValueError('Gekko recipe changed')
        expected = {a['path']: a for a in profile['assets']}
        if {a['path'] for a in r['assets']} != set(expected) or len(r['assets']) != len(expected):
            raise ValueError('Gekko manifest inventory changed')
        if {p.relative_to(output / 'data').as_posix() for p in (output / 'data').rglob('*') if p.is_file()} != set(expected):
            raise ValueError('Gekko data inventory changed')
        for name, a in expected.items():
            actual = file_record(output / 'data' / name)
            if (actual['size'], actual['sha256']) != (a['size'], a['sha256']):
                raise ValueError('Gekko output changed: ' + name)
            assets_checked += 1
        selected, mode = selected_source(mgs_source if mgs_source is not None else r['original_source'])
        if mode != r['source_mode']:
            raise ValueError('Gekko source mode changed')
        expected_sources = {f"{x['page']}/{x['key']:06x}{x['suffix']}": (x['size'], x['sha256']) for x in profile['resources']}
        if mode == 'stage_dat':
            if len(r['sources']) != 1 or (r['sources'][0]['size'], r['sources'][0]['sha256']) != (profile['source']['size'], profile['source']['sha256']):
                raise ValueError('Gekko original DAT identity differs')
        elif {x.get('relative'): (x['size'], x['sha256']) for x in r['sources']} != expected_sources or len(r['sources']) != len(expected_sources):
            raise ValueError('Gekko source resource inventory changed')
        for source in r['sources']:
            path = selected if mode == 'stage_dat' else selected / source['relative']
            actual = file_record(path)
            if (actual['size'], actual['sha256']) != (source['size'], source['sha256']):
                raise ValueError('Gekko original source changed')
            sources_checked += 1
    except (ValueError, OSError, KeyError, TypeError) as exc:
        errors.append(str(exc))
    return {'passed': not errors, 'assets_checked': assets_checked, 'sources_checked': sources_checked, 'errors': errors}

def build_gekko_resources(source_root, output, mgs_source=None, progress=lambda _: None, cancel=lambda: False, resume=False, profile_path=None):
    original, mode = selected_source(mgs_source)
    output = validate_output(Path(source_root).resolve(), output)
    validate_output(original, output)
    if output.exists() and any(output.iterdir()):
        if not resume:
            raise ValueError('Gekko output must be a new empty folder')
        checked = verify_gekko_resources(output, source_root, mgs_source, profile_path)
        if not checked['passed']:
            raise ValueError('Cannot resume Gekko output: ' + '; '.join(checked['errors']))
        r = json.loads((output / 'gekko-result.json').read_text(encoding='utf-8'))
        r['resumed'] = True
        r['verification'] = checked
        return r
    output.mkdir(parents=True, exist_ok=True)
    p = Path(profile_path) if profile_path else Path(__file__).with_name('excv_gekko_profile.json')
    profile = json.loads(p.read_text(encoding='utf-8'))
    if profile.get('schema') != 'MGO2MTEXCV.GEKKO_RECIPE.1':
        raise ValueError('Gekko recipe version')
    resources = {}
    sources = []
    if mode == 'stage_dat':
        before = file_record(original)
        expected = profile['source']
        if (before['size'], before['sha256']) != (expected['size'], expected['sha256']):
            raise ValueError('Unreviewed original stage02.dat')
        stamp, pages = dat_pages(original)
        wanted = {(r['page'], r['key'], r['suffix']): r for r in profile['resources']}
        for page in sorted({r['page'] for r in profile['resources']}):
            progress('gekko / ' + page)
            resources.update(extract_page(original, page, stamp, pages[page], wanted, output / 'source', cancel))
        if file_record(original) != before:
            raise ValueError('Original stage02.dat changed during conversion')
        sources.append(before)
    else:
        for r in profile['resources']:
            if cancel():
                raise InterruptedError('Gekko conversion cancelled')
            path = original / r['page'] / (f"{r['key']:06x}" + r['suffix'])
            before = file_record(path)
            raw = path.read_bytes()
            if (len(raw), sha(raw)) != (r['size'], r['sha256']) or file_record(path) != before:
                raise ValueError('Extracted original Gekko resource differs')
            target = output / 'source' / (r['sha256'] + r['suffix'])
            atomic(target, raw)
            resources[r['sha256']] = target
            sources.append(dict(before, relative=path.relative_to(original).as_posix()))
    if set(resources) != {r['sha256'] for r in profile['resources']}:
        raise ValueError('Required original Gekko resource absent')
    progress('gekko / original model and textures')
    texture_dir = output / 'working/textures'
    textures(resources, profile, texture_dir)
    previous = model.record
    try:
        model.record = lambda path: previous(path if path.is_file() else Path(sys.executable))
        model.build(resources[profile['mdn_sha256']], texture_dir, output / 'working/model')
    finally:
        model.record = previous
    atomic(output / 'data/special/gekko.gwc', (output / 'working/model/gekko.gwc').read_bytes())
    for bank in profile['banks']:
        if cancel():
            raise InterruptedError('Gekko conversion cancelled')
        raw = bytearray(bank['magic'].encode() + struct.pack('<2I', 1, len(bank['clips'])))
        for row in bank['clips']:
            clip = motion_bytes(resources[row['source_sha256']], row)
            if bank['magic'] != 'GWT1':
                clip = b'GWT1' + struct.pack('<2I', 1, 1) + clip
                raw += struct.pack('<2I', row['slot'], len(clip))
            raw += clip
        atomic(output / 'data' / bank['path'], raw)
    for name in ('gekko_step.wav', 'gekko_salute.wav'):
        atomic(output / 'data/special' / name, native_sound(name))
    assets = []
    for expected in profile['assets']:
        actual = file_record(output / 'data' / expected['path'])
        if (actual['size'], actual['sha256']) != (expected['size'], expected['sha256']):
            raise ValueError('Regenerated Gekko runtime differs: ' + expected['path'])
        assets.append(expected)
    for r in sources:
        actual = file_record(Path(r['path']))
        if (actual['size'], actual['sha256']) != (r['size'], r['sha256']):
            raise ValueError('Original Gekko source changed during conversion')
    report = {'format': 'MGO2MTEXCV.GEKKO_RESULT.1', 'complete': True, 'profile': file_record(p), 'original_source': str(original), 'source_mode': mode, 'sources': sources, 'assets': assets, 'original_banks': 4, 'native_sounds': 2, 'native_boundaries': ['Two WAVs are deterministic native fallback synthesis, not original Gekko cues', 'Native clip/action selection and initial model visibility are preserved, not full original dispatcher/IK/material parity', 'Extracted-source mode verifies all original resource hashes but cannot verify the source DAT container']}
    save_json(output / 'gekko-result.json', report)
    return report
