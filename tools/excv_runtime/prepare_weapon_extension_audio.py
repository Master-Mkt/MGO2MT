# Generated converter-only source; historical analysis entry points omitted.
"""Two source-verified original firing cues; native centered/LFE stereo fold.

Only saved normal sdpack_n.dat is read. No patch, playback, manifest or package
mutation. Original listener tier, envelope, filters and reverb are not emulated.
"""
from pathlib import Path
import array, io, json, math, struct, sys, wave
from elf_image import ElfImage
from extract_start_sound import extract
import resolve_start_sound as resolver
from render_start_sound import decode_wave, f32, ELF_SHA
from native_assets import sha
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'outputs/weapon_extensions_20260914'
SOURCE_SHA = '0d38ee1e5389ad213424960f966b33f1b85040f007990eddd01564d2dd9f43c4'
KNOWN = {10087: ('5afb2bcbd34f69528a5524e15b84ca2eaa128067a3300bcee80af9d5833f791b', 'mk2_10087_v0.wav', [13258, 13259, 13211]), 10167: ('5602ef29a5f7f2d9dcdc92a3442ff0c57a0572d76e305b06cfe928fa70088a7c', 'rpg7_10167_v0.wav', [13289, 13290])}

def render(document, waves):
    cue = document['cue']
    if cue not in KNOWN or len(document['sequences']) != 1:
        raise ValueError('cue')
    seq = document['sequences'][0]
    if sha(bytes.fromhex(seq['raw_hex'])) != KNOWN[cue][0]:
        raise ValueError('sequence')
    if [r['wave_id'] for r in seq['note_references']] != KNOWN[cue][2]:
        raise ValueError('wave binding')
    if int.from_bytes(bytes.fromhex(seq['header_hex'])[44:46], 'big') != 48:
        raise ValueError('division')
    tick = f32(479999 / 48) / 1000000.0
    rate = 48000
    frames = math.ceil(seq['duration_ticks'] * tick * rate)
    channels = {}
    active = {}
    notes = []
    refs = {r['event_index']: r for r in seq['note_references']}
    left = array.array('f', [0]) * frames
    right = array.array('f', [0]) * frames
    for index, e in enumerate(seq['events']):
        if 'channel' not in e:
            if e.get('meta') not in (47, 81) or (e['meta'] == 81 and (e['tick'] or e['payload_hex'] != '0752ff')):
                raise ValueError('metadata')
            continue
        ch = e['channel']
        op = e['status'] >> 4
        data = e['data']
        cc = channels.setdefault(ch, {})
        if op == 11:
            if e['tick']:
                raise ValueError('automation')
            cc[data[0]] = data[1]
        elif op == 12:
            if e['tick']:
                raise ValueError('program')
        elif op == 9 and data[1]:
            if ch in active or data[1] != 127:
                raise ValueError('note overlap/velocity')
            active[ch] = (e, refs[index], dict(cc))
        elif op == 9 and (not data[1]):
            if ch not in active:
                raise ValueError('unpaired note')
            start, ref, controls = active.pop(ch)
            if data[0] != ref['note'] or ref['note'] != 60:
                raise ValueError('pitch')
            if ch == 0:
                if [controls.get(k) for k in range(24, 30)] != [0, 0, 0, 0, 0, 127]:
                    raise ValueError('LFE')
                gl = gr = 0.5
            else:
                if controls.get(10) != 64:
                    raise ValueError('pan')
                gl = gr = 1 / math.sqrt(2)
            samples, sampleRate, silent = waves[ref['wave_id']]
            step = sampleRate / rate
            begin = round(start['tick'] * tick * rate)
            off = round(e['tick'] * tick * rate)
            end = min(frames, off, begin + int((len(samples) - 1) / step))
            for at in range(begin, end):
                position = (at - begin) * step
                p = int(position)
                f = position - p
                value = (samples[p] * (1 - f) + samples[p + 1] * f) * controls[7] / 127
                left[at] += value * gl
                right[at] += value * gr
            notes.append(dict(wave=ref['wave_id'], channel=ch, volume=controls[7], begin=begin, end=end, noteOff=off, sampleRate=sampleRate, silentBlocks=silent))
        else:
            raise ValueError('unsupported sequence')
    if active or len(notes) != len(KNOWN[cue][2]):
        raise ValueError('unclosed notes')
    peak = max(max(map(abs, left)), max(map(abs, right)))
    if not math.isfinite(peak) or peak < 0.0001:
        raise ValueError('empty/nonfinite mix')
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
    return (stream.getvalue(), dict(frames=frames, rate=rate, notes=notes, peak=peak, headroom=gain))
