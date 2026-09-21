# Generated converter-only source; historical analysis entry points omitted.
"""Lossless MDN material evidence and the little-endian GWM3 MAT3 trailer.

This module does not infer a physical material or enable a shader rule. Source
records retain their original big-endian bytes. Missing or unreviewed resources
are explicit and must leave the existing renderer fallback available.
"""
import math
import struct
UNRESOLVED = 4294967295
SOURCE_STRINGS = ('mdn_path', 'mdn_sha256', 'package_path', 'package_sha256', 'vertex_program_sha256', 'fragment_program_sha256', 'rule_id', 'fallback_reason')
HALF_LOADER_EVIDENCE = {'elf_sha256': '1a55a41ee5afdebd89075bd205e412c8311f569ba9dc022f629bb63fb4bfd13a', 'address': '0x115AC0 / 0x116920..0x116954', 'formula_u32': '((h & 0x8000)<<16) | ((h & 0x03ff)<<13) | (((h & 0x7c00)+0x1c000)<<13)', 'scope': 'Observed MGO2 loader only; draw-time overrides are not asserted'}

def loader_half_bits(word):
    """Exact bit construction at the audited MGO2 material loader."""
    if not 0 <= word <= 65535:
        raise ValueError('Packed material halfword range')
    return (word & 32768) << 16 | (word & 1023) << 13 | (word & 31744) + 114688 << 13

def _json_float(value):
    return value if math.isfinite(value) else repr(value)

def material_record(raw_material, raw_declaration, material_index, source, textures, uv1_semantic, fallback_reason=None):
    """Return JSON-safe evidence also accepted by encode_material_trailer.

    Each texture is {raw_reference: hex32B, embedded_image_index: u32,
    provenance: JSON-safe object}. Indices follow the MDN's active slot order.
    """
    if len(raw_material) != 112 or len(raw_declaration) != 48:
        raise ValueError('Original material/declaration record extent')
    words = struct.unpack_from('>12I', raw_material)
    if words[2] > 8 or words[3] > 8 or len(textures) != words[2]:
        raise ValueError('Original material active texture/vector count')
    halves = struct.unpack_from('>32H', raw_material, 48)
    ieee = [_json_float(struct.unpack('>e', struct.pack('>H', h))[0]) for h in halves]
    runtime_bits = [loader_half_bits(h) for h in halves]
    runtime = [struct.unpack('>f', struct.pack('>I', h))[0] for h in runtime_bits]
    result = {key: str(source.get(key, '')) for key in SOURCE_STRINGS}
    result.update({'raw_material': raw_material.hex(), 'raw_vertex_declaration': raw_declaration.hex(), 'material_index': material_index, 'material_id': str(source.get('mdn_sha256', '')) + ':' + str(material_index), 'original_key': words[0], 'material_name_hash': words[1], 'active_texture_count': words[2], 'active_vector_count': words[3], 'texture_reference_indices': list(words[4:4 + words[2]]), 'raw_halfwords': [f'{h:04X}' for h in halves], 'ieee_half_float4': [ieee[i:i + 4] for i in range(0, 32, 4)], 'loader_float32_bits': [f'{h:08X}' for h in runtime_bits], 'loader_float4': [runtime[i:i + 4] for i in range(0, 32, 4)], 'inactive_vector_policy': 'Raw values retained; only P0..active_vector_count-1 are bound', 'half_loader_evidence': HALF_LOADER_EVIDENCE, 'restoration_flags': 0, 'normal_slot': UNRESOLVED, 'extra_normal_slot': UNRESOLVED, 'reflection_slot': UNRESOLVED, 'palette_slot': UNRESOLVED, 'uv1_semantic': uv1_semantic, 'textures': textures, 'rule_id': '', 'fallback_reason': fallback_reason or 'No reviewed model/package/VFP shader rule attached; retain existing diffuse rendering'})
    return result

def encode_material_trailer(records):
    """MAT3 + count, then sized records; no implicit padding or alignment."""
    import json

    def string(value):
        value = value.encode('utf-8')
        if len(value) > 65536:
            raise ValueError('Material evidence string exceeds 64 KiB')
        return struct.pack('<I', len(value)) + value
    out = bytearray(b'MAT3' + struct.pack('<I', len(records)))
    for item in records:
        raw = bytes.fromhex(item['raw_material'])
        declaration = bytes.fromhex(item['raw_vertex_declaration'])
        if len(raw) != 112 or len(declaration) != 48:
            raise ValueError('MAT3 raw record extent')
        textures = item['textures']
        count = struct.unpack_from('>I', raw, 8)[0]
        if count > 8 or count != len(textures):
            raise ValueError('MAT3 texture count')
        body = bytearray(raw + declaration)
        body.extend(struct.pack('<6I', item['material_index'], item['restoration_flags'], item['normal_slot'], item['extra_normal_slot'], item['reflection_slot'], item['palette_slot']))
        for key in SOURCE_STRINGS:
            body.extend(string(item[key]))
        body.extend(struct.pack('<I', count))
        for texture in textures:
            reference = bytes.fromhex(texture['raw_reference'])
            if len(reference) != 32:
                raise ValueError('MAT3 texture reference extent')
            body.extend(reference)
            body.extend(struct.pack('<I', texture['embedded_image_index']))
            provenance = texture['provenance']
            body.extend(string(provenance if isinstance(provenance, str) else json.dumps(provenance, ensure_ascii=False, sort_keys=True, allow_nan=False)))
        if len(body) > 1048576:
            raise ValueError('MAT3 material record exceeds 1 MiB')
        out.extend(struct.pack('<I', len(body)))
        out.extend(body)
    return bytes(out)
