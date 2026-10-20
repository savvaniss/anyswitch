# rt

Source of truth: switchbrew Rtld, SVC and Thread Local Region pages.

## Gaps

- **Initialiser order across modules.** Console: rtld runs module initialisers after relocating everything; the exact order between `subsdkN` and `main` is not documented. anyswitch: subsdk modules in load order, then main; finalisers in reverse. Assumption, needs a hardware measurement with homebrew.
- **`nninitStartup` and SDK initialisation.** Console: the SDK runs its own initialisation (heap, file system, ...) before `nnMain`. anyswitch: calls `nninitStartup` if exported, nothing else.
- **Thread Local Region contents.** Only the `ThreadType` pointer at 0x1F8 is filled. The IPC buffer, disable counter, errno, locale and other fields stay zero.
- **Guest thread pointer (`tpidr_el0`) for ELF TLS.** Starts as 0 on every thread. If titles use compiler TLS through `tpidr_el0`, the runtime must allocate a Horizon-layout TLS block per thread (PLAN.md Q3, unmeasured).
- **Host threads calling guest code.** A host-created thread must call `attach_guest_thread()` before entering guest code. Nothing enforces this yet.
- **SVC safety net.** Implemented: SetHeapSize, QueryMemory (from /proc/self/maps, types approximate), ExitProcess, SleepThread, GetSystemTick, GetThreadId (returns the host thread ID; no handles), Break, OutputDebugString, GetInfo 4-8, 11, 18. Everything else stops with exit 86. IPC is out of scope by design (AGENTS.md rule E3).
- **Crash handler.** Runs in the faulting thread's signal context without an alternate stack, so a stack overflow is reported as a plain `SIGSEGV` or not at all.
