# Generated converter-only source; historical analysis entry points omitted.
"""Render only reviewed AK102 cue10002 using original waves and native stereo mixing.

Reuses the established SSPF/SSW2 readers and SPU decoder. This local preparation
does not modify an index, scene, package, original input or runtime configuration.
See notes/COMBAT_NORMAL_GUNSHOT_20260913.md for original evidence and native policy.
"""
import array
import io
import json
import math
from pathlib import Path
import struct
import sys
import wave
from elf_image import ElfImage
from extract_start_sound import extract
from native_assets import sha, atomic
import resolve_start_sound as resolver
from render_start_sound import decode_wave, f32, ELF_SHA
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'outputs/combat_normal_gunshot_20260913'
SOURCE_SHA = '0d38ee1e5389ad213424960f966b33f1b85040f007990eddd01564d2dd9f43c4'
SEQUENCE_SHA = 'b43ab2155a787290c720970c827e2670bad9b48e59318ee9d8bcdf2ec9d95430'

def render_nearest(document, waves):
    if document['cue'] != 10002 or len(document['sequences']) != 1:
        raise ValueError('Only reviewed AK102 near cue10002 is enabled')
    seq = document['sequences'][0]
    if sha(bytes.fromhex(seq['raw_hex'])) != SEQUENCE_SHA:
        raise ValueError('Unreviewed original sequence')
    if int.from_bytes(bytes.fromhex(seq['header_hex'])[44:46], 'big') != 48:
        raise ValueError('Unreviewed division')
    tempo = [e for e in seq['events'] if e.get('meta') == 81]
    if len(tempo) != 1 or tempo[0]['tick'] != 0 or tempo[0]['payload_hex'] != '0752ff':
        raise ValueError('Unreviewed tempo')
    tick_seconds = f32(479999 / 48) / 1000000.0
    rate = 48000
    frames = math.ceil(seq['duration_ticks'] * tick_seconds * rate)
    left = array.array('f', [0]) * frames
    right = array.array('f', [0]) * frames
    refs = {r['event_index']: r for r in seq['note_references']}
    if [(r['channel'], r['note'], r['wave_id']) for r in seq['note_references']] != [(1, 60, 13224), (2, 62, 13201), (0, 60, 13225)]:
        raise ValueError('Unreviewed instruments')
    controls = [{} for _ in range(3)]
    active = {}
    notes = []
    for index, event in enumerate(seq['events']):
        if 'channel' not in event:
            if event.get('meta') not in (47, 81):
                raise ValueError('Unsupported metadata')
            continue
        ch = event['channel']
        if ch not in range(3):
            raise ValueError('Unreviewed channel')
        op, data = (event['status'] >> 4, event['data'])
        if op == 11:
            if event['tick'] or data[0] not in (0, 32, 7, 10, 24, 25, 26, 27, 28, 29):
                raise ValueError('Unreviewed controller or automation')
            controls[ch][data[0]] = data[1]
        elif op == 12:
            if event['tick']:
                raise ValueError('Unreviewed program change')
        elif op == 9 and data[1]:
            if ch in active or data[1] != 127:
                raise ValueError('Overlapping or unreviewed note')
            active[ch] = (event, refs[index], dict(controls[ch]))
        elif op == 9 and (not data[1]):
            if ch not in active:
                raise ValueError('Unmatched note-off')
            start, ref, cc = active.pop(ch)
            if data[0] != start['data'][0]:
                raise ValueError('Mismatched note-off')
            if ch == 0:
                if [cc.get(k) for k in range(24, 30)] != [0, 0, 0, 0, 0, 127] or cc[7] != 40:
                    raise ValueError('Unreviewed LFE channel controls')
                gl = gr = 0.5
            else:
                if cc.get(10) != 64 or cc[7] != (127 if ch == 1 else 70):
                    raise ValueError('Unreviewed centered channel controls')
                gl = gr = 1 / math.sqrt(2)
            samples, source_rate, silent = waves[ref['wave_id']]
            step = source_rate / rate * 2 ** ((ref['note'] - 60) / 12)
            begin = round(start['tick'] * tick_seconds * rate)
            off = round(event['tick'] * tick_seconds * rate)
            end = min(frames, off, begin + int((len(samples) - 1) / step))
            for at in range(begin, end):
                pos = (at - begin) * step
                p, fraction = (int(pos), pos - int(pos))
                value = (samples[p] * (1 - fraction) + samples[p + 1] * fraction) * cc[7] / 127
                left[at] += value * gl
                right[at] += value * gr
            notes.append({'channel': ch, 'wave_id': ref['wave_id'], 'note': ref['note'], 'source_rate': source_rate, 'start_frame': begin, 'note_off_frame': off, 'end_frame': end, 'silent_source_blocks': silent, 'channel_volume': cc[7]})
        else:
            raise ValueError('Unreviewed event')
    if active or len(notes) != 3:
        raise ValueError('Unclosed or missing original notes')
    peak = max(max(map(abs, left)), max(map(abs, right)))
    if not math.isfinite(peak) or peak < 0.0001:
        raise ValueError('Invalid or silent mix')
    headroom = min(1.0, 0.85 / peak)
    pcm = array.array('h')
    for l, r in zip(left, right):
        pcm.extend((round(l * headroom * 32767), round(r * headroom * 32767)))
    if sys.byteorder != 'little':
        pcm.byteswap()
    stream = io.BytesIO()
    with wave.open(stream, 'wb') as wav:
        wav.setparams((2, 2, rate, frames, 'NONE', 'not compressed'))
        wav.writeframes(pcm.tobytes())
    return (stream.getvalue(), {'frames': frames, 'rate': rate, 'notes': notes, 'tick_seconds': tick_seconds, 'peak_before_headroom': peak, 'headroom_gain': headroom})
