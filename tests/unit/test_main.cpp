// SPDX-License-Identifier: MIT
#include "test.hpp"

#include <cstring>
#include <exception>
#include <iostream>

namespace nxtest {

std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

} // namespace nxtest

int main(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int failed = 0;
    int ran = 0;
    for (const auto& c : nxtest::registry()) {
        if (filter && !std::strstr(c.name, filter)) {
            continue;
        }
        ++ran;
        try {
            c.fn();
            std::cout << "PASS " << c.name << "\n";
        } catch (const nxtest::Failure& f) {
            ++failed;
            std::cout << "FAIL " << c.name << "\n  " << f.message << "\n";
        } catch (const std::exception& e) {
            ++failed;
            std::cout << "FAIL " << c.name << "\n  unexpected exception: " << e.what() << "\n";
        }
    }
    std::cout << ran - failed << "/" << ran << " passed\n";
    return failed == 0 && ran > 0 ? 0 : 1;
}
