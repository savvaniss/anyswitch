// SPDX-License-Identifier: MIT
#pragma once

#include "nx/common/bytes.hpp"

namespace nx::lz4 {

// Decodes one LZ4 block (no frame header). The output must be exactly
// expected_size bytes; anything else is reported as a malformed block.
Bytes decompress_block(ByteSpan input, std::size_t expected_size);

// Simple greedy encoder. Used by tests and by the synthetic corpus tooling to
// exercise the decoder; not tuned for ratio.
Bytes compress_block(ByteSpan input);

} // namespace nx::lz4
