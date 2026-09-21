# Generated converter-only source; historical analysis entry points omitted.
"""Portable runtime UI placement/crops from verified original UI extraction.

No installed FULL assets or analysis-work folders are read by this module.
Original glyphs and native briefing geometry retain separate provenance.
"""
from pathlib import Path
import argparse, io, json
from PIL import Image
from excv_core import atomic, save_json, file_record, sha, local_root, validate_output
from excv_ui_resources import Sources, verify_ui_resources, png_bytes, load_package
from excv_ui_profile import REVIEWED_SOURCES
from la2_inspect import Layout
from title_assets import take
PROFILE = Path(__file__).with_name('excv_ui_runtime_profile.json')

def require(yes, why):
    if not yes:
        raise ValueError(why)

def verify_ui_runtime(destination, source_root=None, ui_output=None):
    destination = Path(destination).resolve(strict=True)
    r = json.loads((destination / 'manifest.json').read_text(encoding='utf-8'))
    require(r.get('format') == 'MGO2MTEXCV.UI_RUNTIME.1' and r.get('complete'), 'Incomplete UI runtime')
    require(r['profile_sha256'] == sha(PROFILE.read_bytes()), 'UI runtime recipe changed')
    if source_root is not None:
        require(local_root(source_root) == Path(r['source']).resolve(), 'UI runtime source changed')
    if ui_output is not None:
        require(Path(ui_output).resolve() == Path(r['ui_output']).resolve(), 'Raw UI source changed')
    verify_ui_resources(r['ui_output'], r['source'])
    require(file_record(Path(r['raw_manifest']['path'])) == r['raw_manifest'], 'Raw UI manifest changed')
    for row in r['files']:
        path = destination / row['path']
        require(destination in path.resolve().parents and (not path.is_symlink()), 'UI runtime path escape')
        actual = file_record(path)
        require((actual['size'], actual['sha256']) == (row['size'], row['sha256']), 'UI runtime output changed: ' + row['path'])
    for row in r['additional_sources']:
        require(file_record(Path(row['source']['path'])) == row['source'], 'Additional UI original changed')
    return r

def extract_ui_runtime(source_root, ui_output, destination, resume=False, progress=lambda _: None, cancel=lambda: False):
    root = local_root(source_root)
    ui = Path(ui_output).resolve(strict=True)
    destination = validate_output(root, destination)
    validate_output(ui, destination)
    if resume and (destination / 'manifest.json').exists():
        return verify_ui_runtime(destination, root, ui)
    raw_report = verify_ui_resources(ui, root)
    profile = json.loads(PROFILE.read_text(encoding='utf-8'))
    destination.mkdir(parents=True, exist_ok=True)
    out = destination / 'data'
    rows = []
    errors = []
    report = dict(format='MGO2MTEXCV.UI_RUNTIME.1', source=str(root), ui_output=str(ui), raw_manifest=file_record(ui / 'manifest.json'), profile_sha256=sha(PROFILE.read_bytes()), complete=False, assets=rows, additional_sources=[], files=[], errors=errors, limitations=['Original skill star is shared by 25 native skill IDs; individual original skill icon association remains unverified.', 'Briefing toolbar 14 state images are existing native geometry, not original images.', 'Original Latin glyph shape/UV is preserved; hold-selector role, color and size are native.', 'No new original menu layout, transition or glyph mapping is claimed. Title frame conversion is separate.'])
    raw_files = {x['path']: x for x in raw_report['files']}

    def read_raw(relative):
        require(relative in raw_files, 'Required UI source is absent: ' + relative)
        p = ui / relative
        b = p.read_bytes()
        require(sha(b) == raw_files[relative]['sha256'], 'Raw UI byte identity changed')
        return b

    def emit_png(relative, image, expected=None, proof=None, native=False):
        require(image.mode == 'RGBA', 'RGBA source required')
        if expected:
            require(sha(image.tobytes()) == expected, 'Original runtime glyph changed: ' + relative)
        atomic(out / relative, png_bytes(image))
        rows.append(dict(path=relative, rgba_sha256=sha(image.tobytes()), size=list(image.size), provenance='native geometry' if native else 'original asset', proof=proof))

    def index(relative, values):
        text = 'MGO2MT_WEAPON_ICONS\t1\n' + ''.join((f'ICON\t{i}\t{name}\n' for i, name in values))
        atomic(out / relative, text.encode('utf-8'))

    def job(name, fn):
        if cancel():
            raise InterruptedError('Conversion cancelled')
        progress(name)
        try:
            fn()
        except InterruptedError:
            raise
        except Exception as e:
            errors.append(dict(component=name, error=str(e)))

    def icons():
        detail = json.loads(read_raw('r_onlinelobby/report.json'))
        verified = {(x['domain'], x['id']): x for x in detail['icons']}
        indexes = {'weapon': [], 'equipment': []}
        display = []
        for x in profile['icons']:
            domain = x['domain']
            ident = x['id']
            raw = f'r_onlinelobby/icons/{domain}/{domain}_{ident}.png'
            original = verified[domain, ident]
            require(original['status'] == 'verified' and original['layout_sha256'] == x['layout_sha256'], 'Original icon layout changed')
            b = read_raw(raw)
            image = Image.open(io.BytesIO(b)).convert('RGBA')
            relative = f'{domain}-icons/{domain}_{ident}.png'
            emit_png(relative, image, x['rgba_sha256'], dict(raw=raw, source=original['proof']))
            indexes[domain].append((ident, Path(relative).name))
            if domain == 'equipment':
                proof = original['proof']
                extent = proof.get('display_extent')
                if extent is None:
                    a, c, d, e = proof['bounds_xyxy']
                    extent = [d - a, e - c]
                expected = next((v[1:] for v in profile['equipment_display'] if v[0] == ident))
                require(extent == expected, 'Equipment authored extent changed')
                display.append([ident, *extent])
        for domain, values in indexes.items():
            index(domain + '-icons/index.tsv', values)
        atomic(out / 'equipment-icons/display.tsv', ('MGO2MT_EQUIPMENT_DISPLAY 1\n' + ''.join((' '.join((format(v, '.12g') for v in row)) + '\n' for row in display))).encode())

    def fonts():
        for x in profile['fonts']:
            relative = 'fonts/' + x['name']
            b = read_raw(relative)
            require(sha(b) == x['sha256'], 'Original font revision changed')
            atomic(out / relative, b)
            rows.append(dict(path=relative, provenance='original font', source=next((r for r in raw_report['fonts']['files'] if Path(r['output']['path']).name == x['name']))))

    def controller():
        x = profile['controller']
        raw = f"lobby/crops/{x['layout']:06x}_{x['texture']:06x}.png"
        emit_png('system-ui/controller.png', Image.open(io.BytesIO(read_raw(raw))).convert('RGBA'), x['rgba_sha256'], dict(raw=raw))
        index('system-ui/index.tsv', [(0, 'controller.png')])

    def glyphs():
        sources = Sources(root, 'lobby', destination / 'audit/lobby', REVIEWED_SOURCES, progress, cancel)
        report['additional_sources'] = sources.records
        sources.texture_refs = {key: [row for row in values if row[0] == 0] for key, values in sources.texture_refs.items()}

        def crop(key):
            atlas, proof = sources.resolve(key)
            x = proof['texture']
            bounds = (x['x'], x['y'], x['x'] + x['width'], x['y'] + x['height'])
            require(bounds[0] >= 0 and bounds[1] >= 0 and (bounds[2] <= atlas.width) and (bounds[3] <= atlas.height), 'Original glyph crop bounds')
            return (atlas.crop(bounds), dict(texture_key=key, crop=bounds, source=proof))
        for x in profile['skills']:
            image, proof = crop(x['key'])
            require(list(image.size) == x['size'], 'Original skill glyph size')
            emit_png('skills/' + x['name'] + '.png', image, x['rgba_sha256'], proof)
        index('skills/index.tsv', [(i, 'skill_star.png') for i in range(1, 26)])
        x = profile['hold']
        path = root / 'stage/lobby/cache.dar'
        archive, entries, layout_proof = load_package(root, path, 'lobby', REVIEWED_SOURCES)
        entry = next((e for e in entries if e['name'] == 'indication_item.la2'))
        raw = take(archive, int(entry['offset'], 0), entry['size'])
        require(sha(raw) == x['layout_sha256'], 'Original hold font LA2 changed')
        font = next((f for f in Layout(raw).fonts if f['key'] == x['font']))
        atlas, proof = crop(x['texture'])
        require(list(atlas.size) == x['atlas_size'], 'Original hold atlas dimensions')
        ids = []
        for code in range(x['first'], x['last'] + 1):
            glyph = next((g for g in font['glyphs'] if g['char'] == chr(code)))
            u0, u1, v0, v1 = glyph['uv']
            bounds = [round(u0 * atlas.width / 65535), round(v0 * atlas.height / 65535), round(u1 * atlas.width / 65535), round(v1 * atlas.height / 65535)]
            image = atlas.crop(bounds)
            require(list(image.size) == x['glyph_size'], 'Original hold glyph dimensions')
            name = f'glyph_{code}.png'
            emit_png('hold-font/' + name, image, proof=dict(font=x['font'], layout_sha256=sha(raw), layout_entry=entry, layout_source=sources.use(path), glyph=glyph, crop=bounds, texture=proof))
            ids.append((code, name))
        index('hold-font/index.tsv', ids)

    def briefing():
        from extract_briefing_icons import mask, colored, NAMES, COLORS
        ids = []
        for ident, name in enumerate(NAMES, 1):
            alpha = mask(name)
            for state, color in COLORS.items():
                file = name + '_' + state + '.png'
                emit_png('skills/' + file, colored(alpha, color), proof=dict(native_color=color, recipe='extract_briefing_icons.mask/colored'), native=True)
                ids.append((ident + (100 if state == 'selected' else 0), file))
        index('skills/briefing.tsv', ids)
    try:
        for name, fn in [('Original weapon/equipment icons', icons), ('Original fonts', fonts), ('Original controller', controller), ('Original skill/Latin glyphs', glyphs), ('Native briefing geometry', briefing)]:
            job(name, fn)
        report['complete'] = not errors
    finally:
        for path in sorted(destination.rglob('*')):
            if not path.is_file() or path.name == 'manifest.json':
                continue
            row = file_record(path)
            row['path'] = path.relative_to(destination).as_posix()
            report['files'].append(row)
        save_json(destination / 'manifest.json', report)
    return report
