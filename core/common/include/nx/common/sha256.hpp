// SPDX-License-Identifier: MIT
#pragma once

#include "nx/common/bytes.hpp"

#include <array>

namespace nx {

// FIPS 180-4 SHA-256. Used to verify NSO segment hashes.
std::array<std::uint8_t, 32> sha256(ByteSpan data);

} // namespace nx
