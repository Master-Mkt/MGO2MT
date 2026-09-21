# Generated converter-only source; historical analysis entry points omitted.
"""Assemble verified module outputs locally. No installed-client asset fallback."""
from pathlib import Path
import json, shutil, uuid
from excv_core import file_record, save_json, atomic

def verify_runtime(destination):
    destination = Path(destination).resolve()
    r = json.loads((destination / 'manifest.json').read_text(encoding='utf-8'))
    if r.get('format') not in ('MGO2MTEXCV.RUNTIME_ASSEMBLY.1', 'MGO2EXCV.RUNTIME_ASSEMBLY.1') or not r.get('complete'):
        raise ValueError('Incomplete runtime assembly')
    for row in r['files']:
        path = (destination / row['path']).resolve()
        if not path.is_relative_to(destination):
            raise ValueError('Runtime path escape')
        actual = file_record(path)
        if (actual['size'], actual['sha256']) != (row['size'], row['sha256']):
            raise ValueError('Runtime asset changed: ' + row['path'])
    return r

def completed_module_files(folder, report):
    """Accept successful partial output only with an exact declared inventory."""
    folder = Path(folder).resolve()
    data = folder / 'data'
    expected = {}
    rows = report.get('files')
    if rows is None:
        rows = [dict(r, path='data/' + r['path']) for r in report.get('assets', [])]
    for row in rows:
        name = row.get('path', '')
        if not isinstance(name, str) or not name.startswith('data/'):
            continue
        parts = name.split('/')
        if any((p in ('', '.', '..') for p in parts)) or ':' in name or '\\' in name or (name in expected):
            raise ValueError('Unsafe partial module inventory')
        path = (folder / name).resolve()
        if not path.is_relative_to(data):
            raise ValueError('Partial module path escape')
        actual = file_record(path)
        if (actual['size'], actual['sha256']) != (row['size'], row['sha256']):
            raise ValueError('Partial module asset differs: ' + name)
        expected[name] = row
    present = {p.relative_to(folder).as_posix() for p in data.rglob('*') if p.is_file()} if data.is_dir() else set()
    if present != set(expected):
        raise ValueError('Partial module inventory differs')
    return bool(expected)

def assemble_runtime(root, modules, *, resume=False, cancel=lambda: False, missing_modules=None):
    root = Path(root).resolve()
    final = root / 'runtime'
    destination = final
    missing_modules = list(missing_modules or [])
    if destination.exists():
        if not resume:
            raise ValueError('Existing runtime is not overwritten')
        old = verify_runtime(destination)
        if old['modules'] == modules and old.get('missing_modules', []) == missing_modules:
            return old
    destination = root / ('runtime.pending-' + uuid.uuid4().hex[:12])
    destination.mkdir(parents=True)
    data = destination / 'data'
    data.mkdir()
    rows = {}
    origins = {}
    for name in modules:
        folder = (root / name / 'data').resolve()
        if not folder.is_relative_to(root):
            raise ValueError('Module path escape')
        if not folder.is_dir():
            raise ValueError('Missing complete module data: ' + name)
        for path in sorted(folder.rglob('*')):
            if cancel():
                raise InterruptedError('Runtime assembly cancelled')
            if not path.is_file():
                continue
            if not path.resolve().is_relative_to(folder):
                raise ValueError('Module file path escape')
            relative = path.relative_to(folder).as_posix()
            r = file_record(path)
            if relative in rows:
                if rows[relative]['sha256'] != r['sha256']:
                    raise ValueError('Conflicting runtime modules: ' + relative)
                origins[relative].append(name)
                continue
            target = data / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
            rows[relative] = dict(path='data/' + relative, size=r['size'], sha256=r['sha256'])
            origins[relative] = [name]
    for alias, number in [('ak102', 25), ('operator', 3)]:
        for suffix in ('', '_secondary'):
            relative = f'weapons/{alias}{suffix}.gwm'
            source = data / f'weapons/id_{number:03d}{suffix}.gwm'
            if source.is_file():
                target = data / relative
                shutil.copy2(source, target)
                r = file_record(target)
                rows[relative] = dict(path='data/' + relative, size=r['size'], sha256=r['sha256'])
                origins[relative] = ['native legacy filename alias']
    hashes = ''.join((r['sha256'] + '  ' + relative + '\n' for relative, r in sorted(rows.items())))
    atomic(data / 'assets.sha256', hashes.encode('ascii'))
    r = file_record(data / 'assets.sha256')
    rows['assets.sha256'] = dict(path='data/assets.sha256', size=r['size'], sha256=r['sha256'])
    requirements = [('network.gnk', 'User-selected local GNK input required', False), ('movie_01.mp4', 'Optional user-supplied opening movie', True), ('gameplay.json', 'Application software package supplies current gameplay configuration', False), ('mounted_weapons.json', 'Application software package supplies mounted weapon configuration', False), ('weapon_effects.json', 'Application software package supplies editable effect configuration', False), ('sfx/native_water_step.wav', 'Native water sound recipe output missing', True)]
    missing = [dict(path=name, reason=reason, optional=optional) for name, reason, optional in requirements if not (data / name).is_file()]
    missing.extend((dict(path='module:' + row['module'], reason=row.get('reason', 'Conversion module incomplete'), optional=False) for row in missing_modules))
    report = dict(format='MGO2MTEXCV.RUNTIME_ASSEMBLY.1', complete=True, ready_to_launch=not [m for m in missing if not m.get('optional')], modules=modules, missing_modules=missing_modules, files=list(rows.values()), origins=origins, missing=missing, original_assets_bundled_in_program=False, optional_not_generated=['Full round BGM WAV library; title/lobby BGM availability is recorded in the file inventory'], scope='Local assembled conversion output. Game assets are never copied from an existing client installation.')
    save_json(destination / 'manifest.json', report)
    verify_runtime(destination)
    previous = None
    if final.exists():
        previous = root / ('runtime.previous-' + uuid.uuid4().hex[:12])
        final.rename(previous)
    try:
        destination.rename(final)
    except BaseException:
        if previous and previous.exists() and (not final.exists()):
            previous.rename(final)
        raise
    return report
