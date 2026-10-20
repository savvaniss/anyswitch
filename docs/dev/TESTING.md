# Testing

| Layer | Where | Runs on | What it proves |
|---|---|---|---|
| Unit tests | `tests/unit` | any host | AArch64 encoders against known encodings, LZ4 and SHA-256, NSO/NRO parsing of an in-memory synthetic module, relinker output structure (segments, symbols, relocations, stubs, traps) |
| Synthetic corpus | `tests/corpus` | AArch64 Linux or qemu-user | real NSO/NRO modules built from source are relinked and executed against `libnxrt.so`; output, exit status and stderr are checked |
| Conventions | `tools/check_conventions.py` | any host | licensing headers, forbidden material, debt files, unique guest exports, ABI version bumps |

```bash
tools/dev/test.sh                    # everything
tools/dev/test.sh build sysregs svc  # selected corpus cases
build/host/nx_unit_tests relink      # unit tests whose name contains "relink"
python3 -I tools/check_conventions.py
```

## The synthetic corpus

The corpus is the only kind of guest code allowed in the repository (AGENTS.md rule L2). Each case is a small freestanding C++ program compiled for `aarch64-freestanding-none`, linked with `tests/corpus/rt/module.ld` into the Switch module layout (text at 0, page-aligned rodata and data, MOD0, SysV hash, RELA and PLT relocations), and written as NSO (LZ4-compressed, hashed) or NRO by `tools/corpus/nxo.py`. Guest code calls the runtime through the clean-room prototypes in `tests/corpus/include`, so every mangled name is produced by a real compiler.

Current cases:

| Case | Covers |
|---|---|
| `hello` | NRO input, static constructors and destructors, guarded statics, argc/argv, libc, `operator new[]`/`delete[]` |
| `sysregs` | every patch class: TLR reads, guest `tpidr_el0` read/write, counters into `x0` and other registers, `cntfrq_el0`, exclusives |
| `svc` | raw SVCs: OutputDebugString, GetSystemTick, SleepThread, GetInfo (valid and invalid), ExitProcess, NZCV preserved across a call |
| `threads` | `nn::os` threads (both CreateThread overloads), mutexes, per-thread TLRs, contended atomics |
| `missing` | an unimplemented import stops with exit 86 and the demangled name |
| `abi_surface` | references every runtime export through a real prototype; spot-checks forwarding of arguments and results |
| `multi` | `main` importing from `subsdk0`, initialiser order across modules |

AnyPS5 forbids tests that only check that a symbol is exported, because a missing export fails at start-up there. anyswitch binds missing functions to traps that fire only when called, so a mistyped mangled name would surface late, in a title; `abi_surface` exists to catch that at build time.

Each case must finish within 30 s on CI runners, under qemu-user included (AGENTS.md E8). The runner prints each case's time and warns above 30 s.

After a full run, the runner compares the runtime's exports (`anyswitch-relink provided`) with the imports of every corpus module. `tools/dev/test.sh` passes `--require-coverage`, so an export that no case references fails the run (AGENTS.md rule E4). When you add an export, reference it from `abi_surface.cpp` at least, and exercise its behaviour in a case of its own when it has any.

### Adding a case

1. Add `tests/corpus/src/<name>.cpp` with an `extern "C" void nnMain()`.
2. Declare any new SDK function in `tests/corpus/include` (clean-room prototype, with a source comment).
3. Add an entry to `tests/corpus/cases.json`: modules, format, args, expected exit status, ordered stdout patterns, optional `stdout_absent` and `stderr` patterns.
4. Run `tools/dev/test.sh build <name>`.

### Hardware ground truth

When a case pins down console behaviour (a result code, an ordering, a timing), run the same program as homebrew on hardware you own and note "measured on hardware" in the case's source comment. Never commit anything produced by a commercial title.
