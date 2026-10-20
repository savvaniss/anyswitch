# Running a title you own

A run on a real title is the most useful test there is, and most runs end early. A run that stops at the first unimplemented function, reported completely, is as valuable as one that gets far. This page says how to run a title so the result can be compared with other people's, and what to report.

Only titles you own, from an `exefs` folder you already have in plain form (AGENTS.md L1, L2). Nothing from the title is ever shared: not files, not its text, not screenshots.

## Before a run

- Build `main`, or the pull request you are testing, in a clean worktree: `tools/dev/build.sh`.
- Check what the runtime still lacks for this title, and keep only the summary lines:

  ```bash
  build/host/anyswitch-relink audit --registry build/runtime/libnxrt.so /path/to/exefs
  ```

- Convert and add the runtime:

  ```bash
  build/host/anyswitch-relink exefs /path/to/exefs -o out --registry build/runtime/libnxrt.so
  cp build/runtime/libnxrt.so out/lib/
  ```

- Run one title at a time.

## Host

- Native: AArch64 Linux with 4 KiB pages. Check with `getconf PAGESIZE`; it must print `4096`. Kernels with 16 KiB or 64 KiB pages convert fine but cannot run the output yet ([debt](debt/relinker.md)).
- x86-64 Linux or WSL: the output runs under qemu-user (`$QEMU_AARCH64 -L $AARCH64_SYSROOT out/main`). It is much slower; say so in the report.

## Launch

```bash
NXRT_LOG=1 timeout 600 ./out/main > run.log 2>&1; echo "exit=$?"
```

- `NXRT_LOG=1` logs start-up, every module and every SVC with an `[nxrt]` prefix.
- `NXRT_NO_CRASH_HANDLER=1` leaves crashes to a debugger: run under `gdb` (natively) or `qemu-aarch64 -g 1234` plus `gdb-multiarch`.
- The title's own output is mixed into `run.log`. It is game text: never paste it. Extract the runtime's lines with `grep '^\[nxrt\]' run.log`.

## What to read

| Check | Where | Meaning |
|---|---|---|
| Exit status | shell | 0 normal exit; 86 unimplemented function or SVC; 87 guest crash or abort; 1 runtime set-up failure; 124 the `timeout` cap; 127 the loader could not find a library |
| `[nxrt] fatal:` | `run.log` | The function, SVC or condition that stopped the run |
| `[nxrt] guest crash:` | `run.log` | Signal, fault address, and the location as `module+offset` |
| Loader errors | `run.log` | `symbol lookup error: undefined symbol: nx$...` means a data import the runtime lacks |
| Last SVCs | `[nxrt] svc` lines | What the title asked the kernel for just before stopping |

A converted file keeps the title's symbol names in `.symtab`, so a disassembler or `gdb` shows the function at `module+offset`. Those names come from the title: use them to investigate, but do not post them unless they are SDK functions (`nn::...`, C library names).

## Reporting a result

Post in the "Title bring-up" category of [Discussions](https://github.com/savvaniss/anyswitch/discussions), one thread per title. Include, in this order:

1. The build: the `main` commit, each pull request applied on top with its head commit, and any local change.
2. The title's public name and version.
3. Host: CPU, OS and kernel, page size, native or qemu-user.
4. The exact command and every `NXRT_*` variable set.
5. How far it got and what stopped it: the exit status and the `[nxrt]` lines only.
6. The audit summary lines.
7. How many runs and how long each lasted. One run is an observation, not a reproduction.
8. The date of the test.

Leave a field empty rather than filling it from someone else's report. Keep AI-generated analysis separate from what you measured, and say which is which.
