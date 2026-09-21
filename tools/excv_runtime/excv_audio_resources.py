# Generated converter-only source; historical analysis entry points omitted.
"""Local original audio -> reviewed native PCM/GWA recipes, without asset embedding.

ELF and optional vgmstream executable are explicit user-selected dependencies.
Historical fixed-path entry points are never executed. Unsupported/missing inputs
are recorded per asset, never replaced with another sound.
"""
from pathlib import Path
import argparse, array, io, json, math, struct, subprocess, sys, tempfile, time, types, wave
from excv_core import atomic, save_json, sha, file_record, local_root, validate_output, decode
from excv_ui_resources import checked_path
from excv_audio_profile import SOURCES, RECIPES
from elf_image import ElfImage
from extract_start_sound import chunks, events, u32
import resolve_start_sound as resolver
from render_start_sound import decode_wave, ELF_SHA, render as render_start
from prepare_all_weapon_audio import render as render_general, decode_effect_wave, zero_msb_references
from prepare_normal_ak102_audio import render_nearest
from prepare_weapon_extension_audio import render as render_extension
from prepare_combat_impact_audio import render_variant
from native_assets import HEADER, decode_gwa
GROUPS = ('startup', 'weapons', 'voice', 'radio', 'bgm')
CONTROLLERS = set(resolver.START_CONTROLLERS) | {11, 75, 121}
DECODER_SHA = '29df08c557ada8269c92a6abfe3886ad2f65849b0b26ba78a0819b363bbc5b85'
LIMITATIONS = ['Original waveforms, notes and registered cue recipes are retained; synthesis is the existing native stereo/resampling/headroom adapter, not complete PS3 DSP parity.', 'Original reverb, spatial bus, envelopes, implicit controller state, voice allocation and nonzero waveform loop behavior are not fully reproduced.', 'Weapon normal-near and first-variant choices follow the current native runtime; original dynamic listener/variant selection is not newly recovered.', 'Voice bank letters are native audition labels; exact original creation-screen phrase and wire selection mapping are not claimed.', 'Lobby BGM selects the first four decoded channels; original dynamic layers and listener/ambient region transitions remain outside conversion.', 'Native water footsteps, SOP and notification sounds are software-authored runtime assets, not original audio; this converter does not label them as original.', 'The optional full round-BGM WAV catalog is outside these 540 GWA + 123 combat WAV recipes.']

def group_of(row):
    if row['recipe'] == 'bgm':
        return 'bgm'
    if row['path'].startswith('sfx/'):
        return 'weapons'
    return row['recipe'] if row['recipe'] in ('voice', 'radio') else 'startup'

def parse_cue(bank, cue):
    """Bounded SSPF/ICUE/TSND/ISND reader; offsets relative to this exact bank."""
    parts = chunks(bank)
    icue_at, icue = parts['ICUE']
    count = u32(icue, 8)
    if len(icue) != 16 + 32 * count:
        raise ValueError('ICUE extent')
    matches = [(16 + i * 32, icue[16 + i * 32:48 + i * 32]) for i in range(count) if int.from_bytes(icue[16 + i * 32:18 + i * 32], 'big') == cue]
    if len(matches) != 1:
        raise ValueError('Cue missing or ambiguous')
    at, record = matches[0]
    indices = [int.from_bytes(record[16:18], 'big')]
    for p in range(18, 32, 2):
        index = int.from_bytes(record[p:p + 2], 'big')
        if not index:
            break
        indices.append(index)
    _, tsnd = parts['TSND']
    isnd_at, isnd = parts['ISND']
    total = u32(tsnd, 8)
    used = 16 + 4 * total
    if not used <= len(tsnd) < used + 16 or any(tsnd[used:]) or u32(isnd, 8) != total:
        raise ValueError('Sequence table extent')
    sequences = []
    for index in indices:
        if index >= total:
            raise ValueError('Sequence index')
        begin = u32(tsnd, 16 + index * 4)
        end = u32(tsnd, 20 + index * 4) if index + 1 < total else len(isnd)
        if not 16 <= begin < end <= len(isnd):
            raise ValueError('Sequence extent')
        raw = isnd[begin:end]
        if raw[48:52] != b'MTrk':
            raise ValueError('Sequence format')
        size = u32(raw, 52)
        if size > len(raw) - 56 or any(raw[56 + size:]):
            raise ValueError('Sequence payload extent')
        parsed = events(raw[56:56 + size])
        sequences.append(dict(index=index, source_offset=isnd_at + begin, header_hex=raw[:48].hex(), raw_hex=raw.hex(), events=parsed, channels_used=sorted({e['channel'] for e in parsed if 'channel' in e}), duration_ticks=parsed[-1]['tick']))
    digest = sha(bank)
    return dict(format='MGO2MT.GWS', version=1, cue=cue, source='selected original SSPF bank', source_sha256=digest, bank_offset=0, bank_size=len(bank), bank_sha256=digest, cue_record_offset=icue_at + at, cue_record_hex=record.hex(), sequences=sequences)

def bank_index(data):
    result = {}
    offset = 0
    banks = 0
    while offset >= 0 and offset < len(data):
        size = u32(data, offset + 8)
        if size < 64 or offset + size > len(data):
            raise ValueError('Original SSPF bank extent')
        parts = chunks(data[offset:offset + size])
        icue = parts['ICUE'][1]
        count = u32(icue, 8)
        if len(icue) != 16 + 32 * count:
            raise ValueError('Original cue table extent')
        for i in range(count):
            result.setdefault(int.from_bytes(icue[16 + i * 32:18 + i * 32], 'big'), []).append((offset, size))
        banks += 1
        if banks > 8192:
            raise ValueError('Original bank count')
        offset = data.find(b'SSPF', offset + size)
    return result

def gwa(pcm, channels, rate, frames, begin=0, end=0):
    data = HEADER.pack(b'GWA1', 1, 64, rate, channels, 16, frames, begin, end, int(bool(end)), 0, len(pcm)) + pcm
    decode_gwa(data)
    return data

def wav_to_gwa(data):
    with wave.open(io.BytesIO(data), 'rb') as w:
        if w.getsampwidth() != 2 or w.getcomptype() != 'NONE':
            raise ValueError('PCM16 required')
        return gwa(w.readframes(w.getnframes()), w.getnchannels(), w.getframerate(), w.getnframes())

def pcm_signature(data):
    if data[:4] == b'GWA1':
        d = decode_gwa(data)
        pcm = d.pop('pcm')
        return dict(**d, pcm_sha256=sha(pcm))
    with wave.open(io.BytesIO(data), 'rb') as w:
        if w.getsampwidth() != 2 or w.getcomptype() != 'NONE':
            raise ValueError('PCM16 required')
        return dict(sample_rate=w.getframerate(), channels=w.getnchannels(), frames=w.getnframes(), loop_start=0, loop_end=0, pcm_sha256=sha(w.readframes(w.getnframes())))

def render_single_gwa(document, waves, recipe):
    if len(document['sequences']) != 1 or len(waves) != 1:
        raise ValueError('Single original voice required')
    seq = document['sequences'][0]
    notes = [e for e in seq['events'] if e['status'] >> 4 == 9]
    tempo = [e for e in seq['events'] if e.get('meta') == 81]
    if len(notes) != 2 or notes[0]['tick'] or notes[0]['data'][0] != 60 or (notes[1]['data'] != [60, 0]):
        raise ValueError('Original note profile')
    if recipe != 'menu' and notes[0]['data'][1] != 127:
        raise ValueError('Original speech velocity')
    if int.from_bytes(bytes.fromhex(seq['header_hex'])[44:46], 'big') != 48 or len(tempo) != 1 or tempo[0]['tick'] or (tempo[0]['payload_hex'] != '075300'):
        raise ValueError('Original single-note clock')
    controls = [e for e in seq['events'] if e['status'] >> 4 == 11]
    allowed = {0, 32, 121, 91} if recipe == 'menu' else {0, 32, 121} | (set(range(24, 30)) if recipe == 'radio' else set())
    if any((e['tick'] or e['data'][0] not in allowed for e in controls)):
        raise ValueError('Unreviewed single-note controllers')
    samples, rate, _ = next(iter(waves.values()))
    frames = min(round(seq['duration_ticks'] * 0.01 * 48000), int((len(samples) - 1) * 48000 / rate))
    if not 0 < frames <= 480000:
        raise ValueError('Original single-note duration')
    values = []
    gain = notes[0]['data'][1] / 127 / math.sqrt(2)
    for at in range(frames):
        pos = at * rate / 48000
        i = int(pos)
        f = pos - i
        v = samples[i] * (1 - f) + samples[i + 1] * f
        values.append(v * gain if recipe == 'menu' else v / math.sqrt(2))
    peak = max(map(abs, values))
    if not math.isfinite(peak) or peak < 0.0001:
        raise ValueError('Silent or invalid original voice')
    headroom = min(1, 0.85 / peak)
    pcm = array.array('h')
    for v in values:
        pcm.extend([round(v * headroom * 32767)] * 2)
    if sys.byteorder != 'little':
        pcm.byteswap()
    return gwa(pcm.tobytes(), 2, 48000, frames)

class AudioSource:

    def __init__(self, root, report):
        self.root = root
        self.report = report
        self.cache = {}
        self.bytes = 0

    def get(self, row):
        key = row['source']
        if key in self.cache:
            return self.cache[key]
        path = checked_path(self.root, self.root / key)
        before = file_record(path)
        if before['sha256'] != SOURCES[key]:
            raise ValueError('Reviewed original source SHA differs: ' + key)
        data = path.read_bytes()
        if file_record(path) != before:
            raise ValueError('Original source changed during conversion')
        if row.get('decode_stage'):
            data = decode(data, row['decode_stage'])
        if self.bytes + len(data) > 256 * 1024 * 1024:
            raise ValueError('Original sound bank memory budget')
        index = bank_index(data)
        proof = dict(relative=key, source=before, decoded_sha256=sha(data), decoded_size=len(data))
        self.report['sources'].append(proof)
        self.cache[key] = (data, index, proof)
        self.bytes += len(data)
        return self.cache[key]

    def bank(self, row):
        data, index, proof = self.get(row)
        matches = index.get(row['cue'], [])
        if row.get('bank_offset') is not None:
            matches = [r for r in matches if r[0] == row['bank_offset']]
        if len(matches) != 1:
            raise ValueError('Original cue bank missing or ambiguous')
        offset, size = matches[0]
        return (data[offset:offset + size], dict(source=proof, bank_offset=offset, bank_size=size))

def read_elf(path):
    path = Path(path).resolve(strict=True)
    before = file_record(path)
    if before['size'] > 256 * 1024 * 1024 or before['sha256'] != ELF_SHA:
        raise ValueError('Reviewed original ELF required')
    elf = ElfImage(path)
    if file_record(path) != before:
        raise ValueError('Original ELF changed')
    contract = types.FunctionType(resolver.contracts.__code__, dict(resolver.contracts.__globals__, START_CONTROLLERS=tuple(sorted(CONTROLLERS))))(elf)
    module = elf.read(17326464, 10440)
    coeff = [struct.unpack_from('>8f', module, 9280), struct.unpack_from('>8f', module, 9312), struct.unpack_from('>32f', module, 9344)]
    envelope = struct.unpack('>128f', elf.read(16526296, 512))
    return (before, contract, coeff, envelope)

def render_cue(row, bank, contract, coeff, envelope):
    parse = parse_cue(bank, row['cue'])
    resolve = resolver.resolve
    if row['cue'] == 12006:
        resolve = types.FunctionType(resolve.__code__, dict(resolve.__globals__, note_references=zero_msb_references), argdefs=resolve.__defaults__)
    document = resolve(parse, bank, contract, CONTROLLERS)
    recipe = row['recipe']
    decoder = decode_effect_wave if recipe == 'general' else decode_wave
    waves = {w['wave_id']: decoder(w, coeff, 30) if recipe == 'general' else decoder(w, coeff) for w in document['waveform_dependencies']}
    seq = document['sequences'][0]
    if recipe == 'start':
        data = render_start(document, waves, envelope)[0]
    elif recipe in ('voice', 'radio', 'menu'):
        data = render_single_gwa(document, waves, recipe)
    elif recipe in ('single', 'salute'):
        data = render_variant(seq, waves)[0]
        if recipe == 'salute':
            data = wav_to_gwa(data)
    elif recipe == 'ak102':
        data = render_nearest(document, waves)[0]
    elif recipe == 'extension':
        data = render_extension(document, waves)[0]
    elif recipe == 'general':
        data = render_general(seq, waves, envelope)[0]
    else:
        raise ValueError('Unknown reviewed audio recipe')
    return (data, dict(bank_sha256=sha(bank), sequences=[dict(index=s['index'], sha256=sha(bytes.fromhex(s['raw_hex']))) for s in document['sequences']], waves=[{k: w[k] for k in ('wave_id', 'source_offset', 'source_size', 'source_sha256')} for w in document['waveform_dependencies']]))

def render_bgm(root, row, decoder, destination, cancel):
    if decoder is None:
        raise ValueError('Select the local vgmstream executable for BGM')
    decoder = Path(decoder).resolve(strict=True)
    if sha(decoder.read_bytes()) != DECODER_SHA:
        raise ValueError('Reviewed vgmstream r2117 executable required')
    source = checked_path(root, root / row['source'])
    before = file_record(source)
    if before['sha256'] != SOURCES[row['source']]:
        raise ValueError('Reviewed original BGM differs')
    with tempfile.TemporaryDirectory(prefix='decode-', dir=destination) as temporary:
        wav = Path(temporary) / 'decoded.wav'
        flags = subprocess.CREATE_NO_WINDOW if sys.platform == 'win32' else 0
        process = subprocess.Popen([str(decoder), '-i', '-o', str(wav), str(source)], stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, creationflags=flags)
        started = time.monotonic()
        try:
            while True:
                try:
                    _, error = process.communicate(timeout=0.25)
                    break
                except subprocess.TimeoutExpired:
                    if cancel():
                        raise InterruptedError('Conversion cancelled')
                    if time.monotonic() - started > 300:
                        raise ValueError('Local audio decoder timed out')
            if process.returncode:
                raise ValueError('Local audio decoder failed: ' + error.decode('utf-8', errors='replace')[:300])
        except BaseException:
            process.kill()
            process.communicate()
            raise
        expected = row['audio']
        channels = expected['channels']
        parts = []
        with wave.open(str(wav), 'rb') as w:
            if (w.getsampwidth(), w.getframerate(), w.getnframes()) != (2, expected['sample_rate'], expected['frames']) or w.getnchannels() < channels:
                raise ValueError('Decoded BGM PCM contract')
            actual_channels = w.getnchannels()
            if actual_channels != channels and (not (row['path'] == 'audio/lobby.gwa' and actual_channels == 12 and (channels == 4))):
                raise ValueError('Unreviewed original BGM channel selection')
            while (block := w.readframes(65536)):
                if cancel():
                    raise InterruptedError('Conversion cancelled')
                if actual_channels == channels:
                    parts.append(block)
                else:
                    parts.append(b''.join((block[i:i + channels * 2] for i in range(0, len(block), actual_channels * 2))))
        data = gwa(b''.join(parts), channels, expected['sample_rate'], expected['frames'], expected['loop_start'], expected['loop_end'])
    if file_record(source) != before:
        raise ValueError('Original BGM changed during conversion')
    return (data, dict(source=dict(relative=row['source'], source=before), decoder=file_record(decoder), selected_channels=list(range(channels))))

def verify_audio_resources(destination, source_root=None, elf_path=None):
    destination = Path(destination).resolve(strict=True)
    report = json.loads((destination / 'manifest.json').read_text(encoding='utf-8'))
    if report.get('format') != 'MGO2MTEXCV.AUDIO_RESOURCES.1' or not report.get('complete'):
        raise ValueError('Incomplete audio conversion')
    for row in report['files']:
        path = destination / row['path']
        if destination not in path.resolve().parents or path.is_symlink():
            raise ValueError('Unsafe output path')
        actual = file_record(path)
        if (actual['size'], actual['sha256']) != (row['size'], row['sha256']):
            raise ValueError('Audio output changed')
    if source_root is not None:
        root = local_root(source_root)
        for row in report['sources']:
            path = root / row['relative']
            if root not in path.resolve().parents or file_record(path)['sha256'] != row['source']['sha256']:
                raise ValueError('Original audio changed')
    if elf_path is not None and report.get('elf') and (file_record(Path(elf_path))['sha256'] != report['elf']['sha256']):
        raise ValueError('Original ELF changed')
    return report

def extract_audio_resources(source_root, destination, *, elf_path=None, vgmstream=None, groups=GROUPS, compare_data=None, progress=lambda _: None, cancel=lambda: False, resume=False):
    selected = Path(source_root).resolve(strict=True)
    root = local_root(selected)
    destination = validate_output(selected, Path(destination))
    validate_output(root, destination)
    groups = tuple(groups)
    if not groups or len(set(groups)) != len(groups) or any((g not in GROUPS for g in groups)):
        raise ValueError('Unknown, empty or duplicate audio group')
    compare = Path(compare_data).resolve(strict=True) if compare_data else None
    if destination.exists():
        if not resume:
            raise ValueError('Select a new audio conversion destination')
        r = verify_audio_resources(destination, root, elf_path)
        if r['groups'] != list(groups):
            raise ValueError('Resume audio groups changed')
        return r
    destination.mkdir(parents=True)
    report = dict(format='MGO2MTEXCV.AUDIO_RESOURCES.1', complete=False, groups=list(groups), sources=[], assets=[], files=[], missing=[], limitations=LIMITATIONS)
    try:
        context = None
        elf_error = None
        if any((g != 'bgm' for g in groups)):
            try:
                if elf_path is None:
                    raise ValueError('Select the original ELF for cue synthesis')
                proof, *context = read_elf(elf_path)
                report['elf'] = proof
            except (ValueError, OSError) as error:
                elf_error = str(error)
        source = AudioSource(root, report)
        for row in RECIPES:
            if group_of(row) not in groups:
                continue
            if cancel():
                raise InterruptedError('Conversion cancelled')
            progress('Audio / ' + row['path'])
            try:
                if row['recipe'] == 'bgm':
                    data, proof = render_bgm(root, row, vgmstream, destination, cancel)
                    report['sources'].append(proof['source'])
                else:
                    if elf_error:
                        raise ValueError(elf_error)
                    bank, proof = source.bank(row)
                    data, detail = render_cue(row, bank, *context)
                    proof.update(detail)
                if (len(data), sha(data)) != (row['size'], row['sha256']):
                    raise ValueError('Reviewed native output SHA mismatch')
                signature = pcm_signature(data)
                atomic(destination / 'data' / row['path'], data)
                report['assets'].append(dict(path=row['path'], size=len(data), sha256=sha(data), recipe=row['recipe'], cue=row.get('cue'), pcm=signature, provenance=proof))
            except (ValueError, KeyError, OSError, struct.error, wave.Error) as error:
                report['missing'].append(dict(path=row['path'], source=row['source'], reason=str(error)))
        if 'weapons' in groups:
            mapping = {a['cue']: Path(a['path']).name for a in report['assets'] if a['path'].startswith('sfx/')}
            atomic(destination / 'data/sfx/combat.txt', (f'MGO2MT.COMBAT_AUDIO 1 {len(mapping)}\r\n' + ''.join((f'{k} {v}\r\n' for k, v in sorted(mapping.items())))).encode())
        report['complete'] = True
        report['status'] = 'complete' if not report['missing'] else 'completed_with_missing'
    finally:
        for path in sorted((destination / 'data').rglob('*')) if (destination / 'data').exists() else ():
            if not path.is_file():
                continue
            row = file_record(path)
            row['path'] = path.relative_to(destination).as_posix()
            if compare is not None:
                other = compare / row['path'][5:]
                row['comparison'] = dict(path=str(other), exists=other.is_file())
                if other.is_file():
                    row['comparison'].update(file_record(other), equal=sha(other.read_bytes()) == row['sha256'])
            report['files'].append(row)
        save_json(destination / 'manifest.json', report)
    return report
