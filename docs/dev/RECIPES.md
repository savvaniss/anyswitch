# Recipes

Step-by-step instructions for the tasks that come up most. Each ends with the same check: `tools/dev/test.sh` and `python3 -I tools/check_conventions.py` pass.

## Add a guest function

1. Check it is really missing: `build/host/anyswitch-relink provided --demangle build/runtime/libnxrt.so | grep <Name>`.
2. Find a public source for its behaviour (AGENTS.md rule L4). Note the URL.
3. Add the clean-room prototype to `tests/corpus/include/nn/<area>.hpp` (or `guest/libc.h`), with the source in a comment.
4. Implement it in `runtime/src/<module>/<file>.cpp`:
   ```cpp
   // Source: https://switchbrew.org/wiki/...
   NX_GUEST_FN(Result, os_Foo, "<mangled name>", (Args...)) { ... }
   ```
   To get the mangled name, compile the prototype and read it back:
   `build/host/anyswitch-relink symbols build/corpus/<case>/exefs/main`.
5. Reference it in `tests/corpus/src/abi_surface.cpp` (the coverage check fails otherwise), and call it from a corpus case that checks its behaviour, with the expected output in `tests/corpus/cases.json`.
6. If anything is partial, add a bullet to `docs/dev/debt/<module>.md`.

## Add a new guest API module

1. Create `runtime/src/<module>/` and add `<module>` to `NXRT_MODULES` in `runtime/CMakeLists.txt`.
2. Create `docs/dev/debt/<module>.md` from the template in `docs/dev/debt/README.md`.
3. Add the module's prototypes header under `tests/corpus/include/`.

## Handle another SVC in the safety net

1. Confirm it is not IPC (AGENTS.md rule E3).
2. Add a `case` to `nxrt_svc_dispatch` in `runtime/src/rt/svc.cpp`, with register usage from https://switchbrew.org/wiki/SVC.
3. Exercise it from `tests/corpus/src/svc.cpp` with inline `svc`.

## Add a patch class

1. Add the decoder to `core/relinker/include/nx/relinker/a64.hpp` with a known-encoding unit test in `tests/unit/test_a64.cpp`.
2. Add the `PatchKind`, the scan rule (`patcher.cpp`) and the stub (`writer.cpp`, `emit_stubs`). Stubs must preserve every register and NZCV except the instruction's destination.
3. Add a census category (`census.cpp`).
4. Extend `tests/unit/test_relink.cpp` and `tests/corpus/src/sysregs.cpp`.
5. Document it in the patch table of ARCHITECTURE.md.

## Change the relinker/runtime contract

1. Edit `core/abi/include/nxabi/nxabi.h` and bump `NXABI_VERSION`.
2. Update both sides and the "Relinker/runtime contract" section of ARCHITECTURE.md.
3. `tools/check_conventions.py --base origin/main` fails if the header changed without a version bump.

## Measure your own titles (Phase 0)

These read files you have; they print aggregate numbers only. Do not post per-title output.

```bash
python3 -I tools/phase0/priv_census.py --relink build/host/anyswitch-relink /path/to/exefs ...
python3 -I tools/phase0/symbol_inventory.py --relink build/host/anyswitch-relink \
    --registry build/runtime/libnxrt.so /path/to/title1/exefs /path/to/title2/exefs ...
```

## Investigate a crash in a title you own

1. `NXRT_LOG=1 ./out/main` shows start-up and every SVC.
2. A crash report names `module+offset`. Open the converted file in a disassembler: `.symtab` keeps original names.
3. `NXRT_NO_CRASH_HANDLER=1` lets `gdb` (or `qemu-aarch64 -g 1234` plus `gdb-multiarch`) catch the signal.
4. Report it with the bring-up issue template: function names and offsets only, no files.
