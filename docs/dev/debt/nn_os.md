# nn_os

Names and prototypes: public symbol reconstructions (open-ead/nnheaders, nnsdk-rs). Semantics: switchbrew and corpus measurements.

## Gaps

- **ThreadType and MutexType layouts.** Treated as opaque; state lives in runtime side tables keyed by address. Titles that read these structures directly (inline accessors) will see unexpected contents. Needs a public layout source before changing.
- **Guest-provided thread stacks.** `CreateThread`'s stack argument is ignored; threads run on host stacks of at least 256 KiB. Titles that inspect their own stack memory will notice.
- **Priorities and core affinity.** Recorded but not applied.
- **Thread names, `GetThreadId`, `ChangeThreadPriority`, events, condition variables, semaphores, message queues, timers, barriers, read/write locks.** Not implemented; imports trap with exit 86.
- **Mutex semantics.** Every mutex is recursive. Console mutexes created as non-recursive would assert or deadlock on re-entry; anyswitch lets it through.
- **Memory APIs** (`AllocateMemoryBlock`, `SetMemoryHeapSize`, ...). Not implemented.
