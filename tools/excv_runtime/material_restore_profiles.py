# Generated converter-only source; historical analysis entry points omitted.
"""Reviewable source-only provisional material profile join (not yet executed).

No import side effects, asset conversion, file writes, shader compilation or
runtime probing. Call build_profiles only after the current build/conversion
hold is lifted. An explicit per-MDN assignment is mandatory: asset package
membership is never guessed from a key, basename, or a global DDS lookup.

assignments = {"<MDN SHA256>:<material index>": {
    "shader_package": "base" or "patch", "variant": 0,
    "source_package_sha256": "<original DAR/QAR SHA256>",
    "source_package_path": "<original container path>",
    "context_evidence": "<reviewed model-to-shader-package evidence path>"}}

The result is dict[material_id] -> profile. Existing converters may call
apply_profile(record, profiles) directly after original_material.material_record.
Missing assignments retain the existing fallback. Variant 0 is an explicit
Windows provisional choice, never an observation of the live RSX variant.
"""
import copy
import hashlib
import json
import math
from pathlib import Path
import struct
UNRESOLVED = 4294967295
PACKAGES = {'base': '9f0e7152f8316058e5610d2b0b1ebfcb8aa34adeb1066d19540f02a41cd98dd2', 'patch': 'f3817d31657c86d0324eb91e118b2b8a71fbd397854e59ca1dcc32d199a7ffd0'}
EXTRA = {16385: 3, 16387: 4, 16544: 6, 20483: 4, 20640: 6}
NORMAL = {1048576, 1179648, 16, 80, 97} | set(EXTRA)
COLOR_MASK = {4097}

def _load(path):
    with Path(path).open(encoding='utf-8') as stream:
        return json.load(stream)

def _sha(value):
    return isinstance(value, str) and len(value) == 64 and all((c in '0123456789abcdef' for c in value))

def _program(blob, kind, index):
    """Full Cg blob and ucode are different identities; preserve both."""
    if kind not in ('vp', 'fp') or not isinstance(index, int) or index < 0:
        raise ValueError('VFP program identity')
    header = struct.unpack_from('>8I', blob)
    count, table, stride = (header[4], header[5], 16) if kind == 'vp' else (header[6], header[7], 32)
    if index >= count or table + stride * (index + 1) > len(blob):
        raise ValueError('VFP program table bounds')
    offset, size = struct.unpack_from('>2I', blob, table + index * stride)
    if size < 32 or offset + size > len(blob):
        raise ValueError('VFP program extent')
    program = blob[offset:offset + size]
    words = struct.unpack_from('>8I', program)
    code_size, code_offset = (words[6], words[7])
    if code_offset + code_size > len(program):
        raise ValueError('VFP microcode extent')
    return {'index': index, 'program_sha256': hashlib.sha256(program).hexdigest(), 'microcode_sha256': hashlib.sha256(program[code_offset:code_offset + code_size]).hexdigest()}

def build_profiles(evidence_root, assignments):
    """Join saved static evidence with explicit reviewed context assignments.

    Raises on a stale or contradictory assignment instead of choosing another
    shader package. Does not write a profile or modify any input asset.
    """
    root = Path(evidence_root)
    shader = _load(root / 'shader_comparison.json')
    bits = _load(root / 'key-bits-followup/comparison.json')
    selected = _load(root / 'followup/selected_programs.json')
    materials = _load(root / 'cpu-binding/runtime_material_parameters.json')['materials']
    wanted = {row['material_id']: row for row in materials if row['material_id'] in assignments}
    by_label = {row['label']: row for row in shader['packages']}
    bits_by_label = {row['label']: row for row in bits['packages']}
    selected_by_label = {row['package']: row for row in selected}
    result, blobs = ({}, {})
    for material_id, assignment in assignments.items():
        if material_id not in wanted:
            raise ValueError(f'Material missing from saved runtime evidence: {material_id}')
        row = wanted[material_id]
        label = assignment.get('shader_package')
        if label not in PACKAGES or assignment.get('variant') != 0:
            raise ValueError(f'Explicit audited package and provisional variant 0 required: {material_id}')
        if not _sha(assignment.get('source_package_sha256')) or not assignment.get('source_package_path') or (not assignment.get('context_evidence')):
            raise ValueError(f'Reviewed source-container identity/context required: {material_id}')
        key = int(row['selector'], 16)
        package = by_label[label]
        if package['sha256'] != PACKAGES[label]:
            raise ValueError('Saved VFP source fingerprint changed')
        if label not in blobs:
            blob = Path(package['path']).read_bytes()
            if hashlib.sha256(blob).hexdigest() != PACKAGES[label]:
                raise ValueError('Original VFP source fingerprint changed')
            blobs[label] = blob
        base_profile = {'material_id': material_id, 'original_key': key, 'mdn_sha256': row['model_sha256'], 'material_index': row['material_index'], 'expected_source_package_sha256': assignment['source_package_sha256'], 'expected_source_package_path': assignment['source_package_path'], 'context_evidence': assignment['context_evidence'], 'package_path': package['path'], 'package_sha256': package['sha256'], 'selection_status': 'explicit_provisional_variant_0_not_live_observed', 'variant': 0, 'restoration_flags': 0, 'normal_slot': UNRESOLVED, 'extra_normal_slot': UNRESOLVED, 'reflection_slot': UNRESOLVED, 'palette_slot': UNRESOLVED, 'evidence': [str(root / 'cpu-binding/analysis_findings.json'), str(root / 'followup/analysis_findings.json'), str(root / 'key-bits-followup/analysis_findings.json'), str(root / 'color-uniform-followup-20260915/analysis_findings.json')]}
        if key not in NORMAL | COLOR_MASK:
            base_profile['fallback_reason'] = 'Full selector not yet enabled; palette/color-mask source values retained only'
            result[material_id] = base_profile
            continue
        if key in EXTRA:
            comparisons = bits_by_label[label]['comparisons']
            comp = next((c for c in comparisons if int(c['with'], 16) == key))
            variant = next((v for v in comp['variants'] if v['ordinal'] == 0))
            vp_index, fp_index = (variant['vp']['with'], variant['fp']['with'])
        else:
            group = next((g for g in selected_by_label[label]['groups'] if int(g['selector'], 16) == key))
            vp_index, fp_index = (group['vp']['index'], group['fp']['index'])
        vp, fp = (_program(blobs[label], 'vp', vp_index), _program(blobs[label], 'fp', fp_index))
        base_profile.update(vertex_program_sha256=vp['program_sha256'], fragment_program_sha256=fp['program_sha256'], shader_programs={'vp': vp, 'fp': fp}, restoration_flags=8 if key in COLOR_MASK else 3 if key in EXTRA else 1, normal_slot=UNRESOLVED if key in COLOR_MASK else 1, extra_normal_slot=EXTRA.get(key, UNRESOLVED), reflection_slot={1048576: 3, 16: 3, 97: 4}.get(key, UNRESOLVED))
        base_profile['fallback_reason'] = 'Provisional AG normal; derivative TBN/native lighting approximate; original reflection sampler dimensionality/composition unresolved; palette and P36..38 inactive; no live variant/face animation inferred'
        if key in (16544, 20640):
            base_profile['fallback_reason'] = 'Audited extra-normal pair uses TEX1.xy; normal/mix inactive until its VP UV consumer is preserved'
        if key in COLOR_MASK:
            base_profile['fallback_reason'] = 'Original shared P36..38 defaults verified; no MDN parameter alias; requires preserved original RGBA8 COLOR and explicit effective c467.x per draw; unknown inputs retain legacy rendering; no equipment UI mapping'
        result[material_id] = base_profile
    return result

def apply_profile(record, profiles):
    """Apply one identity/context-checked profile, preserving original evidence.

    GWM3 package_path/SHA identify the VFP after application. Original source
    container fields remain in source_package and the serialized rule_id JSON.
    Original parameters and texture provenance are never changed here.
    """
    item = copy.deepcopy(record)
    profile = profiles.get(item['material_id'])
    if profile is None:
        return item
    if profile.get('material_id') != item['material_id'] or profile.get('original_key') != item['original_key']:
        raise ValueError('Provisional profile MDN/material/key identity mismatch')
    expected = profile.get('expected_source_package_sha256')
    if not _sha(expected) or expected != item['package_sha256']:
        raise ValueError('Provisional shader profile source-container SHA mismatch')
    if str(profile.get('expected_source_package_path', '')).replace('\\', '/').casefold() != item['package_path'].replace('\\', '/').casefold():
        raise ValueError('Provisional shader profile source-container path mismatch')
    if profile.get('variant') != 0 or profile.get('selection_status') != 'explicit_provisional_variant_0_not_live_observed':
        raise ValueError('Missing explicit provisional shader variant selection')
    if profile.get('package_sha256') not in PACKAGES.values() or not profile.get('context_evidence'):
        raise ValueError('Unreviewed shader package/context assignment')
    source_package = {'path': item['package_path'], 'sha256': item['package_sha256']}
    item['source_package'] = source_package
    item['profile_evidence'] = copy.deepcopy(profile)
    for name in ('package_path', 'package_sha256', 'vertex_program_sha256', 'fragment_program_sha256', 'restoration_flags', 'normal_slot', 'extra_normal_slot', 'reflection_slot', 'palette_slot', 'fallback_reason'):
        if name in profile:
            item[name] = profile[name]
    item['rule_id'] = json.dumps({'rule': 'mgo2.original-material.provisional.v0', 'variant': 0, 'runtime_variant_observed': False, 'source_package': source_package, 'context_evidence': profile['context_evidence']}, sort_keys=True, ensure_ascii=False, allow_nan=False)
    if item.get('restoration_flags', 0) != 8 and item['active_vector_count'] < 1 or any((not math.isfinite(v) for vector in item['loader_float4'][:item['active_vector_count']] for v in vector)):
        item['restoration_flags'] = 0
        item['fallback_reason'] = 'Missing/non-finite active source coefficients'
    return item
