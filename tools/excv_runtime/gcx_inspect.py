# Generated converter-only source; historical analysis entry points omitted.
"""Bounded structural GCX inspection, not a VM; retain exact decrypted-file offsets.
Lengths/layout checked against PPU D6928/D69C0/DEC10, scalar tags against D6D90.
No condition evaluation; nested branches are potential dependencies, not execution traces.
"""
from pathlib import Path
import json, collections
ROOT = Path(__file__).resolve().parents[1]

class GcxReader:

    def __init__(self, data):
        self.data = data

    def take(self, p, n, end):
        if p < 0 or n < 0 or p + n > end or (end > len(self.data)):
            raise ValueError(f'truncated at {p:#x} size {n} bound {end:#x}')
        return self.data[p:p + n]

    def uint(self, p, n, end):
        return int.from_bytes(self.take(p, n, end), 'little')

    def block(self, p, end):
        n = self.uint(p, 1, end) & 15
        p += 1
        if n >= 13:
            size = n - 12
            n = self.uint(p, size, end)
            p += size
        self.take(p, n, end)
        return (p, p + n)

    def seq(self, p, end, depth=0):
        if depth > 64:
            raise ValueError('nesting limit')
        nodes = []
        while p < end:
            n, p = self.token(p, end, depth)
            nodes.append(n)
        return nodes

    def token(self, p, end, depth):
        if depth > 64:
            raise ValueError('nesting limit')
        start = p
        t = self.uint(p, 1, end)
        tag = t & 240
        n = {'offset': hex(p), 'tag': hex(t)}
        if tag in [48, 80, 96, 112, 128]:
            body, finish = self.block(p, end)
            p = body
            n['kind'] = {48: 'expression', 80: 'option', 96: 'command', 112: 'proc_call', 128: 'block'}[tag]
            if tag == 96:
                n['code'] = hex(self.uint(p, 3, finish))
                p += 3
                first = self.uint(p, 1, finish)
                p += 1
                size = first
                if first & 128:
                    size = (first & 127) << 8 | self.uint(p, 1, finish)
                    p += 1
                self.take(p, size, finish)
                n['arguments'] = self.seq(p, p + size, depth + 1)
                p += size
            elif tag == 80:
                n['letter'] = chr(self.uint(p, 1, finish))
                n['code'] = hex(self.uint(p + 1, 3, finish))
                p += 4
            elif tag == 112:
                n['procedure_id'] = self.uint(p, 2, finish)
                p += 2
            n['children'] = self.seq(p, finish, depth + 1)
            p = finish
        elif tag in [16, 32]:
            raw = self.take(p, 4, end)
            p += 4
            n.update(kind='variable', descriptor=raw.hex())
            if tag == 32:
                a, p = self.token(p, end, depth + 1)
                b, p = self.token(p, end, depth + 1)
                n['indices'] = [a, b]
        elif tag == 64:
            value = t & 15
            p += 1
            if value == 15:
                value += self.uint(p, 1, end)
                p += 1
            n.update(kind='argument', value=value)
        elif tag == 144:
            n.update(kind='local_argument', value=t & 15)
            p += 1
        elif tag in [160, 176]:
            n.update(kind='operator', value=t & 31)
            p += 1
        elif tag >= 192:
            n.update(kind='integer', value=(t & 63) - 1)
            p += 1
        else:
            p += 1
            if t == 0:
                n['kind'] = 'terminator'
            elif t in [1, 2, 3, 4, 6, 8, 9, 10, 14]:
                count = {1: 2, 2: 1, 3: 1, 4: 1, 6: 3, 8: 2, 9: 4, 10: 4, 14: 2}[t]
                raw = self.take(p, count, end)
                p += count
                n['kind'] = 'hash' if t == 6 else 'resource' if t == 14 else 'integer'
                n['value'] = int.from_bytes(raw, 'little', signed=t in [1, 9])
            elif t == 7:
                size = self.uint(p, 1, end)
                p += 1
                raw = self.take(p, size, end)
                p += size
                if not raw or raw[-1] != 0:
                    raise ValueError(f'invalid string at {start:#x}')
                n.update(kind='string', value=raw[:-1].decode('utf-8', errors='replace'), raw=raw.hex())
            elif t == 13:
                n.update(kind='array', code=hex(self.uint(p, 3, end)), count=self.uint(p + 3, 1, end))
                p += 4
            else:
                raise ValueError(f'unsupported scalar {t:#x} at {start:#x}')
        n['end'] = hex(p)
        return (n, p)

def walk(n):
    yield n
    for key in ['arguments', 'children', 'indices']:
        for child in n.get(key, []):
            yield from walk(child)
