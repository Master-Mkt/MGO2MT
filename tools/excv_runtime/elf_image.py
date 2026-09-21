# Generated converter-only source; historical analysis entry points omitted.
from pathlib import Path
import struct, hashlib

class ElfImage:

    def __init__(self, path):
        self.path = Path(path)
        self.data = self.path.read_bytes()
        if self.data[:6] != b'\x7fELF\x02\x02':
            raise ValueError('expected BE ELF64')
        self.sha256 = hashlib.sha256(self.data).hexdigest()
        phoff = struct.unpack_from('>Q', self.data, 32)[0]
        phsize, phnum = struct.unpack_from('>HH', self.data, 54)
        self.segments = []
        for i in range(phnum):
            kind, flags, off, va, pa, filesz, memsz, align = struct.unpack_from('>IIQQQQQQ', self.data, phoff + i * phsize)
            if kind == 1:
                self.segments.append((va, off, filesz))

    def read(self, va, size):
        for base, off, n in self.segments:
            if base <= va and va + size <= base + n:
                return self.data[off + va - base:off + va - base + size]
        raise ValueError(hex(va))

    def u32(self, va):
        return int.from_bytes(self.read(va, 4), 'big')

    def find(self, needle):
        for base, off, n in self.segments:
            at = off
            while True:
                at = self.data.find(needle, at, off + n)
                if at < 0:
                    break
                yield (base + at - off)
                at += 1

    def cstring(self, va, limit=128):
        return self.read(va, limit).split(b'\x00')[0].decode('utf-8', errors='replace')
