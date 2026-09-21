# Generated converter-only source; historical analysis entry points omitted.
"""LA2 0x10009 structural reader based on PPU 240048/25FF30/24B700/24AE50.
Does not mutate the asset, evaluate GCX, or assume unknown records are portable.
"""
from pathlib import Path
import json, collections
from title_assets import ROOT, read, take

class Layout:

    def __init__(self, b):
        self.b = b
        header = read(b, 0, '13I')
        self.version = header[0]
        if self.version != 65545:
            raise ValueError('LA2 version not reviewed')
        self.sections = header[1:]
        self.nodes = []
        self.properties = {}
        self.events = []
        for off in self.sections:
            if off:
                take(b, off, 4)
        p = header[1]
        self.counts = read(b, p, '22H')
        p += 44
        if self.counts[0] > 10000:
            raise ValueError('node count')
        for i in range(self.counts[0]):
            name, flags, idx, parent, next_id, aux, n, layer = read(b, p, 'II6H')
            if idx != i + 1 or parent >= idx or next_id > self.counts[0]:
                raise ValueError('node relationship')
            self.nodes.append({'offset': p, 'name': name, 'flags': flags, 'type': flags & 15, 'id': idx, 'parent': parent, 'next_raw': next_id, 'aux': aux, 'layer': layer, 'extra': take(b, p + 20, 4 * n).hex()})
            p += 20 + 4 * n
        if p != header[2]:
            raise ValueError('node end')
        count, = read(b, header[2], 'I')
        p = header[2] + 4
        for i in range(count):
            row = self.property(p)
            self.properties[p] = row
            p += row['size']
            if i < len(self.nodes):
                self.nodes[i]['property'] = row['offset']
        if p != header[3]:
            raise ValueError('property section end')
        self.colors = self.pool(header[8])
        self.uv = self.pool(header[9])
        self.fonts = []
        p = header[11] + 4
        for i in range(read(b, header[11], 'I')[0]):
            key, n, size = read(b, p, 'IHH')
            if size != 8 + 16 * n:
                raise ValueError('font extent')
            glyphs = []
            for j in range(n):
                q = p + 8 + 16 * j
                w, h, u0, u1, v0, v1 = read(b, q, '6H')
                char = take(b, q + 12, 4).split(b'\x00')[0]
                glyphs.append({'width': w, 'height': h, 'uv': [u0, u1, v0, v1], 'char': char.decode('ascii') if char.isascii() else None, 'char_bytes': char.hex()})
            self.fonts.append({'key': key, 'glyphs': glyphs})
            p += size
        if p != header[12]:
            raise ValueError('font section end')
        self.textures = [{'texture': read(b, header[7] + 4 + 8 * i, 'I')[0], 'archive': read(b, header[7] + 8 + 8 * i, 'I')[0]} for i in range(read(b, header[7], 'I')[0])]
        for i in range(read(b, header[4], 'I')[0]):
            name, p = read(b, header[4] + 4 + 8 * i, 'II')
            n, flags, sz, pad = read(b, p, '4H')
            event = {'name': name, 'offset': p, 'flags': flags, 'size': sz, 'tracks': []}
            q = p + 8
            for j in range(n):
                node, aux, extent = read(b, q, 'HHI')
                end = q + extent
                r = q + 8
                commands = []
                if not 0 <= node <= len(self.nodes) or extent < 12:
                    raise ValueError('event track')
                while r < end:
                    op, size = read(b, r, 'HH')
                    if size < 4 or r + size > end:
                        raise ValueError('event command extent')
                    payload = take(b, r + 4, size - 4)
                    commands.append({'offset': r, 'opcode': op, 'data': payload.hex()})
                    r += size
                    if not op:
                        break
                if r != end:
                    raise ValueError('event track end')
                event['tracks'].append({'node': node, 'aux': aux, 'offset': q, 'commands': commands})
                q = end
            if q != p + sz:
                raise ValueError('event end')
            self.events.append(event)

    def property(self, p):
        n, size = read(self.b, p, 'HH')
        end = p + size
        q = p + 4
        commands = []
        take(self.b, p, size)
        while q < end:
            op, sz = read(self.b, q, 'HH')
            if sz < 4 or q + sz > end:
                raise ValueError('property command extent')
            commands.append({'offset': q, 'opcode': op, 'data': take(self.b, q + 4, sz - 4).hex()})
            q += sz
            if not op:
                break
        if q != end or len(commands) != n:
            raise ValueError('property command count/end')
        return {'offset': p, 'size': size, 'commands': commands}

    def pool(self, p):
        count, = read(self.b, p, 'I')
        p += 4
        rows = []
        for i in range(count):
            n, size = read(self.b, p, 'HH')
            rows.append({'offset': p, 'count': n, 'data': take(self.b, p + 4, size - 4).hex()})
            p += size
        return rows

    def json(self):
        return {'version': self.version, 'sections': self.sections, 'counts': self.counts, 'nodes': self.nodes, 'properties': list(self.properties.values()), 'colors': self.colors, 'uv': self.uv, 'fonts': self.fonts, 'textures': self.textures, 'events': self.events}
