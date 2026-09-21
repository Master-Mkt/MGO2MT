# Generated converter-only source; historical analysis entry points omitted.
"""Build local stage data from selected original files and verified EXCV stages."""
from pathlib import Path
import contextlib, json, shutil
from excv_core import local_root, validate_output, extract_stage, file_record, save_json, atomic
from excv_stage_runtime import compile_runtime, PROFILES

def verify_stage_data(destination):
    destination = Path(destination).resolve()
    m = json.loads((destination / 'manifest.json').read_text(encoding='utf-8'))
    if m.get('format') != 'MGO2MTEXCV.STAGE_DATA.1' or not m.get('complete'):
        raise ValueError('Incomplete stage data')
    for row in m['files']:
        path = (destination / row['path']).resolve()
        if not path.is_relative_to(destination):
            raise ValueError('Stage data path escape')
        actual = file_record(path)
        if (actual['size'], actual['sha256']) != (row['size'], row['sha256']):
            raise ValueError('Stage data changed: ' + row['path'])
    for row in m['inputs']:
        if file_record(Path(row['path'])) != row:
            raise ValueError('Stage input changed')
    return m

def build_stage_data(source_root, conversion_root, destination, stages, elf_path=None, progress=lambda _: None, cancel=lambda: False, resume=False):
    source = local_root(source_root)
    root = Path(conversion_root).resolve()
    destination = validate_output(source, destination)
    if destination.exists():
        if not resume:
            raise ValueError('Use a new stage data destination')
        old = verify_stage_data(destination)
        if old['stages'] != list(stages) or old['elf'] != (str(Path(elf_path).resolve()) if elf_path else None):
            raise ValueError('Stage data selection changed')
        return old
    destination.mkdir(parents=True)
    assets = destination / 'data/stage'
    assets.mkdir(parents=True)
    report = dict(format='MGO2MTEXCV.STAGE_DATA.1', complete=False, stages=list(stages), elf=str(Path(elf_path).resolve()) if elf_path else None, inputs=[], runtime=[], missing=[], game_maps_enabled=[1, 4, 7, 20, 21], network_used=False, limitations=['All 21 stages are converted and preserved; only the existing five reviewed game maps remain enabled.', 'Original unsupported materials and texture gaps remain in each base stage manifest.'])
    seen = {}

    def copy(path, relative):
        target = assets / relative
        if not target.resolve().is_relative_to(assets.resolve()):
            raise ValueError('Stage resource path escape')
        r = file_record(path)
        if relative in seen:
            if seen[relative] != r['sha256']:
                raise ValueError('Stage resource name collision: ' + relative)
            return
        seen[relative] = r['sha256']
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
    try:
        if elf_path:
            report['inputs'].append(file_record(Path(elf_path)))
        for stage in stages:
            if cancel():
                raise InterruptedError('Stage data cancelled')
            progress('stage runtime / ' + stage)
            converted = root / 'stages' / stage
            manifest = json.loads((converted / 'manifest.json').read_text(encoding='utf-8'))
            report['inputs'].append(file_record(converted / 'manifest.json'))
            report['inputs'].extend(manifest['sources'])
            for row in manifest['files']:
                path = converted / row['path']
                actual = file_record(path)
                if (actual['size'], actual['sha256']) != (row['size'], row['sha256']):
                    raise ValueError('Converted stage changed')
                if path.suffix == '.gwm' or path.name.endswith(('.scene.json', '.collision.cfg', '.lighting.cfg')):
                    copy(path, path.name)
            scene = converted / (stage + '.scene.json')
            if scene.is_file():
                chunks = json.loads(scene.read_text(encoding='utf-8'))['chunks']
                lines = ['MGO2MT.STAGE_CHUNKS 1', 'COUNT ' + str(len(chunks))]
                for chunk in chunks:
                    path = converted / chunk['file']
                    actual = file_record(path)
                    if actual['sha256'] != chunk['sha256']:
                        raise ValueError('Chunk digest differs')
                    lines.append('CHUNK ' + chunk['file'] + ' ' + chunk['sha256'] + ' ' + ' '.join((format(x, '.9g') for x in chunk['bounds'])))
                atomic(assets / (stage + '.chunks.cfg'), ('\n'.join(lines + ['END']) + '\n').encode())
            if stage not in PROFILES:
                continue
            if not elf_path:
                report['missing'].append(dict(stage=stage, reason='Original ELF required for runtime recipe'))
                continue
            work = destination / 'working' / stage
            work.mkdir(parents=True)
            merged, extraction = extract_stage(source, stage, work / 'source', progress, cancel)
            report['inputs'].extend((r['source'] for r in extraction['records']))
            with (work / 'runtime.log').open('w', encoding='utf-8') as log, contextlib.redirect_stdout(log):
                runtime = compile_runtime(stage, merged, work / 'runtime', source, elf_path, converted)
            for row in runtime['files']:
                path = Path(row['path'])
                copy(path, path.relative_to(work / 'runtime').as_posix())
            report['runtime'].append(dict(stage=stage, files=len(runtime['files']), report=str(work / 'runtime/runtime.json')))
        report['inputs'] = list({r['path']: r for r in report['inputs']}.values())
        report['complete'] = not report['missing']
    finally:
        report['files'] = []
        for path in sorted((destination / 'data').rglob('*')):
            if path.is_file():
                r = file_record(path)
                r['path'] = path.relative_to(destination).as_posix()
                report['files'].append(r)
        report['assets'] = [dict(r, path=r['path'].removeprefix('data/')) for r in report['files']]
        save_json(destination / 'manifest.json', report)
    return report
