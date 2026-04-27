#include "reverse_import_common.hpp"

#include "path_check.hpp"
#include "paths.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace up::reverse_import {

namespace {

std::string bucket_registry_key(const std::string& posix_rel_under_write_root) {
  return posix_rel_under_write_root.empty() ? std::string(".") : posix_rel_under_write_root;
}

bool relative_path_has_parent_escape(const std::filesystem::path& rel) {
  for (const auto& seg : rel) {
    if (seg == "..")
      return true;
  }
  return false;
}

std::string claim_dot_targets_bucket(const std::string& target_name, std::map<std::string, int>& bucket_claims) {
  const std::string b = std::string(".targets/") + sanitize_id(target_name);
  ++bucket_claims[bucket_registry_key(b)];
  return b;
}

}  // namespace

std::string read_file_text(const std::filesystem::path& path, std::string& error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    error = "cannot read: " + to_posix_path_string(path);
    return {};
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  error.clear();
  return ss.str();
}

bool is_src_ext(const std::string& ext) {
  const std::string e = [&] {
    std::string x = ext;
    for (char& c : x)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return x;
  }();
  return e == ".c" || e == ".cc" || e == ".cpp" || e == ".cxx" || e == ".c++";
}

bool skip_dir_name(const std::string& name) {
  return name == ".git" || name == ".svn" || name == ".hg" || name == ".intermediate" || name == "build" ||
         name == "out" || name == "cmake-build-debug" || name == "cmake-build-release" || name == "node_modules" ||
         name == ".vs" || name == "Debug" || name == "Release" || name == "x64" || name == "x86";
}

namespace {

std::string ascii_upper_underscore_token(const std::string& raw) {
  std::string u;
  u.reserve(raw.size());
  for (unsigned char uc : raw) {
    const char c = static_cast<char>(uc);
    if (c >= 'a' && c <= 'z')
      u += static_cast<char>(c - 'a' + 'A');
    else if (c >= 'A' && c <= 'Z' || c >= '0' && c <= '9' || c == '_')
      u += c;
    else
      return {};
  }
  return u;
}

bool is_denied_macro_style_target_upper(const std::string& u) {
  if (u.empty())
    return true;
  // Looks like a CMake cache / internal variable, not a target.
  if (u.rfind("CMAKE_", 0) == 0)
    return true;
  // Typical macro / configure_file formal parameters (never use these as real target names).
  static const char* kExact[] = {"SETUP_TARGET_NAME",
                                 "TARGET_NAME",
                                 "LIB_NAME",
                                 "EXE_NAME",
                                 "BINARY_NAME",
                                 "OUTPUT_NAME",
                                 "PROJECT_NAME",
                                 "PACKAGE_NAME",
                                 "UPSTREAM_TARGET",
                                 "MANGLE_TARGET_NAME",
                                 "ARGN"};
  for (const char* s : kExact) {
    if (u == s)
      return true;
  }
  // Very long ALL_CAPS_SNAKE tokens are usually macro glue, not targets.
  if (u.size() >= 28) {
    bool all_caps_snake = true;
    for (char c : u) {
      if (c != '_' && !(c >= 'A' && c <= 'Z') && !(c >= '0' && c <= '9')) {
        all_caps_snake = false;
        break;
      }
    }
    if (all_caps_snake && std::count(u.begin(), u.end(), '_') >= 4)
      return true;
  }
  return false;
}

}  // namespace

bool plausible_cmake_target_name_token(const std::string& raw) {
  if (raw.empty())
    return false;
  if (raw.find('$') != std::string::npos || raw.find('<') != std::string::npos)
    return false;
  // Double-leading-underscore names are almost always macros / template placeholders, not real targets.
  if (raw.size() >= 2 && raw[0] == '_' && raw[1] == '_')
    return false;
  // Namespaced imported targets (Qt5::Core, …): only apply $ / __ rules.
  if (raw.find("::") != std::string::npos)
    return true;
  const std::string u = ascii_upper_underscore_token(raw);
  if (u.empty())
    return false;
  if (is_denied_macro_style_target_upper(u))
    return false;
  return true;
}

std::string sanitize_id(std::string s) {
  if (s.empty())
    s = "target";
  for (char& c : s) {
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_'))
      c = '_';
  }
  return s;
}

std::string normalize_dep_package_name(std::string s) {
  s = sanitize_id(std::move(s));
  for (char& c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

std::string posix_str(const std::filesystem::path& p) {
  return to_posix_path_string(p);
}

std::optional<std::filesystem::path> try_relative(const std::filesystem::path& base, const std::filesystem::path& target) {
  std::error_code ec;
  const std::filesystem::path rel = std::filesystem::relative(target, base, ec);
  if (ec)
    return std::nullopt;
  return rel;
}

bool looks_like_source_token(const std::string& t) {
  if (t.empty() || t[0] == '$' || t[0] == '@')
    return false;
  const auto dot = t.rfind('.');
  if (dot == std::string::npos)
    return false;
  return is_src_ext(t.substr(dot));
}

std::string resolve_target_xml_bucket(const std::filesystem::path& write_root,
                                      const std::filesystem::path& preferred_dir_abs, const std::string& target_name,
                                      const std::vector<std::filesystem::path>& abs_sources,
                                      std::map<std::string, int>& bucket_claims) {
  std::error_code ec;
  std::filesystem::path wr = std::filesystem::weakly_canonical(write_root, ec);
  if (ec)
    wr = std::filesystem::absolute(write_root);
  std::filesystem::path pref = std::filesystem::weakly_canonical(preferred_dir_abs, ec);
  if (ec)
    pref = std::filesystem::absolute(preferred_dir_abs);

  const auto rel_opt = try_relative(wr, pref);
  if (!rel_opt)
    return claim_dot_targets_bucket(target_name, bucket_claims);

  std::filesystem::path norm = rel_opt->lexically_normal();
  std::string preferred_posix;
  if (norm.empty() || norm == ".")
    preferred_posix.clear();
  else
    preferred_posix = posix_str(norm);

  const std::filesystem::path base = wr / preferred_posix;
  for (const auto& abs : abs_sources) {
    std::filesystem::path ap = std::filesystem::weakly_canonical(abs, ec);
    if (ec)
      ap = std::filesystem::absolute(abs);
    const auto rp = try_relative(base, ap);
    if (!rp || relative_path_has_parent_escape(*rp))
      return claim_dot_targets_bucket(target_name, bucket_claims);
  }

  const std::string key = bucket_registry_key(preferred_posix);
  if (bucket_claims[key] > 0)
    return claim_dot_targets_bucket(target_name, bucket_claims);
  ++bucket_claims[key];
  return preferred_posix;
}

void add_target_dependency(TargetDesc& td, const std::string& dep_name) {
  if (dep_name.empty())
    return;
  for (const auto& d : td.dependencies) {
    if (d.name == dep_name)
      return;
  }
  td.dependencies.push_back(TargetDesc::DependencyEntry{dep_name, "private"});
}

bool add_target_include_dir(TargetDesc& td, const std::filesystem::path& write_root, const std::string& subdir,
                            const std::filesystem::path& abs_include_dir, std::vector<std::string>& warnings) {
  if (path_has_non_ascii(abs_include_dir)) {
    warnings.push_back("skip non-ASCII include path: " + posix_str(abs_include_dir));
    return false;
  }
  std::error_code ec;
  std::filesystem::path wr = std::filesystem::weakly_canonical(write_root, ec);
  if (ec)
    wr = std::filesystem::absolute(write_root);
  std::filesystem::path inc = std::filesystem::weakly_canonical(abs_include_dir, ec);
  if (ec)
    inc = std::filesystem::absolute(abs_include_dir);
  const std::filesystem::path target_dir = (wr / subdir).lexically_normal();
  std::filesystem::path rel = inc.lexically_relative(target_dir);
  std::string from;
  if (rel.empty())
    from = posix_str(inc);
  else
    from = posix_str(rel.lexically_normal());
  if (from.empty())
    return false;
  for (const auto& existing : td.includes) {
    if (existing.kind == "dir" && existing.from == from)
      return false;
  }
  td.includes.push_back(TargetDesc::IncludeEntry{"dir", from, "", "", ""});
  return true;
}

void push_target(ImportedPackage& out, const std::filesystem::path& write_root, const std::string& subdir,
                 const std::string& name, const std::string& type, const std::vector<std::filesystem::path>& abs_sources,
                 std::vector<std::string>& warnings) {
  TargetDesc td;
  td.name = name;
  td.type = type;
  const std::filesystem::path target_dir = write_root / subdir;
  for (const auto& abs : abs_sources) {
    if (path_has_non_ascii(abs)) {
      warnings.push_back("skip non-ASCII path: " + posix_str(abs));
      continue;
    }
    auto rel = try_relative(target_dir, std::filesystem::absolute(abs));
    if (!rel) {
      warnings.push_back("cannot relativize (different root?): " + posix_str(abs));
      continue;
    }
    std::string rs = posix_str(*rel);
    td.sources.push_back(rs);
    td.source_entries.push_back({"file", rs, "", ""});
  }
  if (td.sources.empty()) {
    warnings.push_back("target `" + name + "` has no importable sources (heuristic)");
    return;
  }
  out.targets.push_back({subdir, std::move(td)});
}

}  // namespace up::reverse_import
