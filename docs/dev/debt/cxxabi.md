# cxxabi

The Itanium C++ ABI is shared by Horizon and AArch64 Linux, so these forward to the host's libc++abi where possible.

## Gaps

- **Exceptions.** `__cxa_throw`, `__cxa_begin_catch`, `_Unwind_*` and RTTI support are not exported. Titles that throw will trap with exit 86. Converted modules keep `PT_GNU_EH_FRAME`, so the host unwinder can walk guest frames once these are wired up.
- **Replaced `operator new`.** If a title defines its own `operator new`, its own calls bind to it (it is found first), but runtime-internal allocations do not. Memory allocated by one and freed by the other would break; the runtime does not yet route guest-visible allocations through the title's operator.
- **`std::` library symbols.** Titles import parts of libc++ (`std::__1::...`, for example `basic_string` out-of-line members). None are provided yet.
