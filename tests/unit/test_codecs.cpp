// SPDX-License-Identifier: MIT
#include "test.hpp"

#include "nx/common/sha256.hpp"
#include "nx/format/lz4.hpp"

#include <random>
#include <string>

namespace {

std::string hex(const std::array<std::uint8_t, 32>& d) {
    std::string s;
    for (auto b : d) s += std::format("{:02x}", b);
    return s;
}

nx::ByteSpan span_of(const std::string& s) {
    return nx::ByteSpan(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
}

void roundtrip(const nx::Bytes& input) {
    nx::Bytes packed = nx::lz4::compress_block(input);
    nx::Bytes unpacked = nx::lz4::decompress_block(packed, input.size());
    NX_CHECK(unpacked == input);
}

} // namespace

NX_TEST(sha256_known_vectors) {
    NX_CHECK(hex(nx::sha256(span_of(""))) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    NX_CHECK(hex(nx::sha256(span_of("abc"))) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    NX_CHECK(hex(nx::sha256(span_of("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"))) ==
             "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

NX_TEST(lz4_roundtrips) {
    roundtrip({});
    roundtrip({1, 2, 3});
    roundtrip(nx::Bytes(100000, 0xAB));
    nx::Bytes mixed;
    std::mt19937 rng(1234);
    for (int i = 0; i < 50000; ++i) {
        mixed.push_back(static_cast<std::uint8_t>(i % 7 == 0 ? rng() : i & 0x3F));
    }
    roundtrip(mixed);
    nx::Bytes noise(4096);
    for (auto& b : noise) b = static_cast<std::uint8_t>(rng());
    roundtrip(noise);
}

NX_TEST(lz4_rejects_malformed) {
    nx::Bytes packed = nx::lz4::compress_block(nx::Bytes(1000, 7));
    NX_CHECK_THROWS(nx::lz4::decompress_block(packed, 999));
    NX_CHECK_THROWS(nx::lz4::decompress_block(packed, 1001));
    nx::Bytes bad_offset = {0x14, 'a', 0x00, 0x00, 0x10, 'b'}; // match offset 0
    NX_CHECK_THROWS(nx::lz4::decompress_block(bad_offset, 64));
    nx::Bytes truncated = {0xF0};
    NX_CHECK_THROWS(nx::lz4::decompress_block(truncated, 64));
}
