# Third-party provenance

Every piece of third-party code or data in this repository is listed here: what it is, where it came from, the exact commit or release, its license, and how it is used (see the modes in [../docs/REUSE.md](../docs/REUSE.md)). Third-party code enters only as a git submodule in this folder (AGENTS.md rule L5).

| Path | Upstream | Version | License | Mode | Notes |
|---|---|---|---|---|---|
| (none yet) | | | | | |

## Tools used at build or test time (not redistributed)

| Tool | Source | Version | License | Use |
|---|---|---|---|---|
| Zig | https://ziglang.org/download/ | 0.17.0 (checksum-verified by `tools/bootstrap/fetch_toolchain.sh`) | MIT | AArch64 cross compiler and linker for the runtime and the corpus |
| qemu-user | distribution archive (`apt-get download qemu-user`) | as packaged | GPL-2.0 | runs AArch64 test binaries on x86-64 hosts, as a separate process |
| glibc (AArch64) | distribution archive (`libc6-arm64-cross`) | as packaged | LGPL-2.1 | sysroot for qemu-user |

## Formats implemented from public specifications

| Item | Specification |
|---|---|
| LZ4 block decoder and encoder (`core/format/src/lz4.cpp`, `tools/corpus/nxo.py`) | https://github.com/lz4/lz4/blob/dev/doc/lz4_Block_format.md |
| SHA-256 (`core/common/src/sha256.cpp`) | FIPS 180-4 |
| ELF64 / AArch64 definitions (`core/format/include/nx/format/elf.hpp`) | System V gABI, "ELF for the Arm 64-bit Architecture" |
| NSO, NRO, MOD0, NPDM, SVC, Thread Local Region | https://switchbrew.org (pages cited in the source files) |
