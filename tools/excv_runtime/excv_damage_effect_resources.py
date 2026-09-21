# Generated converter-only source; historical analysis entry points omitted.
"""Lossless original blood effect images for the native damage presentation.

Input is an already decoded original stage source directory (cache_files and
*.dlz.dld). This tool never writes to that directory or an existing output.
CPEF aligned texture references establish image dependencies, not particle VM
semantics. Runtime role, atlas frames, timing and movement are native choices.
"""
from pathlib import Path
import argparse
import hashlib
import io
import json
import struct
from title_assets import dld, txn, take
from excv_effect_resources import make_bundle, select_payload, texture_record

def sha(data):
    return hashlib.sha256(data).hexdigest()

def record(path):
    data = path.read_bytes()
    return dict(path=str(path.resolve()), size=len(data), sha256=sha(data))

def preview(payload, image):
    from PIL import Image
    width, height = (image['width'], image['height'])
    header = [124, 528391, height, width, len(payload), 0, 1] + [0] * 11
    header += [32, 4, int.from_bytes(b'DXT1' if image['codec'] == 9 else b'DXT5', 'little'), 0, 0, 0, 0, 0]
    header += [4096, 0, 0, 0, 0]
    with Image.open(io.BytesIO(b'DDS ' + struct.pack('<31I', *header) + payload)) as decoded:
        return decoded.convert('RGBA')

def extract(source, destination, previews=False):
    source = Path(source).resolve(strict=True)
    destination = Path(destination).resolve()
    if destination == source or source in destination.parents:
        raise ValueError('Output must be separate from original source')
    if destination.exists():
        raise ValueError('Select a new output directory; existing output is preserved')
    folder = source / 'cache_files'
    table_path = folder / 'effect.txn'
    table = txn(table_path.read_bytes())
    texture_keys = {t['key'] for t in table['textures']}
    cpef = []
    wanted = set()
    for path in sorted(folder.glob('ef_bld_*.cpef')):
        raw = path.read_bytes()
        if len(raw) > 1024 * 1024:
            raise ValueError('Blood CPEF extent')
        refs = [dict(offset=i, key=struct.unpack_from('>I', raw, i)[0]) for i in range(0, len(raw) - 3, 4) if struct.unpack_from('>I', raw, i)[0] in texture_keys]
        wanted.update((row['key'] for row in refs))
        cpef.append(dict(name=path.name, source=record(path), texture_word_matches=refs))
    if not cpef or not wanted or len(cpef) > 64 or (len(wanted) > 128):
        raise ValueError('Blood dependencies missing or excessive')
    needed = {key: texture_record(key, table) for key in wanted}
    payloads = {}
    sources = []
    for path in sorted(source.glob('*.dlz.dld')):
        data = path.read_bytes()
        rows = dld(data)
        proof = record(path)
        used = False
        for key, (texture, image, size) in needed.items():
            for row in rows:
                if row['key'] != texture['archive'] or row['index'] != texture['image_index'] or row['parent_size'] or (row['size'] < size):
                    continue
                payload = take(data, row['offset'] + 32, size)
                payloads[key] = select_payload(payloads.get(key), payload, dict(source=proof, record=row), row['priority'])
                used = True
        if used:
            sources.append(proof)
    bundle, images = make_bundle(sorted(wanted), table, payloads)
    destination.mkdir(parents=True)
    bundle_path = destination / 'data/fx/damage.gwfx'
    bundle_path.parent.mkdir(parents=True)
    bundle_path.write_bytes(bundle)
    evidence = destination / 'source'
    evidence.mkdir()
    for row in cpef:
        raw = (folder / row['name']).read_bytes()
        if sha(raw) != row['source']['sha256']:
            raise ValueError('Source changed during conversion')
        (evidence / row['name']).write_bytes(raw)
    if previews:
        from PIL import Image, ImageDraw
        image_dir = destination / 'images'
        image_dir.mkdir()
        sheet = Image.new('RGB', (320 * 5, 304 * ((len(images) + 4) // 5)), '#747474')
        draw = ImageDraw.Draw(sheet)
        for index, image in enumerate(images):
            key = image['key']
            decoded = preview(payloads[key][1], image['image'])
            decoded.save(image_dir / f'{key:06x}.png')
            decoded.thumbnail((280, 256))
            x, y = (index % 5 * 320, index // 5 * 304)
            sheet.paste(decoded, (x + (320 - decoded.width) // 2, y + 24), decoded)
            draw.text((x + 12, y + 4), f"{key:06x}  {image['image']['width']}x{image['image']['height']}", fill='white')
        sheet.save(image_dir / 'blood_original_atlases.png')
    report = dict(format='MGO2MTEXCV.DAMAGE_EFFECT_RESOURCES.1', complete=True, txn=record(table_path), sources=sources, cpef=cpef, images=images, bundle=record(bundle_path), limitations=['Original BC top mip bytes are unchanged; alpha is preserved.', 'Aligned CPEF texture-key references are recorded exactly; CPEF bytecode and emitter controls are not emulated.', 'Native presentation chooses emitter role, size, atlas frame, tint, lifetime and movement; no original equivalence is claimed.', 'PNG previews decode original BC data for inspection only; runtime uses the BC data.'])
    (destination / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return report
