// SPDX-License-Identifier: MIT
#include "nx/common/file.hpp"

#include <fstream>

namespace nx {

Bytes read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        fail("cannot open {}", path.string());
    }
    in.seekg(0, std::ios::end);
    std::streamoff size = in.tellg();
    in.seekg(0, std::ios::beg);
    Bytes data(static_cast<std::size_t>(size));
    if (size > 0 && !in.read(reinterpret_cast<char*>(data.data()), size)) {
        fail("cannot read {}", path.string());
    }
    return data;
}

void write_file(const std::filesystem::path& path, ByteSpan data, bool executable) {
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) {
            fail("cannot create {}", path.string());
        }
        out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        if (!out) {
            fail("cannot write {}", path.string());
        }
    }
    if (executable) {
        using std::filesystem::perms;
        std::error_code ec;
        std::filesystem::permissions(path, perms::owner_exec | perms::group_exec | perms::others_exec,
                                     std::filesystem::perm_options::add, ec);
    }
}

} // namespace nx
