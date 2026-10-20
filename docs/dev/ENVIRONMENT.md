# Environment variables and build options

Every `NXRT_*` and `ANYSWITCH_*` name the code reads has a row here, and every row is still read by the code. `tools/check_conventions.py` enforces both directions (rule `env-reference`). Add or remove the row in the same pull request as the code.

## Runtime (`libnxrt.so`)

| Name | Values | Effect |
|---|---|---|
| `NXRT_LOG` | `1` to enable | Logs module discovery, the tick scale, start-up steps and every SVC to stderr, prefixed `[nxrt]`. |
| `NXRT_NO_CRASH_HANDLER` | `1` to enable | Leaves crash signals to the default handler or a debugger instead of printing the `module+offset` report. |

## Build options (CMake)

| Name | Default | Effect |
|---|---|---|
| `ANYSWITCH_WERROR` | `OFF` | Host tools: treat compiler warnings as errors. CI and `tools/dev/build.sh` turn it on. |
| `ANYSWITCH_BUILD_TESTS` | `ON` | Host tools: build the unit tests. |
| `NXRT_WERROR` | `OFF` | Runtime: treat compiler warnings as errors. `tools/dev/build.sh` turns it on. |

## Tooling

| Name | Read by | Effect |
|---|---|---|
| `ANYSWITCH_TOOLCHAIN` | `tools/bootstrap/fetch_toolchain.sh`, `tools/dev/*.sh` | Folder for Zig, qemu-user and the AArch64 sysroot. Default `~/.cache/anyswitch/toolchain`. |

These names are outside the `NXRT_`/`ANYSWITCH_` prefixes, so the checker does not track them, but the tools read them:

| Name | Read by | Effect |
|---|---|---|
| `ZIG` | `cmake/zig/*`, `tools/corpus/run_corpus.py`, `tools/dev/build.sh` | Path of the zig binary. Set by the toolchain's `env.sh`. |
| `ZIG_TARGET` | `cmake/zig/zig-cc`, `cmake/zig/zig-c++` | Runtime target triple. Default `aarch64-linux-gnu.2.28`. |
| `ZIG_VERSION` | `tools/bootstrap/fetch_toolchain.sh` | Zig release to fetch. Default `0.17.0`. |
| `QEMU_AARCH64` | `tools/bootstrap/fetch_toolchain.sh`, `tools/corpus/run_corpus.py` | qemu-user binary for AArch64 on non-AArch64 hosts. |
| `AARCH64_SYSROOT` | `tools/bootstrap/fetch_toolchain.sh`, `tools/corpus/run_corpus.py` | AArch64 glibc root passed to qemu with `-L`. |
| `PR_TITLE`, `PR_BODY`, `PR_AUTHOR` | `tools/check_conventions.py --pr` | Pull request fields, set by the Conventions workflow. |
