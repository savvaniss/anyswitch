# Conventions

## Files

- Every source file starts with `// SPDX-License-Identifier: MIT` (or the `#` / `/* */` form for scripts and linker files).
- C++: `.cpp` / `.hpp`, four-space indent, 110 columns, `.clang-format` at the root.
- Python: standard library only, run with `python3 -I`, type hints, `from __future__ import annotations`.
- Shell: `#!/usr/bin/env bash`, `set -euo pipefail`, LF line endings.

## Naming

| Thing | Style | Example |
|---|---|---|
| Namespaces | lower case | `nx`, `nx::a64`, `nxrt` |
| Types | `PascalCase` | `PatchSite`, `LoadedModule` |
| Functions and variables | `snake_case` | `find_patch_sites`, `trap_names` |
| Constants | `kPascalCase` | `kSegAlign`, `kExitUnimplemented` |
| ABI macros | `NXABI_UPPER_CASE` | `NXABI_TLS_TP_OFFSET` |
| Guest export C names | `<module>_<Function>` | `os_LockMutex`, `libc_malloc` |

## Errors

- Host tools throw `nx::Error` through `nx::fail("{}...", args)` with a message that names the file and offset. `main` prints it and exits with status 2.
- The runtime never throws across the guest boundary. It reports with `nxrt::fatal(exit_code, ...)` or `NX_UNIMPLEMENTED("...")`.
- Exit statuses: 86 unimplemented guest path, 87 guest crash or abort, 1 runtime set-up failure.

## Guest exports

```cpp
// Source: <switchbrew page or other public reference>
NX_GUEST_FN(void, os_SleepThread, "_ZN2nn2os11SleepThreadENS_8TimeSpanE", (TimeSpan span)) {
    ...
}
```

- The third argument is the exact symbol the title imports. Get it from a compiler, not by hand: declare the prototype in `tests/corpus/include` and call it from a corpus case (AGENTS.md rule E4).
- Trivially copyable 8-byte classes such as `nn::TimeSpan` and `nn::os::Tick` are passed and returned in `x0`; mirror them with plain structs from `nxrt/guest.hpp`.
- Guest-owned objects (`ThreadType`, `MutexType`, ...) are opaque. Keep runtime state in a side table keyed by their address unless a public source documents the layout.
- One folder per SDK area under `runtime/src/`, matching a debt file `docs/dev/debt/<folder>.md`.

## Comments

Write comments for sources and for reasons. Do not narrate the code or the change history.

## Commits

Conventional Commits, module as scope: `feat(nn_os): ...`, `fix(relinker): ...`, `test(corpus): ...`, `docs(agents): ...`. One logical change per commit.
