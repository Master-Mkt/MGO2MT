# Generated converter-only source; historical analysis entry points omitted.
"""SSPF cue -> TSND index -> ISND sequence, checked against PPU 296F38.
GWS v1 is analysis-only: preserve raw records and controller events, no synthesizer.
"""
from pathlib import Path
import json
import struct
from native_assets import atomic, sha
ROOT = Path(__file__).resolve().parents[1]

def u32(b, p):
    if p < 0 or p + 4 > len(b):
        raise ValueError('truncated u32')
    return struct.unpack_from('>I', b, p)[0]

def chunks(b):
    if b[:4] != b'SSPF' or u32(b, 8) != len(b):
        raise ValueError('SSPF extent')
    p = u32(b, 4) + 8
    result = {}
    while p < len(b):
        n = u32(b, p + 4) + 8
        if n < 16 or n > len(b) - p:
            raise ValueError('chunk extent')
        tag = b[p:p + 4].decode('ascii')
        if tag in result:
            raise ValueError('duplicate chunk')
        result[tag] = (p, b[p:p + n])
        p += n
    return result

def events(track):
    p = tick = 0
    running = None
    result = []

    def take(n):
        nonlocal p
        if n < 0 or p + n > len(track):
            raise ValueError('truncated MIDI event')
        data = track[p:p + n]
        p += n
        return data

    def vlq():
        n = 0
        for _ in range(4):
            byte = take(1)[0]
            n = n << 7 | byte & 127
            if byte < 128:
                return n
        raise ValueError('VLQ exceeds four bytes')
    while p < len(track):
        start = p
        delta = vlq()
        tick += delta
        first = take(1)[0]
        if first < 128:
            if running is None:
                raise ValueError('missing running status')
            p -= 1
            status = running
        else:
            status = first
        if status == 255:
            kind = take(1)[0]
            payload = take(vlq())
            event = {'meta': kind, 'payload_hex': payload.hex()}
        elif 128 <= status <= 239:
            running = status
            payload = take(1 if status >> 4 in (12, 13) else 2)
            if any((x >= 128 for x in payload)):
                raise ValueError('channel data byte range')
            event = {'channel': status & 15, 'data': list(payload)}
        else:
            raise ValueError('unsupported system event')
        result.append({'offset': start, 'tick': tick, 'delta': delta, 'status': status, **event, 'raw_hex': track[start:p].hex()})
        if len(result) > 10000:
            raise ValueError('event limit')
        if status == 255 and event['meta'] == 47:
            if event['payload_hex'] or p != len(track):
                raise ValueError('end-of-track extent')
            return result
    raise ValueError('missing end-of-track')

def extract(source, cue=18999):
    source = Path(source)
    data = source.read_bytes()
    p = 0
    matches = []
    while p < len(data):
        n = u32(data, p + 8)
        if n < 64 or n > len(data) - p:
            raise ValueError('bank extent')
        bank = data[p:p + n]
        parts = chunks(bank)
        icue_offset, icue = parts['ICUE']
        count = u32(icue, 8)
        if len(icue) != 16 + count * 32:
            raise ValueError('cue table extent')
        for i in range(count):
            record = icue[16 + i * 32:48 + i * 32]
            if int.from_bytes(record[:2], 'big') != cue:
                continue
            indices = [int.from_bytes(record[16:18], 'big')]
            for at in range(18, 32, 2):
                index = int.from_bytes(record[at:at + 2], 'big')
                if not index:
                    break
                indices.append(index)
            tsnd_offset, tsnd = parts['TSND']
            isnd_offset, isnd = parts['ISND']
            total = u32(tsnd, 8)
            used = 16 + 4 * total
            if not used <= len(tsnd) < used + 16 or any(tsnd[used:]) or u32(isnd, 8) != total:
                raise ValueError('sequence table extent')
            sequences = []
            for index in indices:
                if index >= total:
                    raise ValueError('sequence index')
                begin = u32(tsnd, 16 + index * 4)
                end = u32(tsnd, 20 + index * 4) if index + 1 < total else len(isnd)
                if begin < 16 or not begin < end <= len(isnd):
                    raise ValueError('sequence extent')
                raw = isnd[begin:end]
                if raw[48:52] != b'MTrk':
                    raise ValueError('unrecognized sequence layout')
                size = u32(raw, 52)
                if size > len(raw) - 56 or any(raw[56 + size:]):
                    raise ValueError('track extent/padding')
                ev = events(raw[56:56 + size])
                sequences.append({'index': index, 'source_offset': p + isnd_offset + begin, 'header_hex': raw[:48].hex(), 'raw_hex': raw.hex(), 'events': ev, 'channels_used': sorted({e['channel'] for e in ev if 'channel' in e}), 'duration_ticks': ev[-1]['tick']})
            matches.append({'bank_offset': p, 'bank_size': n, 'bank_sha256': sha(bank), 'cue_record_offset': p + icue_offset + 16 + i * 32, 'cue_record_hex': record.hex(), 'sequences': sequences})
        p = (p + n + 2047) // 2048 * 2048
    if len(matches) != 1:
        raise ValueError('cue missing/ambiguous')
    return {'format': 'MGO2MT.GWS', 'version': 1, 'runtime_status': 'analysis_only_no_synthesizer', 'cue': cue, 'source': str(source.resolve()), 'source_sha256': sha(data), 'converter_sha256': sha(Path(__file__).read_bytes()), 'evidence': ['0x296F38', '0x296BB0'], 'unresolved': ['instrument/sample bank selection', 'custom controller meanings', 'envelope/effects', 'SSW2 decoder'], **matches[0]}
