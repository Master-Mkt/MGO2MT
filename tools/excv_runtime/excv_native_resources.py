# Generated converter-only source; historical analysis entry points omitted.
"""Allowlisted native configuration and deterministic native notification audio.

No original asset payload, account data, arbitrary path, network call or external
program execution is accepted. Public server defaults are configuration only.
"""
from pathlib import Path
import argparse, io, json, math, struct, wave
from excv_core import atomic, save_json, file_record, sha
TEXT_NAMES = frozenset(('weapon_catalog.tsv', 'skill_catalog.tsv', 'combat_health.cfg', 'round_items.cfg', 'breakable_lights.cfg', 'bullet_penetration_profile.json', 'item_drop_policy.json', 'launch.cfg', 'lobbies.cfg', 'title.gwp'))
AUDIO_NAMES = frozenset(('audio/sop_native.wav', 'audio/notification_native.wav', 'sfx/native_water_step.wav'))
MISSING = [{'path': 'movie_01.mp4', 'kind': 'user_supplied_input', 'reason': 'User selected video must be supplied separately; not in original game resources'}, {'path': 'network.gnk', 'kind': 'separate_network_input', 'reason': 'Reviewed GNK originates in separate local network implementation; not generated from game originals'}]

def native_water_step():
    """Exact native water_audio.cpp recipe; no original sound payload."""
    rate, count = (22050, 5292)
    pcm = bytearray()
    random = 1463899218
    low = previous = 0.0
    for i in range(count):
        t = i / rate
        random ^= random << 13 & 4294967295
        random ^= random >> 17
        random ^= random << 5 & 4294967295
        noise = random / 4294967295.0 * 2 - 1
        low += 0.18 * (noise - low)
        texture = (noise - previous) * 0.18 + low * 0.9
        previous = noise
        attack = min(t / 0.008, 1.0)
        tail = max(0.0, 1 - i / count)
        bubbles = math.sin(6.283185307 * (380 * t - 430 * t * t)) * math.exp(-t * 24) + 0.45 * math.sin(6.283185307 * (610 * t - 740 * t * t)) * math.exp(-t * 32)
        sample = attack * tail * tail * (texture * 0.8 + bubbles * 0.2) * 8500
        rounded = math.floor(sample + 0.5) if sample >= 0 else math.ceil(sample - 0.5)
        pcm += struct.pack('<h', max(-10000, min(10000, rounded)))
    out = io.BytesIO()
    with wave.open(out, 'wb') as sound:
        sound.setparams((1, 2, rate, count, 'NONE', 'not compressed'))
        sound.writeframes(pcm)
    return out.getvalue()

def native_audio(name):
    if name not in AUDIO_NAMES:
        raise ValueError('Native audio name is not allowlisted')
    if name == 'sfx/native_water_step.wav':
        return native_water_step()
    rate = 48000
    pcm = bytearray()
    channels = 1 if name.endswith('sop_native.wav') else 2
    frames = 17280 if channels == 1 else 20160
    for i in range(frames):
        value = 0.0
        if channels == 1:
            t = i / rate
            for start, frequency in ((0.0, 660.0), (0.1, 880.0), (0.2, 1320.0)):
                u = t - start
                if 0 <= u < 0.16:
                    envelope = min(1.0, u / 0.006) * math.exp(-u * 24) * min(1.0, (0.16 - u) / 0.016)
                    value += 0.12 * envelope * math.sin(2 * math.pi * frequency * u)
            integer = round(max(-1.0, min(1.0, value)) * 32767)
        else:
            for start, duration, frequency in ((0.02, 0.13, 660.0), (0.19, 0.17, 880.0)):
                count = round(duration * rate)
                local = i - round(start * rate)
                if 0 <= local < count:
                    envelope = math.sin(math.pi * local / (count - 1)) ** 2
                    phase = 2.0 * math.pi * frequency * local / rate
                    value += 2200.0 * envelope * (math.sin(phase) + 0.2 * math.sin(phase * 2.0))
            integer = max(-32768, min(32767, round(value)))
        pcm += struct.pack('<' + 'h' * channels, *[integer] * channels)
    out = io.BytesIO()
    with wave.open(out, 'wb') as sound:
        sound.setparams((channels, 2, rate, frames, 'NONE', 'not compressed'))
        sound.writeframes(pcm)
    return out.getvalue()

def recipe(profile_path=None):
    path = Path(profile_path) if profile_path else Path(__file__).with_name('excv_native_profile.json')
    profile = json.loads(path.read_text(encoding='utf-8'))
    if profile.get('schema') != 'MGO2MTEXCV.NATIVE_RECIPE.1':
        raise ValueError('Native recipe version')
    if {r['path'] for r in profile['text_assets']} != TEXT_NAMES or len(profile['text_assets']) != len(TEXT_NAMES):
        raise ValueError('Native text allowlist differs')
    if {r['path'] for r in profile['audio_assets']} != AUDIO_NAMES or len(profile['audio_assets']) != len(AUDIO_NAMES):
        raise ValueError('Native audio allowlist differs')
    for r in profile['text_assets']:
        raw = r['text'].encode('utf-8')
        if (len(raw), sha(raw)) != (r['size'], r['sha256']):
            raise ValueError('Native text recipe digest differs')
    return (path, profile)

def verify_native_resources(output, profile_path=None):
    output = Path(output).resolve()
    errors = []
    assets_checked = 0
    try:
        path, profile = recipe(profile_path)
        r = json.loads((output / 'native-result.json').read_text(encoding='utf-8'))
        if r.get('format') != 'MGO2MTEXCV.NATIVE_RESULT.1' or not r.get('native_complete'):
            raise ValueError('Only completed native resources can resume')
        if r['profile']['sha256'] != file_record(path)['sha256']:
            raise ValueError('Native recipe changed')
        expected = {a['path']: a for a in profile['text_assets'] + profile['audio_assets']}
        if {a['path'] for a in r['assets']} != set(expected) or len(r['assets']) != len(expected):
            raise ValueError('Native manifest inventory changed')
        if {p.relative_to(output / 'data').as_posix() for p in (output / 'data').rglob('*') if p.is_file()} != set(expected):
            raise ValueError('Native runtime inventory changed')
        for name, a in expected.items():
            actual = file_record(output / 'data' / name)
            if (actual['size'], actual['sha256']) != (a['size'], a['sha256']):
                raise ValueError('Native resource changed: ' + name)
            assets_checked += 1
    except (OSError, ValueError, TypeError, KeyError) as exc:
        errors.append(str(exc))
    return {'passed': not errors, 'assets_checked': assets_checked, 'errors': errors}

def build_native_resources(output, progress=lambda _: None, cancel=lambda: False, resume=False, profile_path=None):
    output = Path(output).resolve()
    for p in (output, *output.parents):
        if p.exists() and (p.is_symlink() or (hasattr(p, 'is_junction') and p.is_junction())):
            raise ValueError('Native output cannot use a link/junction')
    if output.exists() and any(output.iterdir()):
        if not resume:
            raise ValueError('Native output must be a new empty folder')
        checked = verify_native_resources(output, profile_path)
        if not checked['passed']:
            raise ValueError('Cannot resume native output: ' + '; '.join(checked['errors']))
        r = json.loads((output / 'native-result.json').read_text(encoding='utf-8'))
        r['resumed'] = True
        r['verification'] = checked
        return r
    path, profile = recipe(profile_path)
    output.mkdir(parents=True, exist_ok=True)
    assets = []
    for row in profile['text_assets'] + profile['audio_assets']:
        if cancel():
            raise InterruptedError('Native resource generation cancelled')
        progress('native / ' + row['path'])
        raw = row['text'].encode('utf-8') if 'text' in row else native_audio(row['path'])
        if (len(raw), sha(raw)) != (row['size'], row['sha256']):
            raise ValueError('Native generation SHA differs: ' + row['path'])
        atomic(output / 'data' / row['path'], raw)
        assets.append({k: v for k, v in row.items() if k != 'text'})
    report = {'format': 'MGO2MTEXCV.NATIVE_RESULT.1', 'native_complete': True, 'complete': True, 'complete_runtime_set': False, 'profile': file_record(path), 'assets': assets, 'missing': MISSING, 'native_boundaries': ['Configuration is the reviewed public native default, not a live service discovery or connectivity check', 'The title control routes are numeric native adapter metadata; no original GCX payload is bundled', 'Three sounds are deterministic native synthesis, not original MGO2 cues', 'No account/authentication data, network GNK, user movie or final integrity index is fabricated']}
    save_json(output / 'native-result.json', report)
    return report
