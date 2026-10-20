# libc

Horizon's C library is musl (switchbrew, "sdk" module contents; FEASIBILITY.md). Functions with identical contracts in glibc forward to the host.

## Gaps

- **Guest heap.** `malloc` and friends use the host allocator. Titles that size their heap with `nn::os::SetMemoryHeapSize` or expect allocations inside a specific range are not modelled.
- **stdio.** Guest `FILE*` values are host `FILE*` values. Only the printf family, puts, putchar, fputs, fputc, fwrite and fflush exist. No fopen: guest file access goes through `nn::fs`, which is not implemented yet.
- **musl versus glibc differences.** printf formatting of NaN/infinity and `%a`, `strtod` rounding and locale behaviour may differ in edge cases. Unmeasured.
- **errno values.** Forwarded to the host's errno; musl and glibc share Linux errno numbers, but whether Horizon's port changed any is unknown.
- **Missing families.** Wide characters, locale, time, math (titles usually inline it, unmeasured), `setjmp`/`longjmp`, `getenv` and threads (`pthread_*` as exposed by the SDK).
