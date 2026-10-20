// SPDX-License-Identifier: MIT
// Freestanding C++ support every corpus module links: the DSO handle that
// compiler-generated __cxa_atexit registrations refer to.
extern "C" {
__attribute__((visibility("hidden"))) void* __dso_handle = &__dso_handle;
}
