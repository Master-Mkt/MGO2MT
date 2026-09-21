# Generated converter-only source; historical analysis entry points omitted.
"""Render reviewed original body-impact cue variants into local PCM WAV files.

This intentionally accepts only the recovered single-note 1369/8168 sequences.
It reuses the source-verified decoder; it does not guess gunshot cue IDs or
the original four-variant selection policy. Nothing is installed or published.
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
from native_assets import atomic, sha
from render_start_sound import decode_wave, ELF_SHA
ROOT = Path(__file__).resolve().parents[1]

def render_variant(sequence, waves):
    events = sequence['events']
    notes = [e for e in events if e['status'] >> 4 == 9]
    tempo = [e for e in events if e.get('meta') == 81]
    division = int.from_bytes(bytes.fromhex(sequence['header_hex'])[44:46], 'big')
    if division != 48 or len(tempo) != 1 or tempo[0]['tick'] or (tempo[0]['payload_hex'] != '075300'):
        raise ValueError('Unreviewed sequence clock')
    if len(notes) != 2 or notes[0]['tick'] or notes[0]['data'][1] != 127 or (notes[1]['data'] != [notes[0]['data'][0], 0]):
        raise ValueError('Expected one closed full-velocity note')
    if any((e['data'][0] not in (0, 32, 121) for e in events if e['status'] >> 4 == 11)):
        raise ValueError('Unreviewed controller')
    if len(sequence['note_references']) != 1:
        raise ValueError('Single waveform required')
    reference = sequence['note_references'][0]
    samples, source_rate, silent = waves[reference['wave_id']]
    rate = 48000
    step = source_rate / rate * 2 ** ((notes[0]['data'][0] - 60) / 12)
    frames = min(round(notes[1]['tick'] * 0.01 * rate), int((len(samples) - 1) / step))
    values = []
    for at in range(frames):
        position = at * step
        index = int(position)
        fraction = position - index
        values.append((samples[index] * (1 - fraction) + samples[index + 1] * fraction) / math.sqrt(2))
    peak = max(map(abs, values))
    if not math.isfinite(peak) or peak < 0.0001:
        raise ValueError('Silent or invalid output')
    headroom = min(1.0, 0.85 / peak)
    pcm = array.array('h')
    for value in values:
        pcm.extend([round(value * headroom * 32767)] * 2)
    if sys.byteorder != 'little':
        pcm.byteswap()
    stream = io.BytesIO()
    with wave.open(stream, 'wb') as output:
        output.setparams((2, 2, rate, frames, 'NONE', 'not compressed'))
        output.writeframes(pcm.tobytes())
    data = stream.getvalue()
    with wave.open(io.BytesIO(data), 'rb') as check:
        assert check.getparams()[:4] == (2, 2, rate, frames)
        assert len(check.readframes(frames)) == frames * 4
    return (data, {'wave_id': reference['wave_id'], 'note': notes[0]['data'][0], 'source_rate': source_rate, 'frames': frames, 'headroom_gain': headroom, 'peak_before_headroom': peak, 'silent_source_blocks': silent})
