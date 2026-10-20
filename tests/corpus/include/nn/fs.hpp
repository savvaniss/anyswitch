// SPDX-License-Identifier: MIT
// Declared but deliberately not implemented by the runtime yet: the "missing"
// corpus case calls it to check that unimplemented imports fail loudly.
#pragma once

#include "nn/os.hpp"

namespace nn::fs {

Result MountRom(const char* name, void* cache, unsigned long cache_size);

} // namespace nn::fs
