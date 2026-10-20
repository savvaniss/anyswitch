// SPDX-License-Identifier: MIT
#pragma once

#include "nx/common/error.hpp"

#include <bit>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nx {

static_assert(std::endian::native == std::endian::little, "only little-endian hosts are supported");

using Bytes = std::vector<std::uint8_t>;
using ByteSpan = std::span<const std::uint8_t>;

constexpr std::uint64_t align_up(std::uint64_t value, std::uint64_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

constexpr std::uint64_t align_down(std::uint64_t value, std::uint64_t alignment) {
    return value & ~(alignment - 1);
}

template <class T>
T load_le(const std::uint8_t* p) {
    T value;
    std::memcpy(&value, p, sizeof(T));
    return value;
}

template <class T>
void store_le(std::uint8_t* p, T value) {
    std::memcpy(p, &value, sizeof(T));
}

// Bounds-checked little-endian reads from a byte span. Offsets are absolute.
class Reader {
public:
    explicit Reader(ByteSpan data, std::string_view what = "input") : data_(data), what_(what) {}

    std::size_t size() const { return data_.size(); }
    ByteSpan data() const { return data_; }

    void require(std::uint64_t offset, std::uint64_t length) const {
        if (offset > data_.size() || length > data_.size() - offset) {
            fail("{}: read of {:#x} bytes at {:#x} is out of bounds (size {:#x})", what_, length, offset,
                 data_.size());
        }
    }

    template <class T>
    T read(std::uint64_t offset) const {
        require(offset, sizeof(T));
        return load_le<T>(data_.data() + offset);
    }

    std::uint8_t u8(std::uint64_t offset) const { return read<std::uint8_t>(offset); }
    std::uint16_t u16(std::uint64_t offset) const { return read<std::uint16_t>(offset); }
    std::uint32_t u32(std::uint64_t offset) const { return read<std::uint32_t>(offset); }
    std::uint64_t u64(std::uint64_t offset) const { return read<std::uint64_t>(offset); }
    std::int32_t s32(std::uint64_t offset) const { return read<std::int32_t>(offset); }

    ByteSpan slice(std::uint64_t offset, std::uint64_t length) const {
        require(offset, length);
        return data_.subspan(offset, length);
    }

    bool matches(std::uint64_t offset, std::string_view magic) const {
        if (offset > data_.size() || magic.size() > data_.size() - offset) {
            return false;
        }
        return std::memcmp(data_.data() + offset, magic.data(), magic.size()) == 0;
    }

    // NUL-terminated string starting at offset, limited to the end of the span.
    std::string cstr(std::uint64_t offset) const {
        require(offset, 0);
        const auto* begin = reinterpret_cast<const char*>(data_.data() + offset);
        std::size_t max = data_.size() - offset;
        std::size_t len = 0;
        while (len < max && begin[len] != '\0') {
            ++len;
        }
        if (len == max) {
            fail("{}: unterminated string at {:#x}", what_, offset);
        }
        return std::string(begin, len);
    }

private:
    ByteSpan data_;
    std::string_view what_;
};

// Appends little-endian values to a growing buffer.
class Writer {
public:
    Bytes& bytes() { return buf_; }
    std::size_t size() const { return buf_.size(); }

    template <class T>
    void put(T value) {
        std::size_t at = buf_.size();
        buf_.resize(at + sizeof(T));
        store_le(buf_.data() + at, value);
    }

    void put_bytes(ByteSpan data) { buf_.insert(buf_.end(), data.begin(), data.end()); }
    void put_string(std::string_view s) {
        buf_.insert(buf_.end(), s.begin(), s.end());
        buf_.push_back(0);
    }
    void pad_to(std::size_t size) {
        if (buf_.size() < size) {
            buf_.resize(size, 0);
        }
    }
    void align(std::size_t alignment) { pad_to(align_up(buf_.size(), alignment)); }

private:
    Bytes buf_;
};

} // namespace nx
