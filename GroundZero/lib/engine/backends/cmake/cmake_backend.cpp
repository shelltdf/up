#include "cmake/cmake_backend.hpp"

#include "commands_common.hpp"
#include "paths.hpp"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace gz {

namespace {

// O(1) "is p under (or same as) base" using lexical relative path — no I/O. Requires paths already
// from weakly_canonical for consistent root/spelling (same as original path_starts_with, but
// that did two weakly_canonical **per test**, which for infer_common_source_root meant millions of
// stat calls for projects with many sources).
static bool path_is_descendant_of_or_equal(const std::filesystem::path& base, const std::filesystem::path& p) {
  if (p == base)
    return true;
  const std::filesystem::path rel = p.lexically_relative(base);
  for (const auto& seg : rel) {
    if (seg == std::filesystem::path(".."))
      return false;
  }
  return !rel.empty();
}

std::optional<std::filesystem::path> infer_common_source_root(const ConfigureGraphModel& model) {
  std::vector<std::filesystem::path> raw;
  for (const auto& t : model.targets) {
    if (t.imported_prebuilt)
      continue;
    for (const auto& sp : t.source_paths)
      raw.push_back(sp);
  }
  if (raw.empty())
    return std::nullopt;
  // One weakly_canonical per source; then only lexical parent() / lexically_relative (no disk).
  std::vector<std::filesystem::path> canon;
  canon.reserve(raw.size());
  for (const auto& p : raw) {
    std::error_code ec;
    std::filesystem::path c = std::filesystem::weakly_canonical(std::filesystem::absolute(p), ec);
    if (ec)
      c = std::filesystem::absolute(p).lexically_normal();
    canon.push_back(std::move(c));
  }
  std::filesystem::path common = canon[0];
  for (size_t i = 1; i < canon.size(); ++i) {
    while (!common.empty() && !path_is_descendant_of_or_equal(common, canon[i]))
      common = common.parent_path();
  }
  if (common.empty())
    return std::nullopt;
  std::error_code ec;
  if (std::filesystem::is_regular_file(common, ec))
    common = common.parent_path();
  if (common.empty())
    return std::nullopt;
  return common;
}

std::string cmake_escape_string_value(const std::string& v) {
  std::string o;
  o.reserve(v.size() + 8);
  for (char c : v) {
    if (c == '\\')
      o += "\\\\";
    else if (c == '"')
      o += "\\\"";
    else if (c == ';')
      o += "\\;";
    else
      o.push_back(c);
  }
  return o;
}

std::string normalize_path_slashes_for_cmake(std::string s) {
  for (char& c : s) {
    if (c == '\\')
      c = '/';
  }
  while (s.size() >= 3 && s[0] == '/' && std::isalpha(static_cast<unsigned char>(s[1])) && s[2] == ':')
    s.erase(0, 1);
  return s;
}

std::string abs_path_string_for_cmake(const std::filesystem::path& p) {
  if (p.empty())
    return {};
  std::error_code ec;
  const std::filesystem::path ab = std::filesystem::absolute(p, ec);
  return normalize_path_slashes_for_cmake(
      to_posix_path_string((ec || ab.empty()) ? p : ab));
}

std::string abs_path_string_for_cmake(const std::string& s) {
  if (s.empty())
    return s;
  return abs_path_string_for_cmake(std::filesystem::path(s));
}

std::string list_path_from_cmake_source(const std::string& src_raw, const std::filesystem::path& build_root) {
  if (src_raw.empty())
    return {};
  if (gz_source_path_is_cmake_binary_dir(src_raw)) {
    const std::string rel = gz_cmake_binary_dir_source_rel(src_raw);
    if (rel.empty())
      return {};
    return std::string("${CMAKE_CURRENT_BINARY_DIR}/") + normalize_path_slashes_for_cmake(rel);
  }
  // write_cmake_lists can call this thousands of times; build_root is fixed — avoid repeated
  // weakly_canonical on the same path.
  static std::string s_br_cache_key;
  static std::filesystem::path s_br;
  static std::error_code s_br_ec;
  {
    const std::string k = to_posix_path_string(std::filesystem::absolute(build_root).lexically_normal());
    if (s_br_cache_key != k) {
      s_br_cache_key = k;
      s_br = std::filesystem::weakly_canonical(std::filesystem::absolute(build_root), s_br_ec);
    }
  }
  const std::filesystem::path& br = s_br;
  if (s_br_ec || br.empty())
    return abs_path_string_for_cmake(std::filesystem::path(src_raw));

  const std::filesystem::path p(src_raw);
  if (p.is_relative()) {
    std::string s = normalize_path_slashes_for_cmake(p.generic_string());
    while (!s.empty() && s.front() == '/')
      s.erase(0, 1);
    return std::string("${CMAKE_SOURCE_DIR}/") + s;
  }

  std::error_code ec;
  const std::filesystem::path abs_p = std::filesystem::absolute(p, ec);
  if (ec)
    return abs_path_string_for_cmake(p);
  std::filesystem::path rel = std::filesystem::relative(abs_p, br, ec);
  if (!ec && !rel.empty() && !rel.is_absolute()) {
    std::string rs = rel.generic_string();
    for (char& c : rs) {
      if (c == '\\')
        c = '/';
    }
    return std::string("${CMAKE_SOURCE_DIR}/") + rs;
  }
  return abs_path_string_for_cmake(abs_p);
}

std::string list_path_from_cmake_source(const std::filesystem::path& src_raw, const std::filesystem::path& build_root) {
  if (src_raw.empty())
    return {};
  return list_path_from_cmake_source(to_posix_path_string(src_raw), build_root);
}

std::string prebuilt_target_linker_path_for_lists(const ConfigureTargetModel& t, const std::filesystem::path& build_root) {
  if (t.type == "prebuilt_static_library")
    return list_path_from_cmake_source(t.imported_location, build_root);
  if (t.type == "prebuilt_shared_library") {
    if (!t.imported_implib.empty())
      return list_path_from_cmake_source(t.imported_implib, build_root);
    return list_path_from_cmake_source(t.imported_location, build_root);
  }
  return {};
}

// ---------------------------------------------------------------------------
// Prebuilt / link index (built once per write_cmake_lists)
// ---------------------------------------------------------------------------
struct PrebuiltLinkIndex {
  std::set<std::string> names;                   // prebuilt target names
  std::map<std::string, std::string> linker_path;  // name → list path; empty → use $<TARGET_LINKER_FILE:>
};

PrebuiltLinkIndex make_prebuilt_index(const ConfigureGraphModel& m) {
  PrebuiltLinkIndex o;
  for (const auto& t : m.targets) {
    if (t.type == "prebuilt_static_library" || t.type == "prebuilt_shared_library")
      o.names.insert(t.name);
  }
  for (const auto& t : m.targets) {
    if (t.type == "prebuilt_static_library" || t.type == "prebuilt_shared_library")
      o.linker_path[t.name] = prebuilt_target_linker_path_for_lists(t, m.build_root);
  }
  return o;
}

// ---------------------------------------------------------------------------
// Stages: append fragments to ostream (output dir / install policy lives in 02-physical spec, not in generated comments).
// ---------------------------------------------------------------------------
constexpr const char* kInd = "  ";

void append_preamble(std::ostringstream& o, const std::string& package_name) {
  o << "cmake_minimum_required(VERSION 3.20)\n";
  o << "project(" << package_name << " LANGUAGES C CXX)\n";
  o << "set(CMAKE_CXX_STANDARD 17)\n";
  o << "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n";
  o << "\n";
}

// GZ: `gz build` passes -DCMAKE_PREFIX_PATH=...; add standard `include/` and flat `zlib.h` roots.
void append_cmake_prefix_path_includes_block(std::ostringstream& o) {
  o << "# GZ: CMAKE_PREFIX_PATH roots: add include/ (find_package layout) and prefix itself if zlib.h is at root.\n";
  o << "if(CMAKE_PREFIX_PATH)\n";
  o << "  foreach(_gz_pfx IN LISTS CMAKE_PREFIX_PATH)\n";
  o << "    if(_gz_pfx)\n";
  o << "      if(EXISTS \"${_gz_pfx}/include\")\n";
  o << "        include_directories(SYSTEM \"${_gz_pfx}/include\")\n";
  o << "      endif()\n";
  o << "      if(EXISTS \"${_gz_pfx}/zlib.h\")\n";
  o << "        include_directories(SYSTEM \"${_gz_pfx}\")\n";
  o << "      endif()\n";
  o << "    endif()\n";
  o << "  endforeach()\n";
  o << "endif()\n";
  o << "\n";
}

// GZ: libzip `lib/compat.h` requires SIZEOF_OFF_T (from config.h or cmake). A minimal/stub config.h
// with those macros missing triggers `#error: unsupported size of off_t` at the final #else.
void append_msvc_libzip_type_size_fallback_block(std::ostringstream& o) {
  o << "# GZ: MSVC fallback for libzip-style `compat.h` when SIZEOF_OFF_T is not set (no/empty config.h).\n";
  o << "if(MSVC)\n";
  o << "  add_compile_definitions(SIZEOF_OFF_T=8 SIZEOF_SIZE_T=8)\n";
  o << "endif()\n";
  o << "\n";
}

void append_gz_workspace_root_block(std::ostringstream& o, const std::string& abs_workspace_posix) {
  if (abs_workspace_posix.empty())
    return;
  o << "# GroundZero: `gz configure` cwd = real project tree. Generated CMakeLists is under -S, so "
       "CMAKE_SOURCE_DIR is not the upstream tree; use GZ_WORKSPACE_ROOT for file(READ) to lib/ and similar.\n";
  o << "set(GZ_WORKSPACE_ROOT \"" << cmake_escape_string_value(normalize_path_slashes_for_cmake(abs_workspace_posix))
    << "\")\n\n";
}

void rewrite_prelude_paths_for_gz_workspace(std::string& prelude) {
  const std::string to = "${GZ_WORKSPACE_ROOT}/";
  static const char* from[] = {"${PROJECT_SOURCE_DIR}/", "${CMAKE_SOURCE_DIR}/"};
  for (const char* f : from) {
    const std::string froms(f);
    for (size_t pos = 0; (pos = prelude.find(froms, pos)) != std::string::npos;) {
      prelude.replace(pos, froms.size(), to);
      pos += to.size();
    }
  }
}

void append_cmake_prelude_block(std::ostringstream& o, const ConfigureGraphModel& model) {
  if (model.cmake_prelude.empty())
    return;
  std::string p = model.cmake_prelude;
  if (!model.gz_workspace_root.empty())
    rewrite_prelude_paths_for_gz_workspace(p);
  o << "# --- from package.xml <cmake_prelude> (before targets; e.g. file(READ)/file(WRITE) for generated sources)\n";
  o << p;
  if (p.back() != '\n')
    o << "\n";
  o << "\n";
}

void append_prebuilt_imported(
    std::ostringstream& o,
    const ConfigureGraphModel& m,
    const std::optional<std::filesystem::path>& pkg_root) {
  for (const auto& t : m.targets) {
    if (!t.imported_prebuilt)
      continue;
    const bool sh = (t.type == "prebuilt_shared_library");
    o << "add_library(" << t.name << " " << (sh ? "SHARED" : "STATIC") << " IMPORTED)\n";
    if (sh) {
      o << "if(WIN32)\n";
      o << kInd << "set_target_properties(" << t.name << " PROPERTIES\n";
      o << kInd << kInd << "IMPORTED_LOCATION \""
         << cmake_escape_string_value(list_path_from_cmake_source(t.imported_location, m.build_root)) << "\"\n";
      if (!t.imported_implib.empty())
        o << kInd << kInd << "IMPORTED_IMPLIB \""
           << cmake_escape_string_value(list_path_from_cmake_source(t.imported_implib, m.build_root)) << "\"\n";
      o << kInd << ")\n";
      o << "else()\n";
      o << kInd << "set_target_properties(" << t.name << " PROPERTIES IMPORTED_LOCATION \""
         << cmake_escape_string_value(list_path_from_cmake_source(t.imported_location, m.build_root)) << "\")\n";
      o << "endif()\n";
    } else {
      o << "set_target_properties(" << t.name << " PROPERTIES IMPORTED_LOCATION \""
         << cmake_escape_string_value(list_path_from_cmake_source(t.imported_location, m.build_root)) << "\")\n";
    }
    if (!t.include_dirs.empty()) {
      o << "target_include_directories(" << t.name << " INTERFACE\n";
      for (const auto& inc : t.include_dirs)
        o << kInd << "\"" << cmake_escape_string_value(list_path_from_cmake_source(inc, m.build_root)) << "\"\n";
      o << ")\n";
    }
    if (pkg_root) {
      o << "target_include_directories(" << t.name << " INTERFACE\n"
         << kInd << "\"" << cmake_escape_string_value(list_path_from_cmake_source(*pkg_root, m.build_root))
         << "\"\n)\n";
    }
    o << "\n";
  }
  o << "\n";
}

// When some sources live under e.g. upstream lib/ and generated files under build/out/lib/, infer_common_source_root
// is the package root only — too shallow for #include "zipint.h" (next to the .c in lib/). Add each source file's
// parent directory so internal headers next to sources resolve (libzip, etc.).
void append_include_parent_dirs_of_sources(std::ostringstream& o, const ConfigureTargetModel& t,
                                           const ConfigureGraphModel& m) {
  std::set<std::string> uniq;
  for (const auto& s : t.source_paths) {
    if (s.empty() || gz_source_path_is_cmake_binary_dir(s))
      continue;
    std::filesystem::path p(s);
    if (!p.has_parent_path())
      continue;
    std::error_code ec;
    const std::filesystem::path par = std::filesystem::absolute(p.parent_path(), ec);
    if (ec)
      continue;
    uniq.insert(to_posix_path_string(par.lexically_normal()));
  }
  if (uniq.empty())
    return;
  o << "target_include_directories(" << t.name << " PUBLIC\n";
  for (const auto& dir : uniq)
    o << kInd << "\"" << cmake_escape_string_value(list_path_from_cmake_source(dir, m.build_root)) << "\"\n";
  o << ")\n";
}

void append_source_rules_for_target(
    std::ostringstream& o, const ConfigureTargetModel& t, const ConfigureGraphModel& m, int& command_idx) {
  for (const auto& s : t.source_rules) {
    if (!s.preprocess_command.empty()) {
      const std::string stamp = "pre_stamp_" + std::to_string(command_idx++);
      o << kInd << "add_custom_command(OUTPUT " << stamp << " COMMAND " << s.preprocess_command
         << " COMMAND ${CMAKE_COMMAND} -E touch " << stamp << " DEPENDS \""
         << cmake_escape_string_value(list_path_from_cmake_source(s.path, m.build_root)) << "\")\n";
      o << kInd << "add_custom_target(pre_target_" << command_idx << " DEPENDS " << stamp << ")\n";
      o << kInd << "add_dependencies(" << t.name << " pre_target_" << command_idx << ")\n";
    }
    if (!s.postprocess_command.empty()) {
      o << kInd << "add_custom_command(TARGET " << t.name << " POST_BUILD COMMAND " << s.postprocess_command
         << ")\n";
    }
  }
}

void append_native_libraries(
    std::ostringstream& o,
    const ConfigureGraphModel& m,
    const std::optional<std::filesystem::path>& pkg_root,
    int& command_idx) {
  for (const auto& t : m.targets) {
    if (t.imported_prebuilt || t.type == "asset_bundle" || t.type == "custom_target")
      continue;
    if (!(t.type == "static_library" || t.type == "shared_library"))
      continue;
    const char* kind = (t.type == "shared_library") ? "SHARED" : "STATIC";
    if (t.source_paths.empty()) {
      o << "add_library(" << t.name << " " << kind << ")\n";
    } else {
      o << "add_library(" << t.name << " " << kind << "\n";
      for (const auto& s : t.source_paths)
        o << kInd << "\"" << cmake_escape_string_value(list_path_from_cmake_source(s, m.build_root)) << "\"\n";
      o << ")\n";
    }
    if (!t.include_dirs.empty()) {
      o << "target_include_directories(" << t.name << " PUBLIC\n";
      for (const auto& inc : t.include_dirs)
        o << kInd << "\"" << cmake_escape_string_value(list_path_from_cmake_source(inc, m.build_root)) << "\"\n";
      o << ")\n";
    }
    if (pkg_root) {
      o << "target_include_directories(" << t.name << " PUBLIC\n"
         << kInd << "\"" << cmake_escape_string_value(list_path_from_cmake_source(*pkg_root, m.build_root))
         << "\"\n)\n";
    }
    append_include_parent_dirs_of_sources(o, t, m);
    if (!t.compile_definitions.empty()) {
      o << "target_compile_definitions(" << t.name << " PRIVATE\n";
      for (const auto& d : t.compile_definitions)
        o << kInd << "\"" << cmake_escape_string_value(d) << "\"\n";
      o << ")\n";
    }
    if (!t.compile_flags.empty()) {
      o << "target_compile_options(" << t.name << " PRIVATE\n";
      for (const auto& f : t.compile_flags)
        o << kInd << "\"" << cmake_escape_string_value(f) << "\"\n";
      o << ")\n";
    }
    if (!t.link_flags.empty()) {
      o << "target_link_options(" << t.name << " PRIVATE\n";
      for (const auto& f : t.link_flags)
        o << kInd << "\"" << cmake_escape_string_value(f) << "\"\n";
      o << ")\n";
    }
    append_source_rules_for_target(o, t, m, command_idx);
    o << "\n";
  }
}

void append_custom_targets(std::ostringstream& o, const ConfigureGraphModel& m) {
  for (const auto& t : m.targets) {
    if (t.type != "custom_target")
      continue;
    o << "add_custom_target(" << t.name << ")\n\n";
  }
  for (const auto& t : m.targets) {
    if (t.type != "custom_target" || t.order_only_dependencies.empty())
      continue;
    o << "add_dependencies(" << t.name;
    for (const auto& n : t.order_only_dependencies)
      o << "\n" << kInd << n;
    o << "\n)\n\n";
  }
}

void append_order_only_for_native_libs(std::ostringstream& o, const ConfigureGraphModel& m) {
  for (const auto& t : m.targets) {
    if (t.type != "static_library" && t.type != "shared_library")
      continue;
    if (t.order_only_dependencies.empty())
      continue;
    o << "add_dependencies(" << t.name;
    for (const auto& n : t.order_only_dependencies)
      o << "\n" << kInd << n;
    o << "\n)\n\n";
  }
}

void append_link_item(
    std::ostringstream& o, const std::string& n, const PrebuiltLinkIndex& pbi) {
  if (pbi.names.count(n)) {
    const auto it = pbi.linker_path.find(n);
    if (it != pbi.linker_path.end() && !it->second.empty()) {
      o << " \"" << cmake_escape_string_value(it->second) << "\"";
      return;
    }
    o << " \"$<TARGET_LINKER_FILE:" << n << ">\"";
    return;
  }
  o << " " << n;
}

void split_links(
    const ConfigureTargetModel& t,
    std::vector<std::string>& out_priv,
    std::vector<std::string>& out_pub,
    std::vector<std::string>& out_iface) {
  for (const auto& pr : t.links) {
    if (pr.second == "public")
      out_pub.push_back(pr.first);
    else if (pr.second == "interface")
      out_iface.push_back(pr.first);
    else
      out_priv.push_back(pr.first);
  }
}

void append_link_group(
    std::ostringstream& o, const char* keyword, const std::vector<std::string>& names, const PrebuiltLinkIndex& pbi) {
  if (names.empty())
    return;
  o << kInd << keyword;
  for (const auto& n : names)
    append_link_item(o, n, pbi);
  o << "\n";
}

void append_executables(
    std::ostringstream& o,
    const ConfigureGraphModel& m,
    const std::optional<std::filesystem::path>& pkg_root,
    const PrebuiltLinkIndex& pbi) {
  for (const auto& t : m.targets) {
    if (t.type != "executable")
      continue;
    if (t.source_paths.empty()) {
      o << "add_executable(" << t.name << ")\n";
    } else {
      o << "add_executable(" << t.name << "\n";
      for (const auto& s : t.source_paths)
        o << kInd << "\"" << cmake_escape_string_value(list_path_from_cmake_source(s, m.build_root)) << "\"\n";
      o << ")\n";
    }
    if (!t.links.empty()) {
      std::vector<std::string> priv, pub, iface;
      split_links(t, priv, pub, iface);
      o << "target_link_libraries(" << t.name << "\n";
      append_link_group(o, "PRIVATE", priv, pbi);
      append_link_group(o, "PUBLIC", pub, pbi);
      append_link_group(o, "INTERFACE", iface, pbi);
      o << ")\n";
    }
    std::set<std::string> dep_unique;
    for (const auto& pr : t.links)
      dep_unique.insert(pr.first);
    for (const auto& d : t.order_only_dependencies)
      dep_unique.insert(d);
    if (!dep_unique.empty()) {
      o << "add_dependencies(" << t.name;
      for (const auto& n : dep_unique)
        o << "\n" << kInd << n;
      o << "\n)\n";
    }
    if (!t.include_dirs.empty()) {
      o << "target_include_directories(" << t.name << " PRIVATE\n";
      for (const auto& inc : t.include_dirs)
        o << kInd << "\"" << cmake_escape_string_value(list_path_from_cmake_source(inc, m.build_root)) << "\"\n";
      o << ")\n";
    }
    if (pkg_root) {
      o << "target_include_directories(" << t.name << " PRIVATE\n"
         << kInd << "\"" << cmake_escape_string_value(list_path_from_cmake_source(*pkg_root, m.build_root))
         << "\"\n)\n";
    }
    append_include_parent_dirs_of_sources(o, t, m);
    if (!t.compile_definitions.empty()) {
      o << "target_compile_definitions(" << t.name << " PRIVATE\n";
      for (const auto& d : t.compile_definitions)
        o << kInd << "\"" << cmake_escape_string_value(d) << "\"\n";
      o << ")\n";
    }
    if (!t.compile_flags.empty()) {
      o << "target_compile_options(" << t.name << " PRIVATE\n";
      for (const auto& f : t.compile_flags)
        o << kInd << "\"" << cmake_escape_string_value(f) << "\"\n";
      o << ")\n";
    }
    if (!t.link_flags.empty()) {
      o << "target_link_options(" << t.name << " PRIVATE\n";
      for (const auto& f : t.link_flags)
        o << kInd << "\"" << cmake_escape_string_value(f) << "\"\n";
      o << ")\n";
    }
    o << "\n";
  }
}

void append_test_block(std::ostringstream& o, const ConfigureGraphModel& m) {
  o << "include(CTest)\n";
  o << "enable_testing()\n";
  o << "\n";
  for (const auto& t : m.targets) {
    if (t.type == "executable")
      o << "add_test(\n"
         << kInd << "NAME " << t.name << "\n"
         << kInd << "COMMAND " << t.name << "\n"
         << ")\n";
  }
  o << "\n";
}

void collect_native_lib_names_by_kind(const ConfigureGraphModel& m, std::vector<std::string>& static_out,
                                      std::vector<std::string>& shared_out) {
  for (const auto& t : m.targets) {
    if (t.imported_prebuilt || t.type == "asset_bundle" || t.type == "custom_target")
      continue;
    if (t.type == "static_library")
      static_out.push_back(t.name);
    else if (t.type == "shared_library")
      shared_out.push_back(t.name);
  }
}

void append_install(
    std::ostringstream& o, const ConfigureGraphModel& m, int& command_idx) {
  if (!m.install_exe_names.empty()) {
    o << "install(TARGETS\n";
    for (const auto& exe_name : m.install_exe_names)
      o << kInd << exe_name << "\n";
    o << kInd << "RUNTIME DESTINATION bin\n)\n";
  }
  o << "\n";
  {
    std::vector<std::string> static_names;
    std::vector<std::string> shared_names;
    collect_native_lib_names_by_kind(m, static_names, shared_names);
    if (!static_names.empty()) {
      o << "install(TARGETS\n";
      for (const auto& n : static_names)
        o << kInd << n << "\n";
      o << kInd << "ARCHIVE DESTINATION lib\n)\n";
    }
    o << "\n";
    if (!shared_names.empty()) {
      // Windows: DLL is RUNTIME, import .lib is ARCHIVE; LIBRARY does not apply to DLLs (CMake install() docs).
      // Mixing LIBRARY+ARCHIVE+RUNTIME for the same target has led to .dll install without import .lib.
      o << "if(WIN32)\n";
      o << "install(TARGETS\n";
      for (const auto& n : shared_names)
        o << kInd << n << "\n";
      o << kInd << "RUNTIME DESTINATION bin\n" << kInd << "ARCHIVE DESTINATION lib\n)\n";
      o << "else()\n";
      o << "install(TARGETS\n";
      for (const auto& n : shared_names)
        o << kInd << n << "\n";
      o << kInd << "LIBRARY DESTINATION lib\n)\n";
      o << "endif()\n";
    }
  }
  o << "\n";
  for (const auto& t : m.targets) {
    if (!t.imported_prebuilt)
      continue;
    if (t.type == "prebuilt_shared_library") {
      o << "if(WIN32)\n";
      if (!t.imported_dll.empty())
        o << kInd << "install(FILES \""
           << cmake_escape_string_value(list_path_from_cmake_source(t.imported_dll, m.build_root))
           << "\" DESTINATION bin)\n";
      if (!t.imported_implib.empty())
        o << kInd << "install(FILES \""
           << cmake_escape_string_value(list_path_from_cmake_source(t.imported_implib, m.build_root))
           << "\" DESTINATION lib)\n";
      o << "else()\n";
      o << kInd << "install(FILES \""
         << cmake_escape_string_value(list_path_from_cmake_source(t.imported_location, m.build_root))
         << "\" DESTINATION lib)\n";
      o << "endif()\n";
    } else {
      o << "install(FILES \""
         << cmake_escape_string_value(list_path_from_cmake_source(t.imported_location, m.build_root))
         << "\" DESTINATION lib)\n";
    }
  }
  o << "\n";
  for (const auto& rule : m.install_dir_rules) {
    if (!rule.preprocess_command.empty())
      o << kInd << "add_custom_target(pre_include_dir_" << command_idx++ << " COMMAND " << rule.preprocess_command
         << ")\n";
    if (!rule.postprocess_command.empty())
      o << kInd << "add_custom_target(post_include_dir_" << command_idx++ << " COMMAND " << rule.postprocess_command
         << ")\n";
    {
      std::string sdir = list_path_from_cmake_source(rule.src, m.build_root);
      if (!sdir.empty() && sdir.back() != '/')
        sdir += '/';
      o << kInd << "install(DIRECTORY \"" << cmake_escape_string_value(sdir) << "\" DESTINATION " << rule.dst
         << " FILES_MATCHING PATTERN \"*.h\" PATTERN \"*.hh\" PATTERN \"*.hpp\" PATTERN \"*.hxx\")\n";
    }
  }
  o << "\n";
  for (const auto& rule : m.install_file_rules) {
    if (!rule.preprocess_command.empty())
      o << kInd << "add_custom_target(pre_include_file_" << command_idx++ << " COMMAND " << rule.preprocess_command
         << ")\n";
    if (!rule.postprocess_command.empty())
      o << kInd << "add_custom_target(post_include_file_" << command_idx++ << " COMMAND " << rule.postprocess_command
         << ")\n";
    o << kInd << "install(FILES \""
      << cmake_escape_string_value(list_path_from_cmake_source(rule.src, m.build_root)) << "\" DESTINATION " << rule.dst
      << ")\n";
  }
  o << "\n";
  for (const auto& rule : m.asset_dir_rules) {
    if (!rule.preprocess_command.empty())
      o << kInd << "add_custom_target(pre_asset_dir_" << command_idx++ << " COMMAND " << rule.preprocess_command
         << ")\n";
    if (!rule.postprocess_command.empty())
      o << kInd << "add_custom_target(post_asset_dir_" << command_idx++ << " COMMAND " << rule.postprocess_command
         << ")\n";
    {
      std::string sdir = list_path_from_cmake_source(rule.src, m.build_root);
      if (!sdir.empty() && sdir.back() != '/')
        sdir += '/';
      o << kInd << "install(DIRECTORY \"" << cmake_escape_string_value(sdir) << "\" DESTINATION " << rule.dst
         << ")\n";
    }
  }
  o << "\n";
  for (const auto& rule : m.asset_file_rules) {
    if (!rule.preprocess_command.empty())
      o << kInd << "add_custom_target(pre_asset_file_" << command_idx++ << " COMMAND " << rule.preprocess_command
         << ")\n";
    if (!rule.postprocess_command.empty())
      o << kInd << "add_custom_target(post_asset_file_" << command_idx++ << " COMMAND " << rule.postprocess_command
         << ")\n";
    o << kInd << "install(FILES \""
       << cmake_escape_string_value(list_path_from_cmake_source(rule.src, m.build_root)) << "\" DESTINATION "
       << rule.dst << ")\n";
  }
  o << "\n";
  o << "install(CODE \"message(STATUS \\\"gz: install stage complete\\\")\")\n";
}

}  // namespace

std::string build_cmake_build_command(const BuildBackendContext& ctx) {
  std::ostringstream cmd;
  cmd << "cmake -S \"" << to_posix_path_string(ctx.src_dir) << "\" -B \"" << to_posix_path_string(ctx.bin_dir)
      << "\" -DCMAKE_INSTALL_PREFIX=\"" << to_posix_path_string(ctx.install_dir) << "\"";
  if (!ctx.cmake_generator.empty()) {
    cmd << " -G \"" << ctx.cmake_generator << "\"";
  }
  for (const auto& kv : ctx.opts) {
    if (kv.first.rfind("GZ_", 0) != 0)
      continue;
    if (kv.first == "GZ_CMAKE_PREFIX_PATH")
      continue;
    cmd << " -D" << kv.first << "=\"" << kv.second << "\"";
  }
  if (!ctx.cmake_prefix_path.empty()) {
    cmd << " -DCMAKE_PREFIX_PATH=\"" << ctx.cmake_prefix_path << "\"";
  }
  if (!ctx.multi_config) {
    cmd << " -DCMAKE_BUILD_TYPE=" << ctx.config_name;
  }
  cmd << " && cmake --build \"" << to_posix_path_string(ctx.bin_dir) << "\" --parallel " << parallel_jobs_for_build(ctx.opts)
      << " --verbose";
  if (ctx.multi_config) {
    cmd << " --config " << ctx.config_name;
  }
  cmd << " --target install";
  return cmd.str();
}

std::string build_cmake_configure_command(const ConfigureBackendContext& ctx) {
  std::ostringstream cmd;
  cmd << "cmake -S \"" << to_posix_path_string(ctx.source_dir) << "\" -B \"" << to_posix_path_string(ctx.out_dir) << "\"";
  if (!ctx.cmake_generator.empty()) {
    cmd << " -G \"" << ctx.cmake_generator << "\"";
  }
  if (!ctx.multi_config) {
    cmd << " -DCMAKE_BUILD_TYPE=" << ctx.config_name;
  }
  return cmd.str();
}

int write_cmake_lists(const ConfigureGraphModel& model) {
  std::cerr << "configure: [gen cmake] assembling fragments (common root, prebuilt, libs, executables, install)…\n"
            << std::flush;
  std::ostringstream cm;
  int command_idx = 0;
  const auto pkg_root = infer_common_source_root(model);
  const PrebuiltLinkIndex prebuilt_link = make_prebuilt_index(model);

  append_preamble(cm, model.package_name);
  append_gz_workspace_root_block(cm, model.gz_workspace_root);
  append_cmake_prelude_block(cm, model);
  append_cmake_prefix_path_includes_block(cm);
  append_msvc_libzip_type_size_fallback_block(cm);
  append_prebuilt_imported(cm, model, pkg_root);
  append_native_libraries(cm, model, pkg_root, command_idx);
  append_custom_targets(cm, model);
  append_order_only_for_native_libs(cm, model);
  append_executables(cm, model, pkg_root, prebuilt_link);
  append_test_block(cm, model);
  append_install(cm, model, command_idx);

  std::cerr << "configure: [gen cmake] rendering (may take a moment for very large project trees)…\n" << std::flush;
  const std::string rendered = cm.str();
  const auto out_cmake = model.build_root / "CMakeLists.txt";
  std::ofstream f(out_cmake);
  if (!f) {
    return 5;
  }
  std::cerr << "configure: [gen cmake] writing " << to_posix_path_string(out_cmake) << "…\n" << std::flush;
  f << rendered;
  std::cout << "Wrote " << to_posix_path_string(out_cmake) << std::endl;
  return 0;
}

}  // namespace gz
