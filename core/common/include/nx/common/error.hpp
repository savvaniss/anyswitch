// SPDX-License-Identifier: MIT
#pragma once

#include <format>
#include <stdexcept>
#include <string>
#include <utility>

namespace nx {

// Every unsupported or malformed input is reported by throwing nx::Error.
// Tools catch it at the top level, print what() and exit non-zero.
class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

template <class... Args>
[[noreturn]] void fail(std::format_string<Args...> fmt, Args&&... args) {
    throw Error(std::format(fmt, std::forward<Args>(args)...));
}

} // namespace nx
