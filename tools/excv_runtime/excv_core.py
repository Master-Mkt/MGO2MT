# Generated converter-only source; historical analysis entry points omitted.
"""Local-only resource extraction. No downloader and no original asset payloads."""
from pathlib import Path
import hashlib, json, math, os, re, struct, tempfile, threading
from archive_inventory import dar
from title_assets import qar, dlz, take
from stage_scene import geometry
VERSION = '0.1.1'
STAGES = ('n001a', 'n002a', 'n003a', 'n004a', 'n005a', 'n006a', 'n007a', 'n008a', 'n009a', 'n010a', 'n018a', 'n012a', 'n013a', 'n014a', 'n015a', 'sm_dd', 'sm_ll', 'n024a', 'n022a', 'n023a', 'n020a')
MAX_FILE = 1024 * 1024 * 1024

def sha(data):
    return hashlib.sha256(data).hexdigest()

def file_record(path):
    with path.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    return dict(path=str(path.resolve()), size=path.stat().st_size, sha256=digest)

def save_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)

    def portable(item):
        if isinstance(item, float) and (not math.isfinite(item)):
            return {'float32_bits': struct.pack('>f', item).hex(), 'nonfinite': str(item)}
        if isinstance(item, dict):
            return {k: portable(v) for k, v in item.items()}
        if isinstance(item, (list, tuple)):
            return [portable(v) for v in item]
        return item
    atomic(path, (json.dumps(portable(value), ensure_ascii=False, indent=2, allow_nan=False) + '\n').encode('utf-8'))

def atomic(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, name = tempfile.mkstemp(prefix=path.name + '.', suffix='.tmp', dir=path.parent)
    try:
        with os.fdopen(fd, 'wb') as f:
            f.write(data)
        os.replace(name, path)
    finally:
        if os.path.exists(name):
            os.unlink(name)

def safe_name(name):
    if not re.fullmatch('[A-Za-z0-9_][A-Za-z0-9_.-]{0,199}', name) or name.endswith('.'):
        raise ValueError('Unsafe resource name: ' + repr(name))
    if name.split('.')[0].upper() in {'CON', 'PRN', 'AUX', 'NUL', *[f'COM{i}' for i in range(1, 10)], *[f'LPT{i}' for i in range(1, 10)]}:
        raise ValueError('Reserved resource name')
    return name

def local_root(source):
    source = Path(source).resolve(strict=True)
    for candidate in (source, source / 'o', source / 'USRDIR/o'):
        if (candidate / 'stage').is_dir():
            return candidate
    if source.name == 'stage' and source.is_dir():
        return source.parent
    raise ValueError('「stage」フォルダーを含むo、USRDIR、ゲームフォルダーを選択してください。')

def discover(source):
    root = local_root(source)
    rows = []
    for stage in STAGES:
        layers = [p for p in (root / 'stage' / stage, root / 'dl/p/stage' / stage) if p.is_dir()]
        if layers:
            rows.append(dict(stage=stage, layers=[str(p) for p in layers]))
    return dict(source=str(root), stages=rows, missing=[s for s in STAGES if s not in {r['stage'] for r in rows}])

def validate_output(source, output):
    source = Path(source).resolve()
    output = Path(output).resolve()
    if output == source or source in output.parents or output in source.parents:
        raise ValueError('出力先は原データと別のフォルダーを指定してください。')
    for p in (output, *output.parents):
        if p.exists() and (p.is_symlink() or (hasattr(p, 'is_junction') and p.is_junction())):
            raise ValueError('出力先のリンク／ジャンクションは使用できません。')
    return output
_ELF_LAYOUTS = {'1a55a41ee5afdebd89075bd205e412c8311f569ba9dc022f629bb63fb4bfd13a': (16687888, 18949968)}
_archive_context = threading.local()

def configure_original_elf(path=None):
    _archive_context.tables = None
    if path is None:
        return None
    path = Path(path).resolve(strict=True)
    if not path.is_file() or path.stat().st_size > MAX_FILE:
        raise ValueError('原ELFのサイズが不正です。')
    with path.open('rb') as stream:
        if stream.read(6) != b'\x7fELF\x02\x02':
            raise ValueError('対応するPS3の原ELFを選択してください。')
        stream.seek(0)
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
        layout = _ELF_LAYOUTS.get(digest)
        if layout is None:
            raise ValueError('この原ELFの版は未対応です。認識済みMGO2の原ELFが必要です。')
        tables = []
        for offset in layout:
            stream.seek(offset)
            raw = stream.read(64)
            if len(raw) != 64:
                raise ValueError('原ELFのテーブル領域が不正です。')
            tables.append(struct.unpack('<8Q', raw))
        stream.seek(0)
        if hashlib.file_digest(stream, 'sha256').hexdigest() != digest:
            raise ValueError('読込中に原ELFが変更されました。')
    _archive_context.tables = tuple(tables)
    return dict(verified=True, sha256=digest)

def derive_key(stage):
    from cryptography.hazmat.decrepit.ciphers.algorithms import Blowfish
    from cryptography.hazmat.primitives.ciphers import Cipher, modes
    tables = getattr(_archive_context, 'tables', None)
    if tables is None:
        raise ValueError('暗号化された原データには、原ELFの明示選択が必要です（GUIのELF欄／--elf）。既に復号された原データはELFなしで読めます。')
    base, keys = tables
    decrypt = Cipher(Blowfish(struct.pack('<7Q', *base[1:])), modes.ECB()).decryptor()
    values = struct.unpack('<8Q', decrypt.update(struct.pack('<8Q', *keys)) + decrypt.finalize())
    block = struct.pack('<8Q', *(v ^ p for v, p in zip(values, [base[0], *keys[:-1]])))
    digest = hashlib.md5(('stage/' + stage).encode('ascii')).digest()
    return struct.unpack('<8Q', bytes((v ^ digest[i % 16] for i, v in enumerate(block))))

def decode(raw, stage):
    if len(raw) < 24:
        raise ValueError('Invalid encoded resource extent')
    keys = derive_key(stage)
    prev = keys[0]
    result = bytearray(raw)
    for i in range((len(raw) - 16) // 8):
        value = struct.unpack_from('<Q', raw, i * 8)[0]
        struct.pack_into('<Q', result, i * 8, value ^ prev ^ keys[i % 7 + 1])
        prev = value
    return bytes(result[:-24])

def validate_data(data, suffix):
    if suffix == '.dci':
        from excv_ui_icon import priority_remaps
        return priority_remaps(data)
    if suffix == '.dar':
        return dar(data)[0]
    if suffix == '.qar':
        return qar(data)
    if suffix == '.dlz':
        if data[:4] != b'segs':
            raise ValueError('DLZ magic')
        return None
    if suffix == '.geom':
        if data[:4] != b'\x0b\xf6\x8b\xfe':
            raise ValueError('GEOM magic')
        geometry(data, allow_stale_size=True)
        return None
    if suffix == '.gcx':
        at = 4
        offsets = []
        while struct.unpack('<I', take(data, at, 4))[0] != 4294967295:
            offsets.append(struct.unpack('<I', take(data, at, 4))[0] & 16777215)
            at += 4
            if len(offsets) > 4096:
                raise ValueError('GCX procedure count')
        base = at + 4
        body = base + struct.unpack('<I', take(data, base, 4))[0] + 4
        size = struct.unpack('<I', take(data, body - 4, 4))[0]
        if not offsets or not size or any((x >= size for x in offsets)):
            raise ValueError('GCX layout')
        take(data, body, size)
        return None
    raise ValueError('Unsupported package: ' + suffix)

def read_package(path, stage):
    if path.is_symlink() or path.stat().st_size > MAX_FILE:
        raise ValueError('Resource file exceeds supported bounds')
    raw = path.read_bytes()
    suffix = path.suffix.lower()
    try:
        info = validate_data(raw, suffix)
        return (raw, 'plain', info)
    except (ValueError, IndexError, KeyError, UnicodeError, struct.error):
        pass
    decoded = decode(raw, stage)
    info = validate_data(decoded, suffix)
    return (decoded, 'stage-key', info)

def extract_stage(source, stage, destination, progress=lambda _: None, cancel=lambda: False, resume=False):
    if stage not in STAGES:
        raise ValueError('Unknown MGO2 stage')
    inventory = discover(source)
    row = next((r for r in inventory['stages'] if r['stage'] == stage), None)
    if not row:
        raise ValueError('Stage source is missing: ' + stage)
    destination = validate_output(inventory['source'], destination)
    inputs = [(i, p) for i, layer in enumerate(row['layers']) for p in sorted(Path(layer).iterdir()) if p.is_file() and p.suffix.lower() in ('.dar', '.qar', '.dlz', '.gcx', '.geom', '.dci')]
    for _, path in inputs:
        safe_name(path.name)
    marker = dict(format='MGO2MTEXCV.WORK.2', stage=stage, source=inventory['source'], inputs=[dict(layer=i, source=file_record(p)) for i, p in inputs])
    if destination.exists():
        owner = destination / 'excv-work.json'
        if not resume or not owner.is_file() or json.loads(owner.read_text(encoding='utf-8')) != marker:
            raise ValueError('EXCVが作成した同じ原データの作業先のみ再開できます: ' + str(destination))
    destination.mkdir(parents=True, exist_ok=True)
    save_json(destination / 'excv-work.json', marker)
    records = []
    merged = {}
    for layer_index, layer in enumerate(row['layers']):
        for path in sorted(Path(layer).iterdir()):
            if cancel():
                raise InterruptedError('中止しました。')
            if not path.is_file() or path.suffix.lower() not in ('.dar', '.qar', '.dlz', '.gcx', '.geom', '.dci'):
                continue
            safe_name(path.name)
            progress(stage + ' / ' + path.name)
            before = file_record(path)
            try:
                data, mode, info = read_package(path, stage)
            except Exception as e:
                raise ValueError(path.name + ': ' + str(e)) from e
            local = destination / f'layer{layer_index}' / path.name
            atomic(local, data)
            if path.suffix == '.dlz':
                plain, _ = dlz(data, allow_cd_padding=stage in ('sm_dd', 'sm_ll'), allow_legacy_remaining_size=stage in ('sm_dd', 'sm_ll'))
                atomic(local.with_name(path.name + '.dld'), plain)
            if path.suffix in ('.dar', '.qar'):
                for entry in info:
                    name = safe_name(entry['name'])
                    offset = entry['offset']
                    if isinstance(offset, str):
                        offset = int(offset, 0)
                    atomic(local.parent / (path.stem + '_files') / name, take(data, offset, entry['size']))
            after = file_record(path)
            if before != after:
                raise ValueError('変換中に原データが変更されました: ' + str(path))
            records.append(dict(source=before, layer=layer_index, mode=mode, decoded_sha256=sha(data), decoded_size=len(data)))
            merged[path.name] = local
    work = destination / 'merged'
    work.mkdir(exist_ok=True)
    for name, path in merged.items():
        atomic(work / name, path.read_bytes())
        if path.suffix == '.dlz':
            atomic(work / (name + '.dld'), path.with_name(name + '.dld').read_bytes())
    for path in sorted(work.glob('*.dar')):
        data = path.read_bytes()
        for entry in dar(data)[0]:
            if entry['name'].endswith('.lt3'):
                atomic(work / safe_name(entry['name']), take(data, int(entry['offset'], 0), entry['size']))
    report = dict(stage=stage, source=inventory['source'], layers=row['layers'], records=records, source_unchanged=True)
    save_json(destination / 'extraction.json', report)
    return (work, report)
