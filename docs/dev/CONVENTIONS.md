# Conventions

`tools/check_conventions.py` enforces the parts of this page it can; each finding names its rule.

## Files

- Every source file starts with `// SPDX-License-Identifier: MIT` (or the `#` / `/* */` form for scripts, CMake, linker and CSS files). Rule `spdx`.
- Text files are UTF-8 with LF line endings. Rules `binary`, `line-endings`.
- No images, binaries or files over 2 MiB in the repository. The Pages site generates its images at build time. Rules `binary`, `large-file`.
- C++: `.cpp` / `.hpp`, four-space indent, 110 columns, `.clang-format` at the root.
- Python: standard library only, run with `python3 -I`, type hints, `from __future__ import annotations`.
- Shell: `#!/usr/bin/env bash`, `set -euo pipefail`, clean under `shellcheck`.

## Naming

| Thing | Style | Example |
|---|---|---|
| Namespaces | lower case | `nx`, `nx::a64`, `nxrt` |
| Types | `PascalCase` | `PatchSite`, `LoadedModule` |
| Functions and variables | `snake_case` | `find_patch_sites`, `trap_names` |
| Constants | `kPascalCase` | `kSegAlign`, `kExitUnimplemented` |
| ABI macros | `NXABI_UPPER_CASE` | `NXABI_TLS_TP_OFFSET` |
| Guest export C names | `<module>_<Function>` | `os_LockMutex`, `libc_malloc` |
| Environment variables | `NXRT_*` (runtime), `ANYSWITCH_*` (tooling and build) | `NXRT_LOG` |

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

- The third argument is the exact symbol the title imports. Get it from a compiler, not by hand: declare the prototype in `tests/corpus/include` and call it from a corpus case (AGENTS.md E4).
- Each symbol is exported once. Rule `duplicate-export`.
- An export whose body only returns a constant is a stub: it calls `NX_UNIMPLEMENTED`, or it is listed under "Silent stubs" in its debt file. Rule `silent-stub`.
- Trivially copyable 8-byte classes such as `nn::TimeSpan` and `nn::os::Tick` are passed and returned in `x0`; mirror them with plain structs from `nxrt/guest.hpp`.
- Guest-owned objects (`ThreadType`, `MutexType`, ...) are opaque. Keep runtime state in a side table keyed by their address unless a public source documents the layout.
- One folder per SDK area under `runtime/src/`, matching a debt file `docs/dev/debt/<folder>.md`. Rule `debt-file`.

## Comments

Comments carry the public source of a layout, constant or behaviour (AGENTS.md L4), or a non-obvious reason. They never narrate the code or the change history. This differs from AnyPS5, which forbids comments: anyswitch needs the source next to the code to keep its clean-room record.

## Documentation

- Text is dry, strict and concise. State facts; do not speculate.
- Known gaps go in `docs/dev/debt/<module>.md` (see its README for the entry format), not in new files.
- Relative links must point at files that exist. Rule `doc-link`.
- New Markdown files go only in `docs/`, `docs/dev/`, `docs/dev/debt/`, `docs/user/` or `.github/`. Notes, logs and investigation write-ups go in the pull request. Rule `notes-file`.
- Every `NXRT_*` / `ANYSWITCH_*` variable or option has a row in [ENVIRONMENT.md](ENVIRONMENT.md). Rule `env-reference`.

## Commits

[Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/) for every commit subject and pull request title: `type(scope): summary`, with type one of `feat`, `fix`, `docs`, `style`, `refactor`, `perf`, `test`, `build`, `ci`, `chore`, `revert`, and the module as scope: `feat(nn_os): ...`, `fix(relinker): ...`, `test(corpus): ...`, `docs(agents): ...`. One logical change per commit. Rules `commit-subject`, `pr-title`.
