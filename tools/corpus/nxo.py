# SPDX-License-Identifier: MIT
"""Writes NSO, NRO and NPDM files from ELF modules linked with
tests/corpus/rt/module.ld. Only used for the synthetic test corpus.

Layouts: https://switchbrew.org/wiki/NSO, /wiki/NRO, /wiki/NPDM.
"""
from __future__ import annotations

import hashlib
import struct
from dataclasses import dataclass

PT_LOAD = 1


@dataclass
class Segment:
    vaddr: int
    data: bytes
    memsz: int


@dataclass
class ElfModule:
    text: Segment
    ro: Segment
    data: Segment
    build_id: bytes
    dynstr: tuple[int, int]  # (vaddr, size)
    dynsym: tuple[int, int]


def read_elf(blob: bytes) -> ElfModule:
    if blob[:4] != b"\x7fELF" or blob[4] != 2 or blob[5] != 1:
        raise ValueError("not a little-endian ELF64 file")
    (phoff, shoff) = struct.unpack_from("<QQ", blob, 0x20)
    (phentsize, phnum, shentsize, shnum, shstrndx) = struct.unpack_from("<HHHHH", blob, 0x36)
    loads = []
    for i in range(phnum):
        p_type, _flags, off, vaddr, _paddr, filesz, memsz, _align = struct.unpack_from(
            "<IIQQQQQQ", blob, phoff + i * phentsize)
        if p_type == PT_LOAD:
            loads.append(Segment(vaddr, blob[off:off + filesz], memsz))
    if len(loads) != 3:
        raise ValueError(f"expected 3 PT_LOAD segments (text, ro, data), found {len(loads)}")
    sections = []
    for i in range(shnum):
        sections.append(struct.unpack_from("<IIQQQQIIQQ", blob, shoff + i * shentsize))
    names_off = sections[shstrndx][4]

    def name(sec):
        start = names_off + sec[0]
        return blob[start:blob.index(b"\0", start)].decode()

    by_name = {name(s): s for s in sections}
    build_id = b""
    note = by_name.get(".note.gnu.build-id")
    if note:
        off = note[4]
        namesz, descsz, _ = struct.unpack_from("<III", blob, off)
        desc = off + 12 + ((namesz + 3) & ~3)
        build_id = blob[desc:desc + descsz]
    dynstr = by_name[".dynstr"]
    dynsym = by_name[".dynsym"]
    return ElfModule(loads[0], loads[1], loads[2], build_id[:32],
                     (dynstr[3], dynstr[5]), (dynsym[3], dynsym[5]))


def lz4_compress(src: bytes) -> bytes:
    """Greedy LZ4 block encoder (mirrors core/format/src/lz4.cpp)."""
    out = bytearray()
    n = len(src)
    anchor = 0

    def put_length(extra: int) -> None:
        while extra >= 255:
            out.append(255)
            extra -= 255
        out.append(extra)

    def emit(literals: int, offset: int, match: int) -> None:
        token_at = len(out)
        out.append(0)
        token = 0xF0 if literals >= 15 else literals << 4
        if literals >= 15:
            put_length(literals - 15)
        out.extend(src[anchor:anchor + literals])
        if match:
            out.extend(struct.pack("<H", offset))
            extra = match - 4
            if extra >= 15:
                token |= 15
                put_length(extra - 15)
            else:
                token |= extra
        out[token_at] = token

    if n > 12:
        table: dict[bytes, int] = {}
        limit = n - 12
        match_end_limit = n - 5
        ip = 0
        while ip < limit:
            key = src[ip:ip + 4]
            ref = table.get(key)
            table[key] = ip
            if ref is not None and ip - ref <= 65535:
                match = 4
                while ip + match < match_end_limit and src[ref + match] == src[ip + match]:
                    match += 1
                emit(ip - anchor, ip - ref, match)
                ip += match
                anchor = ip
            else:
                ip += 1
    emit(n - anchor, 0, 0)
    return bytes(out)


def write_nso(m: ElfModule, compress: bool = True) -> bytes:
    header = bytearray(0x100)
    header[0:4] = b"NSO0"
    flags = 0x38 | (0x07 if compress else 0)
    struct.pack_into("<I", header, 0x0C, flags)
    body = bytearray()
    for i, seg in enumerate((m.text, m.ro, m.data)):
        plain = seg.data
        stored = lz4_compress(plain) if compress else plain
        file_offset = 0x100 + len(body)
        struct.pack_into("<III", header, 0x10 + 0x10 * i, file_offset, seg.vaddr, len(plain))
        struct.pack_into("<I", header, 0x60 + 4 * i, len(stored))
        header[0xA0 + 0x20 * i:0xC0 + 0x20 * i] = hashlib.sha256(plain).digest()
        body += stored
    bss = m.data.memsz - len(m.data.data)
    struct.pack_into("<I", header, 0x3C, bss)
    header[0x40:0x40 + len(m.build_id)] = m.build_id
    struct.pack_into("<IIII", header, 0x90,
                     m.dynstr[0] - m.ro.vaddr, m.dynstr[1], m.dynsym[0] - m.ro.vaddr, m.dynsym[1])
    return bytes(header + body)


def write_nro(m: ElfModule) -> bytes:
    image = bytearray(m.data.vaddr + len(m.data.data))
    for seg in (m.text, m.ro, m.data):
        image[seg.vaddr:seg.vaddr + len(seg.data)] = seg.data
    image[0x10:0x14] = b"NRO0"
    struct.pack_into("<III", image, 0x14, 0, len(image), 0)
    struct.pack_into("<IIIIIII", image, 0x20, m.text.vaddr, len(m.text.data), m.ro.vaddr, len(m.ro.data),
                     m.data.vaddr, len(m.data.data), m.data.memsz - len(m.data.data))
    image[0x40:0x40 + len(m.build_id)] = m.build_id
    struct.pack_into("<IIII", image, 0x70,
                     m.dynstr[0] - m.ro.vaddr, m.dynstr[1], m.dynsym[0] - m.ro.vaddr, m.dynsym[1])
    return bytes(image)


def write_npdm(program_id: int, name: str = "Application", stack_size: int = 0x100000) -> bytes:
    meta = bytearray(0x80)
    meta[0:4] = b"META"
    meta[0x0C] = 0x01 | (3 << 1)  # 64-bit code, 39-bit address space
    meta[0x0E] = 44               # main thread priority
    meta[0x0F] = 0                # main thread core
    struct.pack_into("<I", meta, 0x1C, stack_size)
    meta[0x20:0x20 + len(name)] = name.encode()
    struct.pack_into("<IIII", meta, 0x70, 0x80, 0x40, 0xC0, 0)
    aci = bytearray(0x40)
    aci[0:4] = b"ACI0"
    struct.pack_into("<Q", aci, 0x10, program_id)
    return bytes(meta + aci)
