// SPDX-License-Identifier: MIT
// LZ4 block format, written from the public specification:
// https://github.com/lz4/lz4/blob/dev/doc/lz4_Block_format.md
#include "nx/format/lz4.hpp"

#include <vector>

namespace nx::lz4 {
namespace {

std::size_t read_length(ByteSpan in, std::size_t& ip, std::size_t base) {
    std::size_t length = base;
    if (base != 15) {
        return length;
    }
    std::uint8_t b = 0;
    do {
        if (ip >= in.size()) {
            fail("lz4: truncated length");
        }
        b = in[ip++];
        length += b;
    } while (b == 255);
    return length;
}

void put_length(Bytes& out, std::size_t extra) {
    while (extra >= 255) {
        out.push_back(255);
        extra -= 255;
    }
    out.push_back(static_cast<std::uint8_t>(extra));
}

} // namespace

Bytes decompress_block(ByteSpan in, std::size_t expected_size) {
    Bytes out;
    out.reserve(expected_size);
    std::size_t ip = 0;
    for (;;) {
        if (ip >= in.size()) {
            fail("lz4: truncated block");
        }
        std::uint8_t token = in[ip++];
        std::size_t literals = read_length(in, ip, token >> 4);
        if (literals > in.size() - ip) {
            fail("lz4: literal run past end of input");
        }
        if (literals > expected_size - out.size()) {
            fail("lz4: output larger than the expected {} bytes", expected_size);
        }
        out.insert(out.end(), in.begin() + static_cast<std::ptrdiff_t>(ip),
                   in.begin() + static_cast<std::ptrdiff_t>(ip + literals));
        ip += literals;
        if (ip == in.size()) {
            break;
        }
        if (in.size() - ip < 2) {
            fail("lz4: truncated match offset");
        }
        std::size_t offset = static_cast<std::size_t>(in[ip]) | (static_cast<std::size_t>(in[ip + 1]) << 8);
        ip += 2;
        if (offset == 0 || offset > out.size()) {
            fail("lz4: invalid match offset {}", offset);
        }
        std::size_t match = read_length(in, ip, token & 15) + 4;
        if (match > expected_size - out.size()) {
            fail("lz4: output larger than the expected {} bytes", expected_size);
        }
        std::size_t from = out.size() - offset;
        for (std::size_t i = 0; i < match; ++i) {
            out.push_back(out[from + i]);
        }
    }
    if (out.size() != expected_size) {
        fail("lz4: decoded {} bytes, expected {}", out.size(), expected_size);
    }
    return out;
}

Bytes compress_block(ByteSpan in) {
    constexpr std::size_t kMinMatch = 4;
    constexpr std::size_t kLastLiterals = 5;
    constexpr std::size_t kMatchFindLimit = 12;
    Bytes out;
    const std::size_t n = in.size();
    std::size_t anchor = 0;

    auto emit = [&](std::size_t literal_count, std::size_t offset, std::size_t match) {
        std::size_t token_at = out.size();
        out.push_back(0);
        std::uint8_t token = 0;
        if (literal_count >= 15) {
            token = 0xF0;
            put_length(out, literal_count - 15);
        } else {
            token = static_cast<std::uint8_t>(literal_count << 4);
        }
        out.insert(out.end(), in.begin() + static_cast<std::ptrdiff_t>(anchor),
                   in.begin() + static_cast<std::ptrdiff_t>(anchor + literal_count));
        if (match != 0) {
            out.push_back(static_cast<std::uint8_t>(offset & 0xff));
            out.push_back(static_cast<std::uint8_t>(offset >> 8));
            std::size_t extra = match - kMinMatch;
            if (extra >= 15) {
                token |= 15;
                put_length(out, extra - 15);
            } else {
                token |= static_cast<std::uint8_t>(extra);
            }
        }
        out[token_at] = token;
    };

    if (n > kMatchFindLimit) {
        std::vector<std::int64_t> table(1u << 16, -1);
        const std::size_t limit = n - kMatchFindLimit;
        const std::size_t match_end_limit = n - kLastLiterals;
        auto read32 = [&](std::size_t p) { return load_le<std::uint32_t>(in.data() + p); };
        std::size_t ip = 0;
        while (ip < limit) {
            std::uint32_t h = (read32(ip) * 2654435761u) >> 16;
            std::int64_t ref = table[h];
            table[h] = static_cast<std::int64_t>(ip);
            if (ref >= 0 && ip - static_cast<std::size_t>(ref) <= 65535 &&
                read32(static_cast<std::size_t>(ref)) == read32(ip)) {
                std::size_t r = static_cast<std::size_t>(ref);
                std::size_t match = kMinMatch;
                while (ip + match < match_end_limit && in[r + match] == in[ip + match]) {
                    ++match;
                }
                emit(ip - anchor, ip - r, match);
                ip += match;
                anchor = ip;
            } else {
                ++ip;
            }
        }
    }
    emit(n - anchor, 0, 0);
    return out;
}

} // namespace nx::lz4
