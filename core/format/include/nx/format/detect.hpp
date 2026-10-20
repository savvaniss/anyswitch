// SPDX-License-Identifier: MIT
#pragma once

#include "nx/common/bytes.hpp"

namespace nx {

enum class FileKind {
    Nso,
    Nro,
    Npdm,
    Elf,
    PackagedContainer, // PFS0/HFS0/XCI/NCA and similar: never accepted, see docs/user/USAGE.md
    Unknown,
};

FileKind detect_file_kind(ByteSpan data);
const char* file_kind_name(FileKind kind);

// Throws nx::Error with the input-contract message unless the file is an NSO or NRO.
void require_module_file(ByteSpan data, const std::string& path);

} // namespace nx
