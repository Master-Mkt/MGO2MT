# Generated converter-only source; historical analysis entry points omitted.
"""Original QQ online-map planes, with the existing bounded native UI mapping.

No original GCX/LA2 dynamic binding is claimed. No original bytes or old FULL
assets are bundled; the selected original stage archives are mandatory inputs.
"""
from pathlib import Path
import argparse, io, json
from PIL import Image
from title_assets import qar, txn, dlz, dld, take
from excv_ui_resources import dds_bytes, png_bytes
from excv_core import atomic, save_json, file_record, sha, read_package, local_root, validate_output
SOURCES = {'cache.qar': ('b4c3737df6de4951bb6d4905c51b9ec3b8aa0014318b6b4df74e0b19643d069c', '1a81ca2e13609955275a2c40fa016bc77b9379556aa069c9eee0d7b2593a1e4b'), 'cache_nodld.dlz': ('4c8bbbeb3dda65e115aa21e1d7d3ae6410f3289d301b883c604a4eabb4d07fea', '069596245fe6d5aad1a2a8de7fd97da6dd5611a5dc46d036abd7b914a2bf3fac')}
PIXELS = ('e2115a3177a1a5993de77548ec220a8cb1085a3e355402de2e8ed2d3588c8740', '7d9e35970f657743fd48009c6502e1cf60e64703777b4298e26d9546c000745f')
KEYS = (9937130, 10519869)

def require(value, why):
    if not value:
        raise ValueError(why)

def verify_briefing_map(destination, source_root=None):
    destination = Path(destination).resolve(strict=True)
    r = json.loads((destination / 'manifest.json').read_text(encoding='utf-8'))
    require(r.get('format') == 'MGO2MTEXCV.BRIEFING_MAP.1' and r.get('complete'), 'Incomplete briefing map output')
    if source_root is not None:
        require(local_root(source_root) == Path(r['source']).resolve(), 'Original stage source changed')
    for row in r['sources']:
        require(file_record(Path(row['source']['path'])) == row['source'], 'Original briefing map archive changed')
    for row in r['files']:
        path = destination / row['path']
        require(destination in path.resolve().parents and (not path.is_symlink()), 'Briefing map path escape')
        actual = file_record(path)
        require((actual['size'], actual['sha256']) == (row['size'], row['sha256']), 'Briefing map output changed')
    return r

def extract_briefing_map(source_root, destination, resume=False, progress=lambda _: None, cancel=lambda: False):
    root = local_root(source_root)
    destination = validate_output(root, destination)
    if resume and (destination / 'manifest.json').exists():
        return verify_briefing_map(destination, root)
    destination.mkdir(parents=True, exist_ok=True)
    out = destination / 'data/briefing-map'
    report = dict(format='MGO2MTEXCV.BRIEFING_MAP.1', source=str(root), complete=False, sources=[], images=[], files=[], stage='n022a', map=20, original_gcx_binding=None, selector=dict(txn='online_map.txn', texture_keys=KEYS, archive=13810624, indices=[0, 1]), limitations=['Original GCX/LA2 dynamic texture binding is unresolved; native map20/icon1,2 assignment is retained.', 'Image roles were visually identified as base map/grid. Original RGBA is unchanged.', 'UI tint/placement, world-to-map transform and player markers are not decoded by this recipe.'])

    def load(name):
        if cancel():
            raise InterruptedError('Conversion cancelled')
        progress('QQ briefing map / ' + name)
        p = root / 'stage/n022a' / name
        before = file_record(p)
        require(before['sha256'] == SOURCES[name][0], 'Reviewed briefing map archive changed')
        data, mode, _ = read_package(p, 'n022a')
        require(sha(data) == SOURCES[name][1] and file_record(p) == before, 'Original briefing map identity changed')
        report['sources'].append(dict(source=before, relative='stage/n022a/' + name, mode=mode, decoded_sha256=sha(data)))
        return data
    try:
        q = load('cache.qar')
        entries = [e for e in qar(q) if e['name'] == 'online_map.txn']
        require(len(entries) == 1, 'Original online-map table count')
        entry = entries[0]
        raw = take(q, entry['offset'], entry['size'])
        table = txn(raw)
        require(len(table['images']) == len(table['textures']) == 2, 'Original online-map image count')
        data, _ = dlz(load('cache_nodld.dlz'))
        require(sha(data) == '05a6112bd6876ff621b80dbaa2ae41f1f5c5166b53fb24d3fdd37789d0b2e210', 'Original map DLD changed')
        records = dld(data)
        for index, texture in enumerate(table['textures']):
            if cancel():
                raise InterruptedError('Conversion cancelled')
            image = table['images'][texture['image_index']]
            require(texture['key'] == KEYS[index] and texture['archive'] == 13810624 and (texture['image_index'] == index), 'Original briefing map selector changed')
            require(texture['width'] == texture['height'] == 512 and (not texture['x']) and (not texture['y']) and (texture['uv_scale'] == [1, 1]) and (texture['uv_offset'] == [0, 0]), 'Original map UV transform')
            require(image['width'] == image['height'] == 512 and image['codec'] == 11 and (image['flags'] >> 4 == 15), 'Original map BC3 extent')
            matches = [r for r in records if r['key'] == texture['archive'] and r['index'] == index and (not r['parent_size'])]
            require(len(matches) == 1, 'Ambiguous original map payload')
            record = matches[0]
            require(record['size'] == 262144, 'Original map mip size')
            payload = take(data, record['offset'] + 32, record['size'])
            pixels = Image.open(io.BytesIO(dds_bytes(image, payload))).convert('RGBA')
            require(sha(pixels.tobytes()) == PIXELS[index], 'Original map decoded pixels changed')
            filename = f'n022a-online-map-{index}.png'
            atomic(out / filename, png_bytes(pixels))
            report['images'].append(dict(file=filename, entry=entry, txn_sha256=sha(raw), texture=texture, image=image, record=record, payload_sha256=sha(payload), rgba_sha256=sha(pixels.tobytes())))
        atomic(out / 'index.tsv', b'MGO2MT_WEAPON_ICONS\t1\nICON\t1\tn022a-online-map-0.png\nICON\t2\tn022a-online-map-1.png\n')
        report['complete'] = True
    finally:
        for p in sorted((destination / 'data').rglob('*')):
            if p.is_file():
                r = file_record(p)
                r['path'] = p.relative_to(destination).as_posix()
                report['files'].append(r)
        save_json(destination / 'manifest.json', report)
    return report
