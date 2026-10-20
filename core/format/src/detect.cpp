// SPDX-License-Identifier: MIT
#include "nx/format/detect.hpp"

namespace nx {

FileKind detect_file_kind(ByteSpan data) {
    Reader r(data, "file");
    if (r.matches(0, "NSO0")) {
        return FileKind::Nso;
    }
    if (r.matches(0x10, "NRO0")) {
        return FileKind::Nro;
    }
    if (r.matches(0, "META")) {
        return FileKind::Npdm;
    }
    if (r.matches(0, "\x7f" "ELF")) {
        return FileKind::Elf;
    }
    if (r.matches(0, "PFS0") || r.matches(0, "HFS0") || r.matches(0x100, "HEAD") || r.matches(0x200, "NCA") ||
        r.matches(0, "NCZSECTN") || r.matches(0, "NCZBLOCK")) {
        return FileKind::PackagedContainer;
    }
    return FileKind::Unknown;
}

const char* file_kind_name(FileKind kind) {
    switch (kind) {
    case FileKind::Nso: return "NSO";
    case FileKind::Nro: return "NRO";
    case FileKind::Npdm: return "NPDM";
    case FileKind::Elf: return "ELF";
    case FileKind::PackagedContainer: return "packaged container";
    case FileKind::Unknown: return "unknown";
    }
    return "unknown";
}

void require_module_file(ByteSpan data, const std::string& path) {
    FileKind kind = detect_file_kind(data);
    if (kind == FileKind::Nso || kind == FileKind::Nro) {
        return;
    }
    if (kind == FileKind::PackagedContainer) {
        fail("{}: packaged or encrypted content is not accepted. anyswitch reads only NSO and NRO "
             "executables from an exefs folder you already have in plain form (docs/user/USAGE.md).",
             path);
    }
    fail("{}: not an NSO or NRO executable ({}). Packaged or encrypted content is not accepted "
         "(docs/user/USAGE.md).",
         path, file_kind_name(kind));
}

} // namespace nx
