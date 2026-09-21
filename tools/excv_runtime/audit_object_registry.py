# Generated converter-only source; historical analysis entry points omitted.
"""Private, bounded GCX/GEOM registration audit. Never produces a live registry.

PPC source identity and registration/vtable evidence are checked below. A
procedure's literal order is not proof of successful runtime constructors or
of all other registration paths. No E1 snapshot request is enabled here.
"""
import json
from pathlib import Path
from stage_scene import ROOT, scripts, geometry, lighting, foreach_rows
from elf_image import ElfImage
from gwp import record
ELF_SHA = '1a55a41ee5afdebd89075bd205e412c8311f569ba9dc022f629bb63fb4bfd13a'
FACTORIES = {516601: ('car', 6041336, 6037448, 1, -30200, -32296), 10148454: ('blast_drum', 5977176, 5981644, 1, -30212, -32664), 9672405: ('bottle_group', 5805320, 5807144, 8, -30252, -32620), 11892685: ('cbox', 7592656, 7594444, 2, -29464, -32512)}

def expand(procedures, entry):
    """Literal calls and E1A50 loops only; opaque command bodies stay opaque."""
    result = []

    def scalar(node, args):
        if node['kind'] == 'argument':
            i = node['value'] - 1
            if not 0 <= i < len(args):
                raise ValueError('unbound script argument')
            return args[i]
        return {k: v for k, v in node.items() if k in ('kind', 'value', 'code', 'count', 'descriptor')}

    def visit(nodes, proc, args, path):
        if len(path) > 32 or len(result) > 4096:
            raise ValueError('script expansion limit')
        for n in nodes:
            kind = n['kind']
            if kind == 'block':
                visit(n['children'], proc, args, path)
            elif kind == 'proc_call':
                target = n['procedure_id']
                if target in path:
                    raise ValueError('recursive script audit')
                call_args = [scalar(x, args) for x in n['children'] if x['kind'] != 'terminator']
                visit(procedures[target]['nodes'], target, call_args, path + [target])
            elif kind == 'command':
                first = n.get('arguments', [{}])[0].get('value')
                if n.get('code') == '0x82bc9' and first == 5516077:
                    body = next((o['children'] for o in n['children'] if o.get('letter') == 'e'))
                    for index, row in enumerate(foreach_rows(n)):
                        visit(body, proc, [scalar(x, args) for x in row], path + [f"{n['offset']}:{index}"])
                else:
                    result.append({'procedure': proc, 'offset': n['offset'], 'path': path, 'dispatch': n['code'], 'arguments': [scalar(x, args) for x in n.get('arguments', [])], 'options': {o['code']: [scalar(x, args) for x in o['children']] for o in n['children'] if o['kind'] == 'option'}})
            elif kind != 'terminator':
                raise ValueError('unresolved top-level script operation')
    visit(procedures[entry]['nodes'], entry, [], [entry])
    return result
