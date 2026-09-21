# Generated converter-only source; historical analysis entry points omitted.
"""Versioned native audio conversion with content-based caching and provenance.
GWA1 stores PCM + loop frames; it does not rename or embed a WAV container.
"""
from pathlib import Path
import hashlib
import json
import os
import struct
import tempfile
import wave
HEADER = struct.Struct('<4s5I3Q2IQ')

def sha(data):
    return hashlib.sha256(data).hexdigest()

def atomic(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, name = tempfile.mkstemp(dir=path.parent, prefix=path.name + '.', suffix='.tmp')
    try:
        with os.fdopen(fd, 'wb') as stream:
            stream.write(data)
        os.replace(name, path)
    finally:
        if os.path.exists(name):
            os.unlink(name)

def decode_gwa(data):
    if len(data) < HEADER.size:
        raise ValueError('GWA truncated header')
    magic, version, header, rate, channels, bits, frames, begin, end, flags, reserved, size = HEADER.unpack_from(data)
    if (magic, version, header, bits, reserved) != (b'GWA1', 1, 64, 16, 0):
        raise ValueError('GWA header contract')
    if not 1 <= channels <= 8 or not 8000 <= rate <= 192000 or (not frames) or (flags not in (0, 1)):
        raise ValueError('GWA audio contract')
    if size != frames * channels * 2 or size != len(data) - 64:
        raise ValueError('GWA PCM extent')
    if flags and (not 0 <= begin < end <= frames) or (not flags and (begin or end)):
        raise ValueError('GWA loop range')
    return {'sample_rate': rate, 'channels': channels, 'frames': frames, 'loop_start': begin, 'loop_end': end, 'pcm': data[64:]}

def convert_wav(source, destination, loop_start=0, loop_end=0):
    source, destination = (Path(source).resolve(), Path(destination).resolve())
    if source == destination:
        raise ValueError('source/output collision')
    data = source.read_bytes()
    fingerprint = {'converter': 'wav-to-gwa-v1', 'converter_sha256': sha(Path(__file__).read_bytes()), 'source': str(source), 'source_sha256': sha(data), 'options': {'loop_start': loop_start, 'loop_end': loop_end}}
    manifest = destination.with_suffix(destination.suffix + '.conversion.json')
    if destination.exists() and manifest.exists():
        previous = json.loads(manifest.read_text(encoding='utf-8'))
        if previous['fingerprint'] == fingerprint and sha(destination.read_bytes()) == previous['output_sha256']:
            decode_gwa(destination.read_bytes())
            return {'action': 'cached', **previous}
    with wave.open(str(source), 'rb') as audio:
        if audio.getsampwidth() != 2 or audio.getcomptype() != 'NONE':
            raise ValueError('expected 16-bit uncompressed PCM')
        channels, rate, frames = (audio.getnchannels(), audio.getframerate(), audio.getnframes())
        pcm = audio.readframes(frames)
    if sha(source.read_bytes()) != fingerprint['source_sha256']:
        raise ValueError('source changed during conversion')
    result = HEADER.pack(b'GWA1', 1, 64, rate, channels, 16, frames, loop_start, loop_end, int(bool(loop_end)), 0, len(pcm)) + pcm
    decoded = decode_gwa(result)
    if decoded['pcm'] != pcm:
        raise ValueError('PCM conversion mismatch')
    record = {'fingerprint': fingerprint, 'output': str(destination), 'output_sha256': sha(result), 'pcm_sha256': sha(pcm), 'size': len(result), 'audio': {k: v for k, v in decoded.items() if k != 'pcm'}}
    atomic(destination, result)
    atomic(manifest, (json.dumps(record, indent=2) + '\n').encode())
    return {'action': 'converted', **record}
