# Generated converter-only source; historical analysis entry points omitted.
"""Resolve the reviewed START sequence to source waveforms, without synthesis.

GWS v2 here is an analysis asset. SSW2/SSWF bytes are evidence, not native PCM.
The narrow contract rejects implicit initial instruments and unsupported events.
"""
import argparse
import copy
import json
from pathlib import Path
from elf_image import ElfImage
from extract_start_sound import ROOT, chunks, events, u32
from native_assets import atomic, sha
ELF_SHA = '1a55a41ee5afdebd89075bd205e412c8311f569ba9dc022f629bb63fb4bfd13a'
EVIDENCE_FUNCTIONS = (2715448, 2713184, 2747184, 2741456, 2742728, 2746792, 2746888, 2741872, 2699344, 2694568, 2697224, 2697496)
START_CONTROLLERS = (0, 7, 9, 10, 24, 25, 26, 27, 28, 29, 32, 71, 75, 91)

def contracts(elf):
    if elf.sha256 != ELF_SHA:
        raise ValueError('unreviewed ELF revision')
    toc = 19016192
    controller_table = elf.u32(elf.u32(toc - 31208) - 32768)
    handler_table = elf.u32(elf.u32(toc - 31212) - 32640)
    rows = []
    for cc in START_CONTROLLERS:
        event_id = elf.u32(controller_table + 4 * cc)
        if event_id > 38:
            raise ValueError('controller event table range')
        opd = elf.u32(handler_table + 4 * event_id)
        if elf.u32(opd + 4) != toc:
            raise ValueError('event handler TOC')
        rows.append({'controller': cc, 'event_id': event_id, 'handler': hex(elf.u32(opd)), 'opd': hex(opd)})
    handlers = {row['controller']: row['handler'] for row in rows}
    if (handlers[0], handlers[32]) != ('0x29ea08', '0x29e9a8'):
        raise ValueError('instrument bank handlers')
    if elf.u32(elf.u32(handler_table + 35 * 4)) != 2742728:
        raise ValueError('program handler')
    dispatch_base = elf.u32(toc - 31264)
    format_table = elf.u32(dispatch_base - 32692)
    ssw_magic = elf.u32(dispatch_base - 32700)
    ssw_opd = elf.u32(format_table + 8)
    if elf.read(ssw_magic, 4) != b'SSW2' or elf.u32(ssw_opd) != 2697224 or elf.u32(ssw_opd + 4) != toc:
        raise ValueError('SSW2 dispatch')
    sswf_opd = elf.u32(format_table + 12)
    if elf.read(elf.u32(dispatch_base - 32696), 4) != b'SSWF' or elf.u32(sswf_opd) != 2697496 or elf.u32(sswf_opd + 4) != toc:
        raise ValueError('SSWF dispatch')
    return {'elf_sha256': elf.sha256, 'toc': hex(toc), 'functions': [hex(x) for x in EVIDENCE_FUNCTIONS], 'controller_table': hex(controller_table), 'event_handler_table': hex(handler_table), 'controllers': rows, 'ssw2_handler': hex(elf.u32(ssw_opd)), 'sswf_handler': hex(elf.u32(sswf_opd)), 'ssw2_format_table': hex(format_table), 'instrument_formula': '(controller0 << 14) | (controller32 << 7) | program', 'wave_lookup': 'IWAV count+8, records+16 stride32, compare BE u16 at record+22; BWAV + BE u32 record+0', 'reviewed_instruction_bytes': {hex(a): elf.read(a, n).hex() for a, n in ((2713184, 116), (2742768, 24), (2746832, 32), (2746928, 32), (2741976, 68), (2697248, 108), (2697496, 116))}}

def ssw2_header(raw):
    if len(raw) < 32 or raw[:4] != b'SSW2' or u32(raw, 4) + 8 != len(raw):
        raise ValueError('SSW2 extent/magic')
    if u32(raw, 16) not in (32, 33) or raw[21] != 1:
        raise ValueError('SSW2 unsupported header (PPU 292808)')
    return {'source_format': 'SSW2', 'source_header_hex': raw[:32].hex(), 'payload_offset': 32, 'payload_bytes': u32(raw, 4) - 24, 'lower_format_id': 3, 'rate_encoding': u32(raw, 16), 'rate_code': int.from_bytes(raw[22:24], 'big'), 'decoder_parameter': raw[20] << 5, 'lower_word4': 0, 'lower_word5_signed': int.from_bytes(raw[28:32], 'big', signed=True), 'uninterpreted_header_words': {hex(p): hex(u32(raw, p)) for p in (8, 12, 24)}, 'payload_sha256': sha(raw[32:]), 'unresolved': ['payload codec', 'rate conversion function/rounding', 'decoder parameter units', 'lower word5 semantics and loop units']}

def sswf_header(raw):
    if len(raw) < 16 or raw[:4] != b'SSWF' or 16 + 2 * u32(raw, 12) != len(raw):
        raise ValueError('SSWF extent/magic')
    if raw[4:6] != b'\x01\x01':
        raise ValueError('SSWF unsupported header (PPU 292918)')
    loop = u32(raw, 8)
    return {'source_format': 'SSWF', 'source_header_hex': raw[:16].hex(), 'payload_offset': 16, 'payload_bytes': 2 * u32(raw, 12), 'lower_format_id': 0, 'lower_rate_field': int.from_bytes(raw[6:8], 'big'), 'source_count': u32(raw, 12), 'decoder_parameter': 0, 'lower_word4': 0, 'lower_word5_signed': -1 if loop == 2147483647 else (2 * loop + 2 ** 31) % 2 ** 32 - 2 ** 31, 'payload_sha256': sha(raw[16:]), 'unresolved': ['lower format0 sample representation/endianness', 'lower word5 semantics and loop units']}

def note_references(parsed):
    """Apply the three instrument setters in source order, including equal ticks.

    No default bank/program is inferred. 29EB30 maps both 80/90 to one handler;
    29D670 tests velocity alone. Do not replace that with General MIDI rules.
    """
    state = [[None, None, None] for _ in range(16)]
    result = []
    for index, event in enumerate(parsed):
        status = event['status']
        if status == 255:
            if event['meta'] not in (47, 81):
                raise ValueError('unreviewed meta event')
            continue
        channel = event['channel']
        if not 0 <= channel < 16 or status & 15 != channel:
            raise ValueError('event channel')
        data = event['data']
        if any((not 0 <= x < 128 for x in data)):
            raise ValueError('event value')
        kind = status >> 4
        if kind == 11 and len(data) == 2:
            if data[0] in (0, 32):
                state[channel][0 if data[0] == 0 else 1] = data[1]
        elif kind == 12 and len(data) == 1:
            state[channel][2] = data[0]
        elif kind in (8, 9) and len(data) == 2:
            if data[1]:
                msb, lsb, program = state[channel]
                if None in (msb, lsb, program):
                    raise ValueError('implicit initial instrument is not reviewed')
                wave_id = msb << 14 | lsb << 7 | program
                if wave_id > 65535:
                    raise ValueError('instrument id cannot match IWAV u16')
                result.append({'event_index': index, 'event_offset': event['offset'], 'tick': event['tick'], 'channel': channel, 'note': data[0], 'velocity': data[1], 'bank_msb': msb, 'bank_lsb': lsb, 'program': program, 'wave_id': wave_id})
        elif kind == 14 and len(data) == 2:
            pass
        else:
            raise ValueError('unreviewed event shape')
    return result

def resolve(gws, source, evidence, controllers=None):
    if (gws.get('format'), gws.get('version')) != ('MGO2MT.GWS', 1):
        raise ValueError('expected analysis GWS v1')
    if sha(source) != gws['source_sha256']:
        raise ValueError('source hash changed')
    begin, size = (gws['bank_offset'], gws['bank_size'])
    if begin < 0 or size < 64 or begin + size > len(source):
        raise ValueError('bank bounds')
    bank = source[begin:begin + size]
    if sha(bank) != gws['bank_sha256']:
        raise ValueError('bank hash changed')
    parts = chunks(bank)
    icue_at, icue = parts['ICUE']
    cue_at = gws['cue_record_offset'] - begin - icue_at
    cue_raw = bytes.fromhex(gws['cue_record_hex'])
    if len(cue_raw) != 32 or cue_at < 16 or (cue_at - 16) % 32 or (cue_at + 32 > len(icue)) or (icue[cue_at:cue_at + 32] != cue_raw) or (int.from_bytes(cue_raw[:2], 'big') != gws['cue']):
        raise ValueError('cue provenance')
    cue_indices = [int.from_bytes(cue_raw[16:18], 'big')]
    for at in range(18, 32, 2):
        index = int.from_bytes(cue_raw[at:at + 2], 'big')
        if not index:
            break
        cue_indices.append(index)
    if cue_indices != [s['index'] for s in gws['sequences']]:
        raise ValueError('cue sequence references')
    iwav_at, iwav = parts['IWAV']
    bwav_at, bwav = parts['BWAV']
    count = u32(iwav, 8)
    if len(iwav) != 16 + 32 * count:
        raise ValueError('IWAV table extent')
    wave_table = {}
    for i in range(count):
        record = iwav[16 + i * 32:48 + i * 32]
        wave_id = int.from_bytes(record[22:24], 'big')
        if wave_id in wave_table:
            raise ValueError('ambiguous IWAV wave id')
        wave_table[wave_id] = (i, record)
    sequences = []
    required = set()
    for sequence in gws['sequences']:
        raw = bytes.fromhex(sequence['raw_hex'])
        offset = sequence['source_offset']
        tsnd = parts['TSND'][1]
        if sequence['index'] >= u32(tsnd, 8) or offset != begin + parts['ISND'][0] + u32(tsnd, 16 + 4 * sequence['index']):
            raise ValueError('TSND sequence reference')
        if not begin <= offset or offset + len(raw) > begin + size or source[offset:offset + len(raw)] != raw:
            raise ValueError('sequence provenance')
        if raw[48:52] != b'MTrk':
            raise ValueError('sequence track tag')
        track_size = u32(raw, 52)
        if track_size > len(raw) - 56 or any(raw[56 + track_size:]):
            raise ValueError('sequence track bounds')
        parsed = events(raw[56:56 + track_size])
        if parsed != sequence['events']:
            raise ValueError('GWS events changed')
        for event in parsed:
            event_raw = bytes.fromhex(event['raw_hex'])
            i = 0
            while event_raw[i] & 128:
                i += 1
            if event_raw[i + 1] < 128:
                raise ValueError('running status is not supported by reviewed PPU parser')
            if event['status'] >> 4 == 11 and event['data'][0] not in (START_CONTROLLERS if controllers is None else controllers):
                raise ValueError('controller outside the reviewed START subset')
        refs = note_references(parsed)
        required.update((x['wave_id'] for x in refs))
        sequences.append({**sequence, 'note_references': refs})
    waves = []
    for wave_id in sorted(required):
        if wave_id not in wave_table:
            raise ValueError('missing wave id ' + str(wave_id))
        i, record = wave_table[wave_id]
        offset = u32(record, 0)
        if offset < 16 or offset + 16 > len(bwav):
            raise ValueError('BWAV wave offset')
        tag = bwav[offset:offset + 4]
        if tag == b'SSW2':
            size, header_reader = (u32(bwav, offset + 4) + 8, ssw2_header)
        elif tag == b'SSWF':
            size, header_reader = (16 + 2 * u32(bwav, offset + 12), sswf_header)
        else:
            raise ValueError('unreviewed wave format')
        if offset + size > len(bwav):
            raise ValueError('BWAV wave extent')
        raw = bwav[offset:offset + size]
        waves.append({'wave_id': wave_id, 'iwav_index': i, 'iwav_record_hex': record.hex(), 'iwav_record_source_offset': begin + iwav_at + 16 + i * 32, 'bwav_offset': offset, 'source_offset': begin + bwav_at + offset, 'source_size': size, 'source_sha256': sha(raw), 'source_waveform_hex': raw.hex(), 'header': header_reader(raw)})
    result = copy.deepcopy(gws)
    result.update(version=2, runtime_status='analysis_only_no_decoder_or_synthesizer', evidence=evidence, sequences=sequences, waveform_dependencies=waves, unresolved=['SSW2/SSWF decoding to PCM', 'tempo/timebase and scheduler', 'custom controllers/envelopes/effects', 'native SE playback'])
    return result

def convert(gws_path, destination, elf_path):
    gws_path, destination, elf_path = map(lambda p: Path(p).resolve(), (gws_path, destination, elf_path))
    gws_bytes = gws_path.read_bytes()
    gws = json.loads(gws_bytes)
    source_path = Path(gws['source']).resolve()
    sidecar = destination.with_suffix(destination.suffix + '.conversion.json')
    if destination in (gws_path, source_path, elf_path) or sidecar in (gws_path, source_path, elf_path):
        raise ValueError('input/output collision')
    source = source_path.read_bytes()
    elf = ElfImage(elf_path)
    evidence = contracts(elf)
    fingerprint = {'converter': 'gws-wave-dependencies-v2', 'input_gws_sha256': sha(gws_bytes), 'source_sha256': sha(source), 'elf_sha256': elf.sha256, 'settings': {'implicit_instruments': 'reject', 'decode': False}, 'tools': {n: sha((ROOT / 'tools' / n).read_bytes()) for n in ('resolve_start_sound.py', 'extract_start_sound.py', 'native_assets.py', 'elf_image.py')}}
    result = resolve(gws, source, evidence)
    result['input_gws'] = str(gws_path)
    result['input_gws_sha256'] = sha(gws_bytes)
    result['resolver_sha256'] = fingerprint['tools']['resolve_start_sound.py']
    output = (json.dumps(result, ensure_ascii=True, indent=2) + '\n').encode()
    manifest = {'fingerprint': fingerprint, 'output_sha256': sha(output), 'output_bytes': len(output), 'wave_count': len(result['waveform_dependencies']), 'note_count': sum((len(s['note_references']) for s in result['sequences']))}
    action = 'converted'
    if destination.exists() and sidecar.exists():
        try:
            if json.loads(sidecar.read_bytes()) == manifest and destination.read_bytes() == output:
                action = 'cached'
        except (ValueError, OSError):
            pass
    if action == 'converted':
        atomic(destination, output)
        atomic(sidecar, (json.dumps(manifest, indent=2) + '\n').encode())
    return {'action': action, 'output': str(destination), **manifest}
