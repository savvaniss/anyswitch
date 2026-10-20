// SPDX-License-Identifier: MIT
#pragma once

#include "nx/common/bytes.hpp"

#include <filesystem>

namespace nx {

Bytes read_file(const std::filesystem::path& path);
void write_file(const std::filesystem::path& path, ByteSpan data, bool executable = false);

} // namespace nx
