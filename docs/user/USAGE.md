# Using anyswitch

anyswitch is early-stage software. Today it converts modules, runs the synthetic test corpus and reports exactly which SDK functions a title still needs. No commercial title is expected to run yet.

## Input contract

anyswitch reads only:

- NSO executables (`main`, `subsdk0` to `subsdk9`) and `main.npdm`
- NRO executables

from an `exefs` folder that you already have in plain, unpackaged form, from software you own. It does not read keys, does not decrypt, does not unpack packaged or encrypted containers, and the project will not explain how to obtain any of those things. Packaged or encrypted input is refused with an error.

## Commands

```bash
# what is in a module
anyswitch-relink info exefs/main
anyswitch-relink info exefs/main.npdm

# which SDK functions the runtime still lacks for this title
anyswitch-relink audit --registry lib/libnxrt.so exefs/

# convert: writes out/main, out/lib/subsdkN.so and out/relink-report.json
anyswitch-relink exefs exefs/ -o out --registry lib/libnxrt.so
cp lib/libnxrt.so out/lib/

# run on an AArch64 Linux machine
./out/main
```

Without `--registry`, missing imports make the host loader refuse to start. With it, a call to a missing function stops the program with exit status 86 and the function's name.

## Exit statuses

| Status | Meaning |
|---|---|
| 0 | the title's `nnMain` returned or it exited normally |
| 86 | the title reached a function or SVC the runtime does not implement yet; the name is printed |
| 87 | the title crashed or aborted; the location is printed as `module+offset` |
| 1 | the runtime could not set itself up (message printed) |

## Environment variables

| Variable | Effect |
|---|---|
| `NXRT_LOG=1` | log start-up, modules and SVCs to stderr |
| `NXRT_NO_CRASH_HANDLER=1` | leave crash signals to a debugger |

## Reporting

Use the "Bring-up report" issue template. Share function names, the audit summary line and `module+offset` locations only. Never attach files from a title.
