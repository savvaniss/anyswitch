// SPDX-License-Identifier: MIT
#include "nxrt/runtime.hpp"

#include <cstdio>
#include <cstdlib>
#include <cxxabi.h>
#include <unistd.h>

namespace nxrt {

bool log_enabled() {
    static const bool enabled = [] {
        const char* v = std::getenv("NXRT_LOG");
        return v && *v && *v != '0';
    }();
    return enabled;
}

void log(const char* fmt, ...) {
    if (!log_enabled()) {
        return;
    }
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    std::fprintf(stderr, "[nxrt] %s\n", buf);
}

void fatal(int exit_code, const char* fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    std::fflush(stdout);
    std::fprintf(stderr, "[nxrt] fatal: %s\n", buf);
    std::fflush(stderr);
    _exit(exit_code);
}

std::string demangle(const char* name) {
    int status = 0;
    char* out = abi::__cxa_demangle(name, nullptr, nullptr, &status);
    if (status != 0 || !out) {
        return name;
    }
    std::string result = out;
    std::free(out);
    return result;
}

} // namespace nxrt
