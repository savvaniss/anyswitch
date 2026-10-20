# Building

Two separate builds:

- **Host tools** (`anyswitch-relink`, unit tests): standard C++20, any OS. Built from the repository root.
- **Runtime** (`libnxrt.so`): AArch64 Linux shared library, cross-compiled with Zig from any Linux or macOS host. Built from `runtime/`.

## Quick start (Linux, WSL or macOS)

```bash
tools/bootstrap/fetch_toolchain.sh
tools/dev/build.sh
tools/dev/test.sh
```

`fetch_toolchain.sh` needs no root. It downloads, into `~/.cache/anyswitch/toolchain`:

| Item | Source | Use |
|---|---|---|
| Zig (checksum-verified) | ziglang.org | clang and lld for AArch64, plus glibc link stubs |
| qemu-user | distribution archive via `apt-get download` | runs AArch64 binaries on x86-64 hosts |
| libc6-arm64-cross | distribution archive via `apt-get download` | AArch64 glibc for qemu |

It writes `env.sh` with `ZIG`, `QEMU_AARCH64` and `AARCH64_SYSROOT`; the dev scripts source it automatically. If you already have qemu and a sysroot, export `QEMU_AARCH64` and `AARCH64_SYSROOT` before running the script and it will use them. On an AArch64 Linux host qemu is not needed.

Requirements: CMake 3.24+, a C++20 compiler for the host tools (GCC 13+, Clang 17+, MSVC 2022+), Python 3.10+.

## Host tools only

```bash
cmake -S . -B build/host -DANYSWITCH_WERROR=ON
cmake --build build/host
ctest --test-dir build/host
```

On Windows this works with MSVC (`-G "Visual Studio 17 2022"` or newer) and with MinGW-w64 GCC (`-G Ninja`). MinGW builds link statically.

## Runtime only

```bash
. ~/.cache/anyswitch/toolchain/env.sh
cmake -S runtime -B build/runtime --toolchain cmake/toolchains/aarch64-linux-zig.cmake
cmake --build build/runtime
```

The toolchain file wraps `zig cc`/`zig c++` with target `aarch64-linux-gnu.2.28`, so the library runs on glibc 2.28 and newer. Override with `ZIG_TARGET`.

## Converting a title you own

See [../user/USAGE.md](../user/USAGE.md).
