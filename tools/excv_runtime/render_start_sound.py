# Generated converter-only source; historical analysis entry points omitted.
"""Render reviewed START sources to GWA; native stereo mixing is not PS3 DSP parity.

PPU 292808/292918, 29D0A8, 29D670 and SPU module at ELF VA 1086180.
SPU decoder offsets 774..BD8: four interleaved predictors; zero header = 16-byte silence.
No external executable or copied third-party codec implementation is used.
"""
import array, json, math, struct
from pathlib import Path
from elf_image import ElfImage
from native_assets import sha, atomic, HEADER, decode_gwa
ROOT = Path(__file__).resolve().parents[1]
ELF_SHA = '1a55a41ee5afdebd89075bd205e412c8311f569ba9dc022f629bb63fb4bfd13a'
f32 = lambda v: struct.unpack('<f', struct.pack('<f', v))[0]

def decode_ssw2(raw, coefficients, allow_zero_loop=False):
    if len(raw) < 32 or raw[:4] != b'SSW2' or int.from_bytes(raw[4:8], 'big') + 8 != len(raw):
        raise ValueError('SSW2 extent')
    if raw[20:22] != bytes((8, 1)) or (raw[28:32] != b'\xff' * 4 and (not (allow_zero_loop and raw[24:32] == bytes(8)))):
        raise ValueError('Only reviewed mono, non-looping 256-sample blocks supported')
    c1, c2, scale = coefficients
    out = []
    p = 32
    silent = 0
    while p < len(raw):
        if p + 16 > len(raw):
            raise ValueError('Truncated predictor header')
        headers = struct.unpack_from('>4I', raw, p)
        p += 16
        if not any(headers):
            out.extend([0.0] * 256)
            silent += 1
            continue
        if p + 128 > len(raw):
            raise ValueError('Truncated ADPCM block')
        for group, h in enumerate(headers):
            older = struct.unpack('>h', (h >> 16 & 65520).to_bytes(2, 'big'))[0]
            newer = struct.unpack('>h', (h >> 4 & 65520).to_bytes(2, 'big'))[0]
            predictor = h >> 5 & 7
            step = scale[h & 31]
            for row in range(8):
                for col in range(4):
                    byte = raw[p + row * 16 + group * 4 + col]
                    for nibble in (byte >> 4, byte & 15):
                        n = nibble if nibble < 8 else nibble - 16
                        residual = f32(n * step)
                        value = f32(older * c2[predictor] + f32(newer * c1[predictor] + residual))
                        out.append(value / 32768)
                        older, newer = (newer, value)
        p += 128
    return (out, silent)

def decode_wave(w, coefficients):
    raw = bytes.fromhex(w['source_waveform_hex'])
    h = w['header']
    if sha(raw) != w['source_sha256']:
        raise ValueError('Source waveform hash')
    if raw[:4] == b'SSWF':
        if h['lower_word5_signed'] != -1 or len(raw) != 16 + 2 * h['source_count']:
            raise ValueError('SSWF bounds/loop')
        samples = [v[0] / 32768 for v in struct.iter_unpack('>h', raw[16:])]
        return (samples, h['lower_rate_field'], 0)
    if raw[:4] != b'SSW2':
        raise ValueError('Wave format')
    samples, silent = decode_ssw2(raw, coefficients)
    if h['rate_encoding'] == 32:
        rate = h['rate_code']
    elif h['rate_encoding'] == 33:
        rate = int(f32(f32(2.0 ** f32(h['rate_code'] * f32(1 / 4608))) * 93.75))
    else:
        raise ValueError('Rate encoding')
    return (samples, rate, silent)

def render(document, waves, envelope):
    seq = document['sequences'][0]
    events = seq['events']
    header = bytes.fromhex(seq['header_hex'])
    division = int.from_bytes(header[44:46], 'big')
    if len(document['sequences']) != 1 or division != 48:
        raise ValueError('Unreviewed sequence timebase')
    tempo = [e for e in events if e.get('meta') == 81]
    if len(tempo) != 1 or tempo[0]['tick'] != 0:
        raise ValueError('Unreviewed tempo changes')
    tick_seconds = f32(int(tempo[0]['payload_hex'], 16) / division) / 1000000.0
    refs = {r['event_index']: r for r in seq['note_references']}
    rate = 48000
    frames = math.ceil((seq['duration_ticks'] * tick_seconds + 1.0) * rate)
    left = array.array('f', [0]) * frames
    right = array.array('f', [0]) * frames
    channels = [{} for _ in range(16)]
    active = {}
    rendered = []

    def emit(voice, end):
        c = voice['controls']
        samples, sr, _ = waves[voice['wave']]
        start = voice['start']
        stop = round(end * tick_seconds * rate)
        release = round(envelope[c.get(75, 0)])
        attack = round(envelope[c.get(71, 0)])
        step = sr / rate * 2 ** ((voice['note'] - 60) / 12)
        if all((k in c for k in range(24, 30))):
            a, b, cc, d, e, f = [c[k] / 127 for k in range(24, 30)]
            gl = a + 0.70710678 * b + 0.70710678 * d + 0.5 * f
            gr = cc + 0.70710678 * b + 0.70710678 * e + 0.5 * f
        else:
            pan = max(0, min(1, c.get(9, 64) / 127))
            gl = math.sqrt(1 - pan)
            gr = math.sqrt(pan)
        gain = voice['velocity'] / 127 * c.get(7, 127) / 127
        endframe = min(frames, stop + release, start + int((len(samples) - 1) / step))
        for at in range(start, endframe):
            pos = (at - start) * step
            idx = int(pos)
            frac = pos - idx
            v = (samples[idx] * (1 - frac) + samples[idx + 1] * frac) * gain
            if attack:
                v *= min(1, (at - start + 1) / attack)
            if at >= stop:
                v *= max(0, 1 - (at - stop + 1) / max(1, release))
            left[at] += v * gl
            right[at] += v * gr
        rendered.append({'channel': voice['channel'], 'wave_id': voice['wave'], 'note': voice['note'], 'start_frame': start, 'note_off_frame': stop, 'end_frame': endframe, 'attack_frames': attack, 'release_frames': release})
    for i, e in enumerate(events):
        if 'channel' not in e:
            continue
        ch = e['channel']
        data = e['data']
        op = e['status'] >> 4
        if op == 11:
            channels[ch][data[0]] = data[1]
        elif op in (8, 9):
            if ch in active:
                emit(active.pop(ch), e['tick'])
            if data[1]:
                ref = refs[i]
                active[ch] = {'channel': ch, 'wave': ref['wave_id'], 'note': data[0], 'velocity': data[1], 'controls': dict(channels[ch]), 'start': round(e['tick'] * tick_seconds * rate)}
        elif op != 12:
            raise ValueError('Unsupported sequence event')
    if active or len(rendered) != 23:
        raise ValueError('Unclosed notes / unexpected count')
    peak = max(max((abs(v) for v in left)), max((abs(v) for v in right)))
    if not math.isfinite(peak) or peak <= 0:
        raise ValueError('Invalid/silent synthesis')
    headroom = min(1, 0.85 / peak)
    pcm = array.array('h')
    for a, b in zip(left, right):
        pcm.extend((round(a * headroom * 32767), round(b * headroom * 32767)))
    import sys
    if sys.byteorder != 'little':
        pcm.byteswap()
    raw = pcm.tobytes()
    gwa = HEADER.pack(b'GWA1', 1, 64, rate, 2, 16, frames, 0, 0, 0, 0, len(raw)) + raw
    return (gwa, {'notes': rendered, 'tempo_us': int(tempo[0]['payload_hex'], 16), 'division': division, 'tick_seconds': tick_seconds, 'frames': frames, 'headroom_gain': headroom, 'mix_peak_before_headroom': peak})
