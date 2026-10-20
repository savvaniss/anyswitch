// SPDX-License-Identifier: MIT
// The set of guest symbols the runtime provides. Built from the runtime's
// shared libraries (every exported "nx$" symbol) or from plain text lists.
#pragma once

#include <filesystem>
#include <string>
#include <unordered_set>

namespace nx {

class Registry {
public:
    void add_file(const std::filesystem::path& path);
    void add(std::string name) { names_.insert(std::move(name)); }
    bool contains(const std::string& name) const { return names_.count(name) != 0; }
    std::size_t size() const { return names_.size(); }
    bool empty() const { return names_.empty(); }

private:
    std::unordered_set<std::string> names_;
};

} // namespace nx
