// SPDX-License-Identifier: MIT
// ELF64 definitions for AArch64, written from the System V gABI and the
// "ELF for the Arm 64-bit Architecture" ABI document. Kept local so the tools
// build on hosts without <elf.h>.
#pragma once

#include <cstdint>

namespace nx::elf {

struct Ehdr {
    std::uint8_t ident[16];
    std::uint16_t type;
    std::uint16_t machine;
    std::uint32_t version;
    std::uint64_t entry;
    std::uint64_t phoff;
    std::uint64_t shoff;
    std::uint32_t flags;
    std::uint16_t ehsize;
    std::uint16_t phentsize;
    std::uint16_t phnum;
    std::uint16_t shentsize;
    std::uint16_t shnum;
    std::uint16_t shstrndx;
};

struct Phdr {
    std::uint32_t type;
    std::uint32_t flags;
    std::uint64_t offset;
    std::uint64_t vaddr;
    std::uint64_t paddr;
    std::uint64_t filesz;
    std::uint64_t memsz;
    std::uint64_t align;
};

struct Shdr {
    std::uint32_t name;
    std::uint32_t type;
    std::uint64_t flags;
    std::uint64_t addr;
    std::uint64_t offset;
    std::uint64_t size;
    std::uint32_t link;
    std::uint32_t info;
    std::uint64_t addralign;
    std::uint64_t entsize;
};

struct Sym {
    std::uint32_t name;
    std::uint8_t info;
    std::uint8_t other;
    std::uint16_t shndx;
    std::uint64_t value;
    std::uint64_t size;
};

struct Rela {
    std::uint64_t offset;
    std::uint64_t info;
    std::int64_t addend;
};

struct Rel {
    std::uint64_t offset;
    std::uint64_t info;
};

struct Dyn {
    std::int64_t tag;
    std::uint64_t val;
};

static_assert(sizeof(Ehdr) == 64);
static_assert(sizeof(Phdr) == 56);
static_assert(sizeof(Shdr) == 64);
static_assert(sizeof(Sym) == 24);
static_assert(sizeof(Rela) == 24);
static_assert(sizeof(Rel) == 16);
static_assert(sizeof(Dyn) == 16);

constexpr std::uint16_t ET_EXEC = 2;
constexpr std::uint16_t ET_DYN = 3;
constexpr std::uint16_t EM_AARCH64 = 183;

constexpr std::uint32_t PT_NULL = 0;
constexpr std::uint32_t PT_LOAD = 1;
constexpr std::uint32_t PT_DYNAMIC = 2;
constexpr std::uint32_t PT_INTERP = 3;
constexpr std::uint32_t PT_NOTE = 4;
constexpr std::uint32_t PT_PHDR = 6;
constexpr std::uint32_t PT_TLS = 7;
constexpr std::uint32_t PT_GNU_EH_FRAME = 0x6474e550;
constexpr std::uint32_t PT_GNU_STACK = 0x6474e551;
constexpr std::uint32_t PT_GNU_RELRO = 0x6474e552;

constexpr std::uint32_t PF_X = 1;
constexpr std::uint32_t PF_W = 2;
constexpr std::uint32_t PF_R = 4;

constexpr std::uint32_t SHT_NULL = 0;
constexpr std::uint32_t SHT_PROGBITS = 1;
constexpr std::uint32_t SHT_SYMTAB = 2;
constexpr std::uint32_t SHT_STRTAB = 3;
constexpr std::uint32_t SHT_RELA = 4;
constexpr std::uint32_t SHT_HASH = 5;
constexpr std::uint32_t SHT_DYNAMIC = 6;
constexpr std::uint32_t SHT_NOTE = 7;
constexpr std::uint32_t SHT_NOBITS = 8;
constexpr std::uint32_t SHT_DYNSYM = 11;

constexpr std::uint64_t SHF_WRITE = 1;
constexpr std::uint64_t SHF_ALLOC = 2;
constexpr std::uint64_t SHF_EXECINSTR = 4;
constexpr std::uint64_t SHF_TLS = 0x400;

constexpr std::uint16_t SHN_UNDEF = 0;
constexpr std::uint16_t SHN_ABS = 0xfff1;

constexpr std::uint8_t STB_LOCAL = 0;
constexpr std::uint8_t STB_GLOBAL = 1;
constexpr std::uint8_t STB_WEAK = 2;
constexpr std::uint8_t STT_NOTYPE = 0;
constexpr std::uint8_t STT_OBJECT = 1;
constexpr std::uint8_t STT_FUNC = 2;
constexpr std::uint8_t STT_SECTION = 3;
constexpr std::uint8_t STT_TLS = 6;

constexpr std::uint8_t st_bind(std::uint8_t info) { return info >> 4; }
constexpr std::uint8_t st_type(std::uint8_t info) { return info & 0xf; }
constexpr std::uint8_t st_info(std::uint8_t bind, std::uint8_t type) {
    return static_cast<std::uint8_t>((bind << 4) | (type & 0xf));
}

constexpr std::int64_t DT_NULL = 0;
constexpr std::int64_t DT_NEEDED = 1;
constexpr std::int64_t DT_PLTRELSZ = 2;
constexpr std::int64_t DT_PLTGOT = 3;
constexpr std::int64_t DT_HASH = 4;
constexpr std::int64_t DT_STRTAB = 5;
constexpr std::int64_t DT_SYMTAB = 6;
constexpr std::int64_t DT_RELA = 7;
constexpr std::int64_t DT_RELASZ = 8;
constexpr std::int64_t DT_RELAENT = 9;
constexpr std::int64_t DT_STRSZ = 10;
constexpr std::int64_t DT_SYMENT = 11;
constexpr std::int64_t DT_INIT = 12;
constexpr std::int64_t DT_FINI = 13;
constexpr std::int64_t DT_SONAME = 14;
constexpr std::int64_t DT_RPATH = 15;
constexpr std::int64_t DT_SYMBOLIC = 16;
constexpr std::int64_t DT_REL = 17;
constexpr std::int64_t DT_RELSZ = 18;
constexpr std::int64_t DT_RELENT = 19;
constexpr std::int64_t DT_PLTREL = 20;
constexpr std::int64_t DT_DEBUG = 21;
constexpr std::int64_t DT_TEXTREL = 22;
constexpr std::int64_t DT_JMPREL = 23;
constexpr std::int64_t DT_BIND_NOW = 24;
constexpr std::int64_t DT_INIT_ARRAY = 25;
constexpr std::int64_t DT_FINI_ARRAY = 26;
constexpr std::int64_t DT_INIT_ARRAYSZ = 27;
constexpr std::int64_t DT_FINI_ARRAYSZ = 28;
constexpr std::int64_t DT_RUNPATH = 29;
constexpr std::int64_t DT_FLAGS = 30;
constexpr std::int64_t DT_PREINIT_ARRAY = 32;
constexpr std::int64_t DT_PREINIT_ARRAYSZ = 33;
constexpr std::int64_t DT_RELRSZ = 35;
constexpr std::int64_t DT_RELR = 36;
constexpr std::int64_t DT_RELRENT = 37;
constexpr std::int64_t DT_GNU_HASH = 0x6ffffef5;
constexpr std::int64_t DT_RELACOUNT = 0x6ffffff9;
constexpr std::int64_t DT_RELCOUNT = 0x6ffffffa;
constexpr std::int64_t DT_FLAGS_1 = 0x6ffffffb;

constexpr std::uint64_t DF_TEXTREL = 0x4;
constexpr std::uint64_t DF_BIND_NOW = 0x8;
constexpr std::uint64_t DF_1_NOW = 0x1;
constexpr std::uint64_t DF_1_PIE = 0x08000000;

constexpr std::uint32_t R_AARCH64_NONE = 0;
constexpr std::uint32_t R_AARCH64_ABS64 = 257;
constexpr std::uint32_t R_AARCH64_ABS32 = 258;
constexpr std::uint32_t R_AARCH64_COPY = 1024;
constexpr std::uint32_t R_AARCH64_GLOB_DAT = 1025;
constexpr std::uint32_t R_AARCH64_JUMP_SLOT = 1026;
constexpr std::uint32_t R_AARCH64_RELATIVE = 1027;
constexpr std::uint32_t R_AARCH64_TLS_DTPMOD64 = 1028;
constexpr std::uint32_t R_AARCH64_TLS_DTPREL64 = 1029;
constexpr std::uint32_t R_AARCH64_TLS_TPREL64 = 1030;
constexpr std::uint32_t R_AARCH64_TLSDESC = 1031;
constexpr std::uint32_t R_AARCH64_IRELATIVE = 1032;

constexpr std::uint32_t r_sym(std::uint64_t info) { return static_cast<std::uint32_t>(info >> 32); }
constexpr std::uint32_t r_type(std::uint64_t info) { return static_cast<std::uint32_t>(info); }
constexpr std::uint64_t r_info(std::uint32_t sym, std::uint32_t type) {
    return (static_cast<std::uint64_t>(sym) << 32) | type;
}

const char* reloc_name(std::uint32_t type);

// SysV ELF hash used by DT_HASH.
std::uint32_t sysv_hash(const char* name);

} // namespace nx::elf
