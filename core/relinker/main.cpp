// SPDX-License-Identifier: MIT
// anyswitch-relink: converts NSO/NRO executables into host ELF files and
// measures what a module needs from the runtime.
#include "nxabi/nxabi.h"

#include "nx/common/file.hpp"
#include "nx/format/detect.hpp"
#include "nx/format/elf_reader.hpp"
#include "nx/format/module.hpp"
#include "nx/format/npdm.hpp"
#include "nx/relinker/census.hpp"
#include "nx/relinker/json.hpp"
#include "nx/relinker/registry.hpp"
#include "nx/relinker/writer.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#if __has_include(<cxxabi.h>)
#include <cxxabi.h>
#define NX_HAVE_CXXABI 1
#endif
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace nx {
namespace {

constexpr const char* kVersion = "0.1.0-dev";

const char* kUsage = R"(anyswitch-relink )"
                     "0.1.0-dev"
                     R"(

usage:
  anyswitch-relink info <file>
      Describe an NSO, NRO or NPDM file, or a module written by this tool.
  anyswitch-relink symbols [--json] <file>
      List the imports and exports of an NSO or NRO.
  anyswitch-relink census [--json] <file>...
      Count privileged instructions and relocation types (PLAN.md Phase 0).
  anyswitch-relink audit --registry LIB [--json] <file|exefs-dir>...
      List imports the runtime does not provide yet, per module.
  anyswitch-relink provided [--demangle] <runtime-lib|list>...
      List the guest symbols a runtime build provides.
  anyswitch-relink module <in> -o <out> [options]
      Convert one NSO or NRO.
        --shared              write a shared object instead of a main executable
        --name NAME           module name recorded for the runtime (default: main)
        --load-order N        exefs load order (main = 0, subsdkN = N + 1)
        --needed LIB          add a DT_NEEDED entry (repeatable)
        --soname NAME         DT_SONAME for --shared
        --runpath PATH        DT_RUNPATH (default: $ORIGIN/lib)
        --npdm FILE           main.npdm to embed
  anyswitch-relink exefs <dir> -o <outdir> [options]
      Convert main and subsdk0-9 from an exefs folder; rtld and sdk are replaced
      by the runtime. Writes <outdir>/main, <outdir>/lib/subsdkN.so and
      <outdir>/relink-report.json.
        --runtime-lib NAME    runtime library to depend on (default: libnxrt.so)

options shared by module and exefs:
  --registry FILE             runtime library or text list of provided guest
                              symbols (repeatable); missing called imports are
                              bound to stubs that report the name and exit
  --no-patch                  do not patch privileged instructions
  --no-ordered-exclusives     keep ldxr/stxr as they are
  --interp PATH               program interpreter (default /lib/ld-linux-aarch64.so.1)
)";

struct Args {
    std::vector<std::string> positional;
    std::map<std::string, std::vector<std::string>> values;
    std::map<std::string, bool> flags;

    bool flag(const std::string& name) const { return flags.count(name) != 0; }
    std::string value(const std::string& name, const std::string& fallback = {}) const {
        auto it = values.find(name);
        return it == values.end() || it->second.empty() ? fallback : it->second.back();
    }
    std::vector<std::string> all(const std::string& name) const {
        auto it = values.find(name);
        return it == values.end() ? std::vector<std::string>{} : it->second;
    }
};

Args parse_args(int argc, char** argv, int first) {
    static const char* with_value[] = {"-o", "--name", "--load-order", "--needed", "--soname", "--runpath",
                                       "--npdm", "--runtime-lib", "--registry", "--interp"};
    Args a;
    for (int i = first; i < argc; ++i) {
        std::string s = argv[i];
        bool takes_value = false;
        for (const char* w : with_value) {
            takes_value |= s == w;
        }
        if (takes_value) {
            if (i + 1 >= argc) {
                fail("{} needs a value", s);
            }
            a.values[s].push_back(argv[++i]);
        } else if (s.size() > 1 && s[0] == '-') {
            a.flags[s] = true;
        } else {
            a.positional.push_back(s);
        }
    }
    return a;
}

std::string hex_id(const std::array<std::uint8_t, 32>& id) {
    std::string s;
    std::size_t end = id.size();
    while (end > 0 && id[end - 1] == 0) {
        --end;
    }
    for (std::size_t i = 0; i < end; ++i) {
        s += std::format("{:02x}", id[i]);
    }
    return s.empty() ? "(none)" : s;
}

void print_module(const Module& m) {
    std::cout << std::format("{} ({})\n", m.name, m.format == ModuleFormat::Nso ? "NSO" : "NRO");
    std::cout << std::format("  module id     {}\n", hex_id(m.module_id));
    std::cout << std::format("  .text         {:#010x} size {:#x}\n", m.text.offset, m.text.size);
    std::cout << std::format("  .rodata       {:#010x} size {:#x}\n", m.ro.offset, m.ro.size);
    std::cout << std::format("  .data         {:#010x} size {:#x}\n", m.data.offset, m.data.size);
    std::cout << std::format("  .bss          {:#010x} size {:#x}\n", m.bss.offset, m.bss.size);
    std::cout << std::format("  MOD0          {:#010x} (dynamic {:#x}, eh_frame_hdr {:#x}-{:#x})\n", m.mod0.offset,
                             m.mod0.dynamic, m.mod0.eh_frame_hdr_start, m.mod0.eh_frame_hdr_end);
    std::cout << std::format("  symbols       {} ({} imports, {} exports), hash: {}{}\n", m.symbols.size(),
                             m.import_count(), m.export_count(), m.has_hash ? "sysv " : "",
                             m.has_gnu_hash ? "gnu" : "");
    std::map<std::string, std::size_t> by_type;
    for (const auto& r : m.relocs) {
        ++by_type[elf::reloc_name(r.type)];
    }
    for (const auto& [type, count] : by_type) {
        std::cout << std::format("  {:<24}{}\n", type, count);
    }
    std::cout << std::format("  init_array    {} entries, fini_array {} entries\n", m.init_array_size / 8,
                             m.fini_array_size / 8);
    std::cout << std::format("  nnMain        {}\n", m.find_export("nnMain") ? "exported" : "not exported");
}

void print_npdm(const Npdm& n) {
    std::cout << std::format("NPDM {}\n", n.name);
    std::cout << std::format("  64-bit code       {}\n", n.is_64bit ? "yes" : "no");
    std::cout << std::format("  address space     {} ({})\n", address_space_name(n.address_space), n.address_space);
    std::cout << std::format("  main thread       priority {}, core {}, stack {:#x}\n", n.main_thread_priority,
                             n.main_thread_core, n.main_thread_stack_size);
    if (n.program_id) {
        std::cout << std::format("  program id        {:016x}\n", *n.program_id);
    }
}

void print_converted(const ByteSpan data) {
    ElfFile f = read_elf(data);
    const elf::Phdr* p = f.find_segment(NXABI_PT_MODULE_INFO);
    if (!p) {
        std::cout << "ELF file without anyswitch module info\n";
        return;
    }
    Reader r(data, "elf");
    auto info = r.read<nxabi_module_info>(p->offset);
    std::cout << std::format("anyswitch module '{}' (abi v{}, load order {}, flags {:#x})\n",
                             std::string(info.name, strnlen(info.name, sizeof(info.name))), info.abi_version,
                             info.load_order, info.flags);
    std::cout << std::format("  image at {:#x}, size {:#x}; {} patches, {} trapped imports\n", info.image_offset,
                             info.image_size, info.patch_count, info.trap_count);
    std::cout << std::format("  nnMain {:#x}, init_array {} entries\n", info.nnmain_offset, info.init_array_count);
    for (const auto& d : f.dynamic) {
        if (d.tag == elf::DT_NEEDED) {
            std::cout << "  needs (see readelf -d)\n";
            break;
        }
    }
}

int cmd_info(const Args& a) {
    if (a.positional.size() != 1) {
        fail("info takes one file");
    }
    Bytes data = read_file(a.positional[0]);
    switch (detect_file_kind(data)) {
    case FileKind::Nso:
    case FileKind::Nro: print_module(load_module(data, fs::path(a.positional[0]).filename().string())); break;
    case FileKind::Npdm: print_npdm(parse_npdm(data)); break;
    case FileKind::Elf: print_converted(data); break;
    default: require_module_file(data, a.positional[0]);
    }
    return 0;
}

int cmd_symbols(const Args& a) {
    if (a.positional.size() != 1) {
        fail("symbols takes one file");
    }
    Bytes data = read_file(a.positional[0]);
    Module m = load_module(data, fs::path(a.positional[0]).filename().string());
    if (a.flag("--json")) {
        json::Builder j;
        j.begin_object().key("module").value(m.name);
        j.key("imports").begin_array();
        for (std::size_t i = 1; i < m.symbols.size(); ++i) {
            if (!m.symbols[i].defined() && !m.symbols[i].name.empty()) j.value(m.symbols[i].name);
        }
        j.end_array().key("exports").begin_array();
        for (std::size_t i = 1; i < m.symbols.size(); ++i) {
            const Symbol& s = m.symbols[i];
            if (s.defined() && !s.name.empty() && s.bind() != elf::STB_LOCAL) j.value(s.name);
        }
        j.end_array().end_object();
        std::cout << j.str() << "\n";
        return 0;
    }
    for (std::size_t i = 1; i < m.symbols.size(); ++i) {
        const Symbol& s = m.symbols[i];
        if (s.name.empty()) continue;
        std::cout << (s.defined() ? "export " : "import ") << s.name << "\n";
    }
    return 0;
}

int cmd_census(const Args& a) {
    if (a.positional.empty()) {
        fail("census needs at least one file");
    }
    bool as_json = a.flag("--json");
    if (as_json) std::cout << "[";
    for (std::size_t i = 0; i < a.positional.size(); ++i) {
        Bytes data = read_file(a.positional[i]);
        Census c = run_census(load_module(data, a.positional[i]));
        if (as_json) {
            std::cout << (i ? "," : "") << census_to_json(c);
        } else {
            std::cout << census_to_text(c);
        }
    }
    if (as_json) std::cout << "]\n";
    return 0;
}

std::string demangle(const std::string& name) {
#ifdef NX_HAVE_CXXABI
    int status = 0;
    char* out = abi::__cxa_demangle(name.c_str(), nullptr, nullptr, &status);
    if (status == 0 && out) {
        std::string s = out;
        std::free(out);
        return s;
    }
#endif
    return name;
}

std::vector<fs::path> expand_inputs(const std::vector<std::string>& inputs) {
    std::vector<fs::path> files;
    for (const auto& in : inputs) {
        fs::path p = in;
        if (!fs::is_directory(p)) {
            files.push_back(p);
            continue;
        }
        for (const char* name : {"main", "subsdk0", "subsdk1", "subsdk2", "subsdk3", "subsdk4", "subsdk5",
                                 "subsdk6", "subsdk7", "subsdk8", "subsdk9"}) {
            if (fs::exists(p / name)) files.push_back(p / name);
        }
    }
    return files;
}

int cmd_audit(const Args& a) {
    Registry registry;
    for (const auto& r : a.all("--registry")) registry.add_file(r);
    if (registry.empty()) fail("audit needs --registry <runtime library or list>");
    std::vector<fs::path> files = expand_inputs(a.positional);
    if (files.empty()) fail("audit needs at least one module or exefs folder");
    std::vector<Module> mods;
    std::unordered_set<std::string> exported;
    for (const auto& f : files) {
        mods.push_back(load_module(read_file(f), f.filename().string()));
        for (const auto& s : mods.back().symbols) {
            if (s.defined() && !s.name.empty()) exported.insert(s.name);
        }
    }
    bool as_json = a.flag("--json");
    json::Builder j;
    if (as_json) j.begin_array();
    for (const auto& m : mods) {
        std::vector<std::string> missing;
        std::size_t imports = 0;
        for (std::size_t i = 1; i < m.symbols.size(); ++i) {
            const Symbol& s = m.symbols[i];
            if (s.defined() || s.name.empty()) continue;
            ++imports;
            if (!registry.contains(s.name) && !exported.count(s.name)) missing.push_back(s.name);
        }
        std::sort(missing.begin(), missing.end());
        if (as_json) {
            j.begin_object().key("module").value(m.name);
            j.key("imports").value(static_cast<std::uint64_t>(imports));
            j.key("missing").begin_array();
            for (const auto& n : missing) j.value(n);
            j.end_array().end_object();
            continue;
        }
        std::cout << std::format("{}: {} of {} imports provided, {} missing\n", m.name, imports - missing.size(),
                                 imports, missing.size());
        for (const auto& n : missing) std::cout << "  " << demangle(n) << (demangle(n) != n ? "  [" + n + "]" : "") << "\n";
    }
    if (as_json) std::cout << j.end_array().str() << "\n";
    return 0;
}

int cmd_provided(const Args& a) {
    if (a.positional.empty()) fail("provided needs a runtime library or list");
    std::vector<std::string> names;
    for (const auto& p : a.positional) {
        Bytes data = read_file(p);
        if (detect_file_kind(data) == FileKind::Elf) {
            for (const auto& s : read_elf(data).dynamic_symbols) {
                if (s.shndx != elf::SHN_UNDEF && s.name.starts_with(NXABI_SYMBOL_PREFIX)) {
                    names.push_back(s.name.substr(std::strlen(NXABI_SYMBOL_PREFIX)));
                }
            }
        }
    }
    std::sort(names.begin(), names.end());
    for (const auto& n : names) {
        std::cout << (a.flag("--demangle") ? demangle(n) : n) << "\n";
    }
    return 0;
}

void apply_common(const Args& a, ConvertOptions& opt, Registry& registry) {
    for (const auto& r : a.all("--registry")) {
        registry.add_file(r);
    }
    if (!registry.empty()) opt.registry = &registry;
    opt.patch = !a.flag("--no-patch");
    opt.scan.ordered_exclusives = !a.flag("--no-ordered-exclusives");
    if (auto interp = a.value("--interp"); !interp.empty()) opt.interpreter = interp;
}

void print_report(const std::string& name, const ConvertReport& r) {
    std::cout << std::format("{}: {} imports, {} exports, {} relocations", name, r.imports, r.exports, r.relocations);
    std::size_t patches = 0;
    for (const auto& [kind, n] : r.patches) patches += n;
    std::cout << std::format(", {} patched instructions", patches);
    if (!r.trapped.empty()) std::cout << std::format(", {} missing functions bound to traps", r.trapped.size());
    std::cout << "\n";
    for (const auto& [kind, n] : r.patches) std::cout << std::format("  patch {:<22}{}\n", kind, n);
    for (const auto& n : r.unresolved_data) std::cout << "  unresolved non-call import (loader will fail): " << n << "\n";
    if (r.text_relocations) std::cout << "  note: relocations target read-only segments (DT_TEXTREL set)\n";
}

void report_json(json::Builder& j, const std::string& name, const ConvertReport& r) {
    j.begin_object();
    j.key("module").value(name);
    j.key("imports").value(static_cast<std::uint64_t>(r.imports));
    j.key("exports").value(static_cast<std::uint64_t>(r.exports));
    j.key("relocations").value(static_cast<std::uint64_t>(r.relocations));
    j.key("patches").begin_object();
    for (const auto& [kind, n] : r.patches) j.key(kind).value(static_cast<std::uint64_t>(n));
    j.end_object();
    j.key("trapped").begin_array();
    for (const auto& n : r.trapped) j.value(n);
    j.end_array();
    j.key("unresolved_data").begin_array();
    for (const auto& n : r.unresolved_data) j.value(n);
    j.end_array();
    j.end_object();
}

int cmd_module(const Args& a) {
    if (a.positional.size() != 1 || a.value("-o").empty()) {
        fail("module needs one input and -o <out>");
    }
    Bytes data = read_file(a.positional[0]);
    ConvertOptions opt;
    Registry registry;
    apply_common(a, opt, registry);
    opt.kind = a.flag("--shared") ? OutputKind::Shared : OutputKind::Main;
    opt.module_name = a.value("--name", opt.kind == OutputKind::Main ? "main" : fs::path(a.positional[0]).stem().string());
    opt.load_order = static_cast<std::uint32_t>(std::stoul(a.value("--load-order", "0")));
    opt.needed = a.all("--needed");
    opt.soname = a.value("--soname", opt.module_name + ".so");
    opt.runpath = a.value("--runpath", opt.runpath);
    if (auto npdm = a.value("--npdm"); !npdm.empty()) {
        opt.npdm_raw = read_file(npdm);
        opt.npdm = parse_npdm(opt.npdm_raw);
    }
    Module m = load_module(data, opt.module_name);
    ConvertResult r = convert_module(m, opt);
    write_file(a.value("-o"), r.elf, opt.kind == OutputKind::Main);
    print_report(opt.module_name, r.report);
    return 0;
}

int cmd_exefs(const Args& a) {
    if (a.positional.size() != 1 || a.value("-o").empty()) {
        fail("exefs needs one input folder and -o <outdir>");
    }
    fs::path in = a.positional[0];
    fs::path out = a.value("-o");
    std::string runtime_lib = a.value("--runtime-lib", "libnxrt.so");
    if (!fs::is_directory(in)) {
        fail("{} is not a folder", in.string());
    }
    for (const auto& e : fs::directory_iterator(in)) {
        if (!e.is_regular_file()) continue;
        std::string name = e.path().filename().string();
        bool known = name == "main" || name == "main.npdm" || name == "rtld" || name == "sdk" ||
                     (name.starts_with("subsdk") && name.size() == 7 && name[6] >= '0' && name[6] <= '9');
        if (!known) {
            Bytes probe = read_file(e.path());
            if (detect_file_kind(probe) == FileKind::PackagedContainer) {
                require_module_file(probe, e.path().string());
            }
            std::cerr << "note: ignoring " << name << "\n";
        }
    }
    if (!fs::exists(in / "main")) {
        fail("{}: no 'main' module", in.string());
    }
    for (const char* skipped : {"rtld", "sdk"}) {
        if (fs::exists(in / skipped)) std::cerr << "note: '" << skipped << "' is replaced by the runtime and not converted\n";
    }

    ConvertOptions base;
    Registry registry;
    apply_common(a, base, registry);

    struct Item {
        std::string name;
        std::uint32_t order;
        Module module;
    };
    std::vector<Item> items;
    items.push_back({"main", 0, load_module(read_file(in / "main"), "main")});
    for (int i = 0; i <= 9; ++i) {
        fs::path p = in / std::format("subsdk{}", i);
        if (fs::exists(p)) {
            std::string name = std::format("subsdk{}", i);
            items.push_back({name, static_cast<std::uint32_t>(i + 1), load_module(read_file(p), name)});
        }
    }
    std::unordered_set<std::string> exports;
    for (const auto& it : items) {
        for (std::size_t s = 1; s < it.module.symbols.size(); ++s) {
            const Symbol& sym = it.module.symbols[s];
            if (sym.defined() && !sym.name.empty() && sym.bind() != elf::STB_LOCAL) exports.insert(sym.name);
        }
    }

    json::Builder j;
    j.begin_object().key("modules").begin_array();
    std::vector<std::string> sub_libs;
    for (const auto& it : items) {
        if (it.name != "main") sub_libs.push_back(it.name + ".so");
    }
    for (const auto& it : items) {
        ConvertOptions opt = base;
        opt.module_name = it.name;
        opt.load_order = it.order;
        opt.sibling_exports = exports;
        if (it.name == "main") {
            opt.kind = OutputKind::Main;
            opt.needed = sub_libs;
            opt.needed.push_back(runtime_lib);
            opt.runpath = "$ORIGIN/lib";
            if (fs::exists(in / "main.npdm")) {
                opt.npdm_raw = read_file(in / "main.npdm");
                opt.npdm = parse_npdm(opt.npdm_raw);
            }
        } else {
            opt.kind = OutputKind::Shared;
            opt.soname = it.name + ".so";
            opt.needed = {runtime_lib};
            opt.runpath = "$ORIGIN";
        }
        ConvertResult r = convert_module(it.module, opt);
        fs::path dest = it.name == "main" ? out / "main" : out / "lib" / (it.name + ".so");
        write_file(dest, r.elf, it.name == "main");
        print_report(it.name, r.report);
        report_json(j, it.name, r.report);
    }
    j.end_array().end_object();
    std::string report = j.str() + "\n";
    write_file(out / "relink-report.json", ByteSpan(reinterpret_cast<const std::uint8_t*>(report.data()), report.size()));
    return 0;
}

int run(int argc, char** argv) {
    if (argc < 2 || !std::strcmp(argv[1], "-h") || !std::strcmp(argv[1], "--help") || !std::strcmp(argv[1], "help")) {
        std::cout << kUsage;
        return argc < 2 ? 1 : 0;
    }
    std::string cmd = argv[1];
    Args a = parse_args(argc, argv, 2);
    if (cmd == "version") {
        std::cout << kVersion << " (nxabi v" << NXABI_VERSION << ")\n";
        return 0;
    }
    if (cmd == "info") return cmd_info(a);
    if (cmd == "symbols") return cmd_symbols(a);
    if (cmd == "census") return cmd_census(a);
    if (cmd == "audit") return cmd_audit(a);
    if (cmd == "provided") return cmd_provided(a);
    if (cmd == "module") return cmd_module(a);
    if (cmd == "exefs") return cmd_exefs(a);
    fail("unknown command '{}' (try --help)", cmd);
}

} // namespace
} // namespace nx

int main(int argc, char** argv) {
    try {
        return nx::run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "anyswitch-relink: error: " << e.what() << "\n";
        return 2;
    }
}
