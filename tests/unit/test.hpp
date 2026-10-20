// SPDX-License-Identifier: MIT
// Minimal test harness: no third-party dependency, one executable, CTest runs it.
#pragma once

#include <cstdint>
#include <format>
#include <string>
#include <vector>

namespace nxtest {

struct Case {
    const char* name;
    void (*fn)();
};

std::vector<Case>& registry();

struct Register {
    Register(const char* name, void (*fn)()) { registry().push_back({name, fn}); }
};

struct Failure {
    std::string message;
};

} // namespace nxtest

#define NX_TEST(name)                                                    \
    static void name();                                                  \
    static ::nxtest::Register name##_registration(#name, &name);         \
    static void name()

#define NX_CHECK(cond)                                                                              \
    do {                                                                                            \
        if (!(cond)) {                                                                              \
            throw ::nxtest::Failure{std::format("{}:{}: check failed: {}", __FILE__, __LINE__, #cond)}; \
        }                                                                                           \
    } while (0)

#define NX_CHECK_EQ(a, b)                                                                              \
    do {                                                                                               \
        auto nx_a = static_cast<std::uint64_t>(a);                                                     \
        auto nx_b = static_cast<std::uint64_t>(b);                                                     \
        if (nx_a != nx_b) {                                                                            \
            throw ::nxtest::Failure{std::format("{}:{}: {} == {} failed: {:#x} != {:#x}", __FILE__,    \
                                                __LINE__, #a, #b, nx_a, nx_b)};                        \
        }                                                                                              \
    } while (0)

#define NX_CHECK_THROWS(expr)                                                                      \
    do {                                                                                           \
        bool nx_threw = false;                                                                     \
        try {                                                                                      \
            (void)(expr);                                                                          \
        } catch (const std::exception&) {                                                          \
            nx_threw = true;                                                                       \
        }                                                                                          \
        if (!nx_threw) {                                                                           \
            throw ::nxtest::Failure{std::format("{}:{}: expected exception from {}", __FILE__, __LINE__, #expr)}; \
        }                                                                                          \
    } while (0)
