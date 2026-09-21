# Generated converter-only source; historical analysis entry points omitted.
"""Original normal-near firearm and MTSQ motion cues, bounded native PCM mixer.

Original note pitch, timing, CC7 and pan retained. Native linear resampling,
stereo surround/LFE fold and headroom; PS3 envelopes/reverb not emulated.
"""
from pathlib import Path
import array, io, json, math, struct, sys, wave
from elf_image import ElfImage
from extract_start_sound import extract, u32
from native_assets import sha
import resolve_start_sound as resolver
from render_start_sound import decode_wave, decode_ssw2, f32, ELF_SHA
from prepare_normal_ak102_audio import SOURCE_SHA
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'outputs/weapon_expansion_20260921/effects'
SOCOM_SHA = 'dc669e6c0dedb2656b2065e0bc2e0a08b49e4fd9cc578f383baeea9da5ccbc02'
FIRE = {2: 10087, 3: 10092, 4: 10197, 7: 10082, 8: 10102, 15: 10117, 18: 10127, 20: 10072, 23: 10122, 24: 10007, 25: 10002, 26: 10027, 30: 10022, 31: 10017, 35: 10037, 37: 10042, 38: 10047, 39: 10142, 41: 10062, 42: 10052, 43: 10067, 44: 10187, 50: 10167}

def decode_effect_wave(w, coeff, seconds):
    raw = bytes.fromhex(w['source_waveform_hex'])
    if raw[:4] != b'SSW2' or raw[24:32] != bytes(8):
        return decode_wave(w, coeff)
    if sha(raw) != w['source_sha256']:
        raise ValueError('source wave identity')
    samples, silent = decode_ssw2(raw, coeff, allow_zero_loop=True)
    h = w['header']
    rate = h['rate_code'] if h['rate_encoding'] == 32 else int(f32(f32(2 ** f32(h['rate_code'] * f32(1 / 4608))) * 93.75))
    need = min(30 * rate, math.ceil(seconds * rate * 4))
    if not samples:
        raise ValueError('empty loop')
    return ((samples * math.ceil(need / len(samples)))[:need], rate, silent)

def render(seq, waves, envelope=None):
    division = int.from_bytes(bytes.fromhex(seq['header_hex'])[44:46], 'big')
    tempo = [e for e in seq['events'] if e.get('meta') == 81]
    if not division or len(tempo) != 1 or tempo[0]['tick']:
        raise ValueError('clock automation')
    tick = int(tempo[0]['payload_hex'], 16) / division / 1000000.0
    rate = 48000
    tail = max((round(envelope[e['data'][1]]) for e in seq['events'] if e.get('status', 0) >> 4 == 11 and e['data'][0] == 75), default=0) if envelope else 0
    frames = math.ceil(seq['duration_ticks'] * tick * rate) + tail
    expression = {}
    pitch = {}
    for e in seq['events']:
        if e.get('status', 0) >> 4 == 11 and e['data'][0] == 11:
            expression.setdefault(e['channel'], []).append((round(e['tick'] * tick * rate), e['data'][1] / 127))
        if e.get('status', 0) >> 4 == 14:
            pitch.setdefault(e['channel'], []).append((round(e['tick'] * tick * rate), 1.1224620342254639 ** ((e['data'][0] + (e['data'][1] << 7) - 8192) / 8192)))
    for target, ch, events in [(target, ch, events) for target in (expression, pitch) for ch, events in target.items()]:
        curve = array.array('f', [1]) * frames
        for i, (begin, value) in enumerate(events):
            end = events[i + 1][0] if i + 1 < len(events) else frames
            curve[begin:end] = array.array('f', [value]) * (end - begin)
        target[ch] = curve
    if not 0 < frames <= 30 * rate:
        raise ValueError('duration')
    left = array.array('f', [0]) * frames
    right = array.array('f', [0]) * frames
    active = {}
    controls = {}
    refs = {r['event_index']: r for r in seq['note_references']}
    notes = []
    for index, e in enumerate(seq['events']):
        if 'channel' not in e:
            if e.get('meta') not in (47, 81):
                raise ValueError('metadata')
            continue
        ch = e['channel']
        op = e['status'] >> 4
        d = e['data']
        cc = controls.setdefault(ch, {})
        if op == 11:
            if d[0] not in (0, 7, 10, 11, 24, 25, 26, 27, 28, 29, 32, 75, 121) or (d[0] == 121 and d[1] != 0) or (e['tick'] and d[0] != 11 and any((k[0] == ch for k in active))):
                raise ValueError('controller/automation')
            cc[d[0]] = d[1]
        elif op == 12:
            if any((k[0] == ch for k in active)):
                raise ValueError('program changes during note')
        elif op == 9 and d[1]:
            k = (ch, d[0])
            if k in active:
                raise ValueError('overlapping same note')
            active[k] = (e, refs[index], dict(cc))
        elif op == 9 and (not d[1]) or op == 8:
            k = (ch, d[0])
            if k not in active:
                raise ValueError('unpaired note')
            start, ref, cc = active.pop(k)
            samples, sr, silent = waves[ref['wave_id']]
            step = sr / rate * 2 ** ((start['data'][0] - 60) / 12)
            gain = cc.get(7, 127) / 127 * start['data'][1] / 127
            pan = cc.get(10, 64) / 127
            gl = math.cos(pan * math.pi / 2)
            gr = math.sin(pan * math.pi / 2)
            direct = [cc.get(k, 0) for k in range(24, 30)]
            if any(direct):
                gl = (direct[0] + direct[1] * 0.7071 + direct[3] * 0.7071 + direct[5] * 0.5) / 127
                gr = (direct[2] + direct[1] * 0.7071 + direct[4] * 0.7071 + direct[5] * 0.5) / 127
            begin = round(start['tick'] * tick * rate)
            stop = round(e['tick'] * tick * rate)
            release = round(envelope[cc[75]]) if envelope and 75 in cc else 0
            end = min(frames, stop + release, begin + int((len(samples) - 1) / step))
            curve = expression.get(ch)
            bend = pitch.get(ch)
            position = 0.0
            for at in range(begin, end):
                pos = position if bend is not None else (at - begin) * step
                i = int(pos)
                if i + 1 >= len(samples):
                    break
                if bend is not None:
                    position += step * bend[at]
                v = (samples[i] * (1 - (pos - i)) + samples[i + 1] * (pos - i)) * gain
                v *= curve[at] if curve is not None else 1
                v *= max(0, 1 - (at - stop + 1) / max(1, release)) if at >= stop else 1
                left[at] += v * gl
                right[at] += v * gr
            notes.append(dict(wave=ref['wave_id'], channel=ch, note=start['data'][0], begin=begin, end=end, source_rate=sr, silent_blocks=silent))
        elif op == 14:
            pass
        else:
            raise ValueError('unsupported event')
    if active or not notes:
        raise ValueError('unclosed/no notes')
    peak = max(max(map(abs, left)), max(map(abs, right)))
    if not math.isfinite(peak) or peak < 1e-05:
        raise ValueError('silent mix')
    gain = min(1.0, 0.85 / peak)
    pcm = array.array('h')
    for l, r in zip(left, right):
        pcm.extend((round(l * gain * 32767), round(r * gain * 32767)))
    if sys.byteorder != 'little':
        pcm.byteswap()
    stream = io.BytesIO()
    with wave.open(stream, 'wb') as w:
        w.setparams((2, 2, rate, frames, 'NONE', 'not compressed'))
        w.writeframes(pcm.tobytes())
    return (stream.getvalue(), dict(frames=frames, rate=rate, notes=notes, headroom=gain))

def zero_msb_references(parsed):
    prefix = [dict(status=176 | ch, channel=ch, data=[0, 0], tick=0, offset=-1) for ch in range(16)]
    result = STRICT_REFERENCES(prefix + parsed)
    for row in result:
        row['event_index'] -= len(prefix)
    return result
STRICT_REFERENCES = resolver.note_references
