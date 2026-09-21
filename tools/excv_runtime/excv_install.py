# Generated converter-only source; historical analysis entry points omitted.
"""Apply verified local conversion data to MGO2MT software-only installations.

No download, network-key constant, game asset or account credential is embedded.
The selected application's existing settings are retained. Every replacement is
prepared beside data, then committed with the previous complete data as backup.
"""
from pathlib import Path, PurePosixPath
import datetime, hashlib, json, math, os, shutil, struct, uuid
from excv_core import atomic, file_record, save_json
from excv_runtime_assembly import verify_runtime
EXES = {'client': 'MGO2MT.exe', 'host': 'MGO2MTHOST.exe'}
LOCAL_OPTIONAL = ('character/hit_geometry.gwhit', 'motion/evade_travel.gwet', 'weapon/connection_points.gwcp', 'special/gekko_jump.gwjc')
SETTINGS = frozenset(('gameplay.json', 'mounted_weapons.json', 'weapon_effects.json', 'launch.cfg', 'lobbies.cfg', 'weapon_catalog.tsv', 'skill_catalog.tsv', 'combat_health.cfg', 'round_items.cfg', 'breakable_lights.cfg', 'bullet_penetration_profile.json', 'item_drop_policy.json', 'resource-requirements.json'))
TEXT_SUFFIXES = frozenset(('.cfg', '.tsv', '.json', '.gwp'))

def relative(value):
    if not isinstance(value, str) or not value or len(value) > 240 or ('\\' in value) or (':' in value):
        return False
    p = PurePosixPath(value)
    if p.is_absolute() or value != p.as_posix() or any((part in ('', '.', '..') or part.endswith(('.', ' ')) for part in p.parts)):
        return False
    for part in p.parts:
        stem = part.split('.')[0].upper()
        if stem in {'CON', 'PRN', 'AUX', 'NUL', *[f'COM{i}' for i in range(1, 10)], *[f'LPT{i}' for i in range(1, 10)]}:
            return False
    return all((ord(c) >= 32 and c not in '<>"|?*' for c in value))

def no_links(path):
    path = Path(path).absolute()
    for p in (path, *path.parents):
        if p.exists() and (p.is_symlink() or (hasattr(p, 'is_junction') and p.is_junction())):
            raise ValueError('Links/junctions are not installation destinations: ' + str(p))
    return path.resolve()

def files_under(folder):
    files = []
    if not folder.exists():
        return files
    for base, dirs, names in os.walk(folder, followlinks=False):
        for name in dirs + names:
            no_links(Path(base) / name)
        files.extend((Path(base) / name for name in names))
        if len(files) > 30000:
            raise ValueError('Installation file-count limit')
    return files

def external_file(path, name):
    path = no_links(path)
    if not path.is_file():
        raise ValueError('Missing user-local input: ' + name)
    size = path.stat().st_size
    with path.open('rb') as f:
        head = f.read(32)
    if name == 'network.gnk':
        if size != 8380 or head[:16] != b'GNK1' + struct.pack('>III', 1042, 2, 0):
            raise ValueError('network.gnk does not match the reviewed GNK1 layout')
    elif name == 'character/hit_geometry.gwhit':
        if size != 6576 or head[:8] != b'GWHIT1\x00\x00' or struct.unpack_from('<4I', head, 8) != (1, 6, 21, 0):
            raise ValueError('hit_geometry.gwhit header/extent differs')
        raw = path.read_bytes()
        reference = []
        for pose in range(6):
            keys = []
            for bone in range(21):
                values = struct.unpack_from('<I12f', raw, 24 + (pose * 21 + bone) * 52)
                key = values[0]
                axes = [values[1 + 3 * i:4 + 3 * i] for i in range(3)]
                origin = values[10:13]
                if not 0 < key <= 16777215 or key in keys or (not all((math.isfinite(v) for v in values[1:]))) or any((abs(v) > 5000 for v in origin)):
                    raise ValueError('hit_geometry.gwhit record invalid')
                if any((abs(sum((v * v for v in axis)) - 1) > 0.0001 for axis in axes)) or any((abs(sum((a * b for a, b in zip(axes[i], axes[j])))) > 0.0001 for i, j in ((0, 1), (0, 2), (1, 2)))):
                    raise ValueError('hit_geometry.gwhit axes invalid')
                a, b, c = axes
                cross = (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
                if sum((x * y for x, y in zip(cross, c))) <= 0.9999:
                    raise ValueError('hit_geometry.gwhit basis invalid')
                keys.append(key)
            if pose == 0:
                reference = keys
            elif keys != reference:
                raise ValueError('hit_geometry.gwhit pose keys differ')
    elif name == 'motion/evade_travel.gwet':
        if size != 372 or head[:8] != b'GWEVAD1\x00' or struct.unpack_from('<4I', head, 8) != (1, 41, 46, 0):
            raise ValueError('evade_travel.gwet header/extent differs')
        values = struct.unpack_from('<87f', path.read_bytes(), 24)
        if any((not math.isfinite(v) or abs(v) > 100000 for v in values)):
            raise ValueError('evade_travel.gwet samples invalid')
        total = 0
        for curve, limit in ((values[:41], 40), (values[41:], 35)):
            total += sum((min(100, max(0, curve[i] - curve[i - 1])) for i in range(1, limit + 1)))
        if not 0 < total <= 8500:
            raise ValueError('evade_travel.gwet travel invalid')
    elif name == 'weapon/connection_points.gwcp':
        if size < 76 or head[:4] != b'GCP1':
            raise ValueError('connection_points.gwcp header invalid')
        version, count = struct.unpack_from('<II', head, 4)
        if version != 1 or not 1 <= count <= 4096 or size != 12 + 64 * count:
            raise ValueError('connection_points.gwcp extent invalid')
        raw = path.read_bytes()
        seen = set()
        models = {}
        for i in range(count):
            at = 12 + 64 * i
            weapon, key = struct.unpack_from('<II', raw, at)
            values = struct.unpack_from('<6f', raw, at + 40)
            if not 1 <= weapon <= 511 or not 0 <= key <= 16777215 or (weapon, key) in seen or (raw[at + 8:at + 40] == bytes(32)):
                raise ValueError('connection_points.gwcp identity invalid')
            if any((not math.isfinite(v) or abs(v) > 1000000.0 for v in values)) or sum(((values[j + 3] - values[j]) ** 2 for j in range(3))) < 1e-08:
                raise ValueError('connection_points.gwcp axis invalid')
            digest = raw[at + 8:at + 40]
            if weapon in models and models[weapon] != digest:
                raise ValueError('connection_points.gwcp model identity differs')
            models[weapon] = digest
            seen.add((weapon, key))
    elif name == 'special/gekko_jump.gwjc':
        if size != 776 or head[:8] != b'GWGJ1\x00\x00\x00' or struct.unpack_from('<II', head, 8) != (1, 190):
            raise ValueError('gekko_jump.gwjc header/extent differs')
        values = struct.unpack_from('<190f', path.read_bytes(), 16)
        if any((not math.isfinite(v) or not 0 <= v <= 20000 for v in values)) or values[0] != 0 or values[-1] != 0:
            raise ValueError('gekko_jump.gwjc samples invalid')
    elif name == 'movie_01.mp4':
        if not 32 <= size <= 4 * 1024 ** 3 or head[4:8] != b'ftyp':
            raise ValueError('movie_01.mp4 is not a supported MP4 container')
    else:
        raise ValueError('Unapproved external input name')
    return path

def requirements(folder, role):
    for path in (folder / 'resource-requirements.json', folder / 'data/resource-requirements.json'):
        if not path.is_file():
            continue
        value = json.loads(path.read_text(encoding='utf-8-sig'))
        if value.get('format') != 'MGO2MT.ResourceRequirements' or value.get('version') != 1 or value.get('role') != role:
            raise ValueError('Application resource requirements do not match its role')
        rows = value.get('files')
        seen = set()
        if not isinstance(rows, list) or len(rows) > 30000:
            raise ValueError('Resource requirement list invalid')
        for row in rows:
            if not isinstance(row, dict) or not relative(row.get('path')) or row['path'] in seen or (not isinstance(row.get('optional', False), bool)):
                raise ValueError('Unsafe or duplicate resource requirement')
            seen.add(row['path'])
        return (rows, True)
    names = ['network.gnk', 'lobbies.cfg', 'gameplay.json', 'mounted_weapons.json', 'weapon_effects.json']
    names.extend(('launch.cfg', 'title/frame.m2pv', 'character/appearance.gwc', 'character/player.gwmot', 'weapons/hands.gwh', 'fx/original.gwfx', 'fx/damage.gwfx', 'special/gekko.gwc') if role == 'client' else ())
    return ([dict(path=name, optional=False) for name in names] + [dict(path=name, optional=True) for name in LOCAL_OPTIONAL] + ([dict(path='movie_01.mp4', optional=True)] if role == 'client' else []), False)

def host_asset(name):
    if name in SETTINGS:
        return True
    if not name.startswith('stage/'):
        return False
    return name.endswith(('.cfg', '.gww')) or name in ('stage/n007a.gwm', 'stage/objects/n007a_light_a0.gwm', 'stage/objects/n007a_light_b0.gwm')

def native_brand(raw, name):
    if Path(name).suffix not in TEXT_SUFFIXES:
        return raw
    try:
        text = raw.decode('utf-8')
    except UnicodeError:
        return raw
    return text.replace('MGO2WIN', 'MGO2MT').replace('MGO2EXCV', 'MGO2MTEXCV').encode('utf-8')

def install(runtime, folder, role, *, local_data=None, progress=lambda _: None, cancel=lambda: False):
    if role not in EXES:
        raise ValueError('Choose client or host')
    runtime = Path(runtime).resolve()
    if (runtime / 'runtime/manifest.json').is_file():
        runtime = runtime / 'runtime'
    manifest = verify_runtime(runtime)
    folder = no_links(folder)
    if not (folder / EXES[role]).is_file():
        raise ValueError(f'{EXES[role]} があるフォルダーを指定してください。')
    if folder == runtime or runtime.is_relative_to(folder) or folder.is_relative_to(runtime):
        raise ValueError('Conversion and application folders must be separate')
    data = folder / 'data'
    if data.exists() and (not data.is_dir()):
        raise ValueError('Application data must be a directory')
    oldfiles = files_under(data)
    needed, authoritative = requirements(folder, role)
    needed_paths = {r['path'] for r in needed}
    source_rows = []
    seen = set()
    for row in manifest['files']:
        name = row['path']
        if not isinstance(name, str) or not name.startswith('data/') or (not relative(name[5:])) or (name in seen):
            raise ValueError('Unsafe runtime inventory')
        seen.add(name)
        name = name[5:]
        if name == 'assets.sha256':
            continue
        if name in ('network.gnk', 'movie_01.mp4'):
            raise ValueError('External network/movie input must be selected separately')
        if role == 'host' and (not (name in needed_paths if authoritative else host_asset(name))):
            continue
        source_rows.append((name, row))
    maximum = sum((p.stat().st_size for p in oldfiles)) + sum((row['size'] for _, row in source_rows))
    if maximum > 50000000000:
        raise ValueError('Installation staging exceeds 50 GB')
    if shutil.disk_usage(folder).free < maximum + 128 * 1024 * 1024:
        raise ValueError('Insufficient space for staged data and its backup')
    stamp = datetime.datetime.now().strftime('%Y%m%d%H%M%S') + '-' + uuid.uuid4().hex[:8]
    pending = folder / ('data.excv-stage-' + stamp)
    pending.mkdir()
    copied = []
    preserved = []
    try:
        for path in oldfiles:
            if cancel():
                raise InterruptedError('Installation cancelled before commit')
            target = pending / path.relative_to(data)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
        for name, row in source_rows:
            if cancel():
                raise InterruptedError('Installation cancelled before commit')
            target = pending / name
            if name in SETTINGS and target.is_file():
                preserved.append(name)
                continue
            source = runtime / 'data' / name
            if file_record(source)['sha256'] != row['sha256']:
                raise ValueError('Verified source changed while installing: ' + name)
            progress(role + ' / ' + name)
            target.parent.mkdir(parents=True, exist_ok=True)
            if Path(name).suffix in TEXT_SUFFIXES and source.stat().st_size <= 16 * 1024 * 1024:
                atomic(target, native_brand(source.read_bytes(), name))
            else:
                shutil.copy2(source, target)
            copied.append(name)
        if local_data:
            local = no_links(local_data)
            for name in ('network.gnk', 'movie_01.mp4', *LOCAL_OPTIONAL) if role == 'client' else ('network.gnk', *LOCAL_OPTIONAL):
                if (pending / name).is_file() or not (local / name).exists():
                    continue
                source = external_file(local / name, name)
                (pending / name).parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, pending / name)
                copied.append(name)
        for name in ('network.gnk', 'movie_01.mp4', *LOCAL_OPTIONAL):
            if (pending / name).is_file():
                external_file(pending / name, name)
        missing = [dict(path=r['path'], optional=r.get('optional', False)) for r in needed if not (pending / r['path']).is_file()]
        payload = [p for p in files_under(pending) if p.name not in ('assets.sha256', 'excv-install.json')]
        hashes = ''.join((file_record(p)['sha256'] + '  ' + p.relative_to(pending).as_posix() + '\n' for p in sorted(payload)))
        atomic(pending / 'assets.sha256', hashes.encode('utf-8'))
        backup = folder / ('data.before-excv-' + stamp)
        result = dict(format='MGO2MTEXCV.INSTALL.1', role=role, completed=True, resource_files_complete=not any((not r['optional'] for r in missing)), application_check_performed=False, additional_original_conversion_supported=False, optional_local_resource_names=list(LOCAL_OPTIONAL), requirements_manifest_present=authoritative, copied=copied, preserved_settings=preserved, missing=missing, source_modules=manifest.get('modules', []), conversion_missing=manifest.get('missing', []), previous_data_backup=backup.name if data.exists() else None, scope='Local resource application only; software, original source files and existing application settings are unchanged; no network access.')
        save_json(pending / 'excv-install.json', result)
        if cancel():
            raise InterruptedError('Installation cancelled before commit')
        if data.exists():
            data.rename(backup)
        try:
            pending.rename(data)
        except BaseException:
            if backup.exists() and (not data.exists()):
                backup.rename(data)
            raise
        return result
    except BaseException as error:
        if pending.exists():
            save_json(pending / 'installation-incomplete.json', dict(error=str(error), existing_application_data_unchanged=True))
        raise
