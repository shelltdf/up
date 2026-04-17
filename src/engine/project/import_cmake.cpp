#include "project_import_internal.hpp"
#include "project_import_common.hpp"

#include "cli_verbose.hpp"
#include "path_check.hpp"
#include "paths.hpp"

#include <iostream>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <vector>

namespace up {
namespace {

struct InstallTargetRule {
  std::vector<std::string> targets;
  std::string runtime_dest;
  std::string library_dest;
  std::string archive_dest;
  std::string include_dest;
};

struct CmakeLibraryInfo {
  enum class Kind {
    StaticOnly,
    SharedOnly,
    Unknown
  };
  Kind kind = Kind::Unknown;
  std::string name;
};

std::optional<std::string> extract_braced_var(const std::string& token) {
  if (token.size() >= 4 && token[0] == '$' && token[1] == '{' && token.back() == '}')
    return token.substr(2, token.size() - 3);
  return std::nullopt;
}

std::optional<std::string> balanced_paren_content(const std::string& s, size_t open_paren) {
  if (open_paren >= s.size() || s[open_paren] != '(')
    return std::nullopt;
  int depth = 0;
  for (size_t i = open_paren; i < s.size(); ++i) {
    if (s[i] == '(')
      depth++;
    else if (s[i] == ')') {
      depth--;
      if (depth == 0)
        return s.substr(open_paren + 1, i - open_paren - 1);
    }
  }
  return std::nullopt;
}

void split_cmake_args(const std::string& inner, std::vector<std::string>& out_tokens) {
  std::string cur;
  bool in_str = false;
  for (size_t i = 0; i < inner.size(); ++i) {
    const char c = inner[i];
    if (!in_str) {
      if (c == '"' || c == '\'') {
        in_str = true;
        continue;
      }
      if (std::isspace(static_cast<unsigned char>(c))) {
        if (!cur.empty()) {
          out_tokens.push_back(cur);
          cur.clear();
        }
        continue;
      }
      cur += c;
    } else {
      if (c == '"' || c == '\'') {
        in_str = false;
        continue;
      }
      cur += c;
    }
  }
  if (!cur.empty())
    out_tokens.push_back(cur);
}

bool is_cmake_keyword_skip(const std::string& t) {
  static const std::set<std::string> k{"STATIC", "SHARED", "MODULE", "OBJECT", "INTERFACE", "IMPORTED", "ALIAS",
                                       "WIN32", "MACOSX_BUNDLE", "EXCLUDE_FROM_ALL", "GLOBAL", "NO_POLICY_SCOPE"};
  std::string u = t;
  for (char& c : u)
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return k.count(u) > 0;
}

std::string map_lib_type(const std::vector<std::string>& toks) {
  bool sh = false;
  for (const auto& t : toks) {
    std::string u = t;
    for (char& c : u)
      c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (u == "SHARED")
      sh = true;
  }
  if (sh)
    return "shared_library";
  return "static_library";
}

void strip_quotes(std::string& s) {
  if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
    s = s.substr(1, s.size() - 2);
  else if (s.size() >= 2 && s.front() == '\'' && s.back() == '\'')
    s = s.substr(1, s.size() - 2);
}

std::string upper_ascii(std::string s) {
  for (char& c : s)
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}

std::string normalize_rel_install_path(std::string s) {
  for (char& c : s) {
    if (c == '\\')
      c = '/';
  }
  while (!s.empty() && s.front() == '/')
    s.erase(s.begin());
  while (!s.empty() && s.back() == '/')
    s.pop_back();
  return s;
}

std::string join_rel_install_path(const std::string& dir, const std::string& name) {
  const std::string d = normalize_rel_install_path(dir);
  if (d.empty())
    return normalize_rel_install_path(name);
  return d + "/" + normalize_rel_install_path(name);
}

void collect_set_scalar_vars(const std::string& text, std::map<std::string, std::string>& vars) {
  const std::string needle = "set(";
  size_t pos = 0;
  while ((pos = text.find(needle, pos)) != std::string::npos) {
    if (pos > 0 && (std::isalnum(static_cast<unsigned char>(text[pos - 1])) || text[pos - 1] == '_')) {
      pos += needle.size();
      continue;
    }
    const size_t lparen = pos + needle.size() - 1;
    auto inner_opt = balanced_paren_content(text, lparen);
    if (!inner_opt) {
      pos += needle.size();
      continue;
    }
    std::vector<std::string> parts;
    split_cmake_args(*inner_opt, parts);
    if (parts.size() >= 2) {
      std::string k = parts[0];
      std::string v = parts[1];
      strip_quotes(k);
      strip_quotes(v);
      if (!k.empty() && !v.empty() && v.find('$') == std::string::npos)
        vars[k] = v;
    }
    pos += needle.size();
  }
}

// Collect literal source paths from `set(VAR a.c b.c ...)` for later ${VAR} expansion (e.g. zlib's ZLIB_SRCS).
void collect_set_source_lists(const std::filesystem::path& cmake_dir, const std::string& text,
                              std::map<std::string, std::vector<std::filesystem::path>>& var_paths) {
  const std::string needle = "set(";
  size_t pos = 0;
  while ((pos = text.find(needle, pos)) != std::string::npos) {
    if (pos > 0 && (std::isalnum(static_cast<unsigned char>(text[pos - 1])) || text[pos - 1] == '_')) {
      pos += needle.size();
      continue;
    }
    const size_t lparen = pos + needle.size() - 1;
    auto inner_opt = balanced_paren_content(text, lparen);
    if (!inner_opt) {
      pos += needle.size();
      continue;
    }
    std::vector<std::string> parts;
    split_cmake_args(*inner_opt, parts);
    if (parts.size() < 2) {
      pos += needle.size();
      continue;
    }
    strip_quotes(parts[0]);
    const std::string& var = parts[0];
    if (parts.size() >= 2 && (parts[1] == "CACHE" || parts[1] == "PARENT_SCOPE")) {
      pos += needle.size();
      continue;
    }
    std::vector<std::filesystem::path> acc;
    for (size_t i = 1; i < parts.size(); ++i) {
      if (parts[i] == "CACHE" || parts[i] == "PARENT_SCOPE" || parts[i] == "FORCE")
        break;
      strip_quotes(parts[i]);
      const std::string& tok = parts[i];
      if (tok.find('$') != std::string::npos)
        continue;
      if (!project_import::looks_like_source_token(tok))
        continue;
      std::filesystem::path rel(tok);
      std::filesystem::path abs_p = rel.is_absolute() ? rel : (cmake_dir / rel);
      std::error_code ec;
      abs_p = std::filesystem::weakly_canonical(abs_p, ec);
      if (!ec && std::filesystem::exists(abs_p))
        acc.push_back(abs_p);
    }
    if (!acc.empty()) {
      auto& slot = var_paths[var];
      slot.insert(slot.end(), acc.begin(), acc.end());
    }
    pos += needle.size();
  }
}

void append_resolved_paths(const std::filesystem::path& cmake_dir, const std::string& token,
                           const std::map<std::string, std::vector<std::filesystem::path>>& var_paths,
                           std::vector<std::filesystem::path>& abs) {
  if (project_import::looks_like_source_token(token)) {
    std::filesystem::path rel(token);
    std::filesystem::path abs_p = rel.is_absolute() ? rel : (cmake_dir / rel);
    std::error_code ec;
    abs_p = std::filesystem::weakly_canonical(abs_p, ec);
    if (!ec && std::filesystem::exists(abs_p))
      abs.push_back(abs_p);
    return;
  }
  static const std::regex br(R"(\$\{([^}]+)\})");
  std::smatch m;
  if (std::regex_match(token, m, br)) {
    const auto it = var_paths.find(m[1].str());
    if (it != var_paths.end()) {
      for (const auto& p : it->second)
        abs.push_back(p);
    }
    return;
  }
  const auto it = var_paths.find(token);
  if (it != var_paths.end()) {
    for (const auto& p : it->second)
      abs.push_back(p);
  }
}

std::vector<std::filesystem::path> collect_subdirectory_cmakes(const std::filesystem::path& parent_dir,
                                                                 const std::string& text) {
  std::vector<std::filesystem::path> out;
  std::string lower = text;
  for (char& c : lower)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  const std::string needle = "add_subdirectory(";
  size_t pos = 0;
  while ((pos = lower.find(needle, pos)) != std::string::npos) {
    if (pos > 0 && (std::isalnum(static_cast<unsigned char>(lower[pos - 1])) || lower[pos - 1] == '_')) {
      ++pos;
      continue;
    }
    const size_t lparen = pos + needle.size() - 1;
    auto inner_opt = balanced_paren_content(text, lparen);
    if (!inner_opt) {
      pos += needle.size();
      continue;
    }
    std::vector<std::string> toks;
    split_cmake_args(*inner_opt, toks);
    if (toks.empty()) {
      pos += needle.size();
      continue;
    }
    std::string sub = toks[0];
    strip_quotes(sub);
    std::filesystem::path child_cmake = parent_dir / sub / "CMakeLists.txt";
    std::error_code ec;
    if (std::filesystem::exists(child_cmake, ec))
      out.push_back(std::move(child_cmake));
    pos += needle.size();
  }
  return out;
}

void collect_executable_targets_from_cmake(const std::string& text, std::set<std::string>& out) {
  const std::string needle = "add_executable(";
  size_t p = 0;
  while ((p = text.find(needle, p)) != std::string::npos) {
    if (p > 0 && (std::isalnum(static_cast<unsigned char>(text[p - 1])) || text[p - 1] == '_')) {
      ++p;
      continue;
    }
    const size_t lparen = p + needle.size() - 1;
    auto inner_opt = balanced_paren_content(text, lparen);
    if (!inner_opt) {
      p = lparen + 1;
      continue;
    }
    std::vector<std::string> toks;
    split_cmake_args(*inner_opt, toks);
    if (toks.empty()) {
      p = lparen + 1;
      continue;
    }
    strip_quotes(toks[0]);
    if (!project_import::plausible_cmake_target_name_token(toks[0])) {
      p = lparen + 1;
      continue;
    }
    bool skip = false;
    for (const auto& t : toks) {
      const std::string u = upper_ascii(t);
      if (u == "ALIAS" || u == "IMPORTED")
        skip = true;
    }
    if (!skip) {
      const std::string id = project_import::sanitize_id(toks[0]);
      if (!id.empty())
        out.insert(id);
    }
    p = lparen + 1;
  }
}

void collect_library_targets_from_cmake(const std::string& text, std::map<std::string, CmakeLibraryInfo>& libs) {
  const std::string needle = "add_library(";
  size_t p = 0;
  while ((p = text.find(needle, p)) != std::string::npos) {
    if (p > 0 && (std::isalnum(static_cast<unsigned char>(text[p - 1])) || text[p - 1] == '_')) {
      ++p;
      continue;
    }
    const size_t lparen = p + needle.size() - 1;
    auto inner_opt = balanced_paren_content(text, lparen);
    if (!inner_opt) {
      p = lparen + 1;
      continue;
    }
    std::vector<std::string> toks;
    split_cmake_args(*inner_opt, toks);
    if (toks.empty()) {
      p = lparen + 1;
      continue;
    }
    strip_quotes(toks[0]);
    if (!project_import::plausible_cmake_target_name_token(toks[0])) {
      p = lparen + 1;
      continue;
    }
    const std::string sanitized = project_import::sanitize_id(toks[0]);
    if (sanitized.empty()) {
      p = lparen + 1;
      continue;
    }
    bool skip = false;
    bool shared = false;
    bool statik = false;
    for (const auto& t : toks) {
      const std::string u = upper_ascii(t);
      if (u == "INTERFACE" || u == "IMPORTED" || u == "ALIAS" || u == "OBJECT")
        skip = true;
      if (u == "SHARED" || u == "MODULE")
        shared = true;
      if (u == "STATIC")
        statik = true;
    }
    if (!skip) {
      CmakeLibraryInfo info;
      info.name = sanitized;
      if (shared)
        info.kind = CmakeLibraryInfo::Kind::SharedOnly;
      else if (statik)
        info.kind = CmakeLibraryInfo::Kind::StaticOnly;
      else
        info.kind = CmakeLibraryInfo::Kind::Unknown;
      libs[sanitized] = std::move(info);
    }
    p = lparen + 1;
  }
}

std::string resolve_cmake_var_token(std::string v, const std::map<std::string, std::string>& vars) {
  strip_quotes(v);
  auto key = extract_braced_var(v);
  if (key.has_value()) {
    const auto it = vars.find(*key);
    if (it != vars.end())
      return it->second;
    const std::string u = upper_ascii(*key);
    if (u.find("BIN") != std::string::npos)
      return "bin";
    if (u.find("LIB") != std::string::npos || u.find("ARCHIVE") != std::string::npos)
      return "lib";
    if (u.find("INCLUDE") != std::string::npos || u.find("HEADER") != std::string::npos)
      return "include";
  }
  return v;
}

std::optional<InstallTargetRule> parse_install_targets_rule(const std::string& inner,
                                                            const std::map<std::string, std::string>& vars) {
  std::vector<std::string> toks;
  split_cmake_args(inner, toks);
  if (toks.empty())
    return std::nullopt;
  if (upper_ascii(toks[0]) != "TARGETS")
    return std::nullopt;

  InstallTargetRule rule;
  std::string mode = "targets";
  static const std::set<std::string> section_keys = {"ARCHIVE", "LIBRARY", "RUNTIME", "PUBLIC_HEADER", "PRIVATE_HEADER",
                                                      "INCLUDES", "NAMELINK_COMPONENT", "NAMELINK_ONLY", "NAMELINK_SKIP",
                                                      "OBJECTS", "FRAMEWORK", "BUNDLE", "RESOURCE"};
  static const std::set<std::string> stop_keys = {"EXPORT", "COMPONENT", "CONFIGURATIONS", "OPTIONAL", "PERMISSIONS",
                                                  "RENAME", "DESTINATION"};
  for (size_t i = 1; i < toks.size(); ++i) {
    std::string tok = toks[i];
    strip_quotes(tok);
    const std::string u = upper_ascii(tok);
    if (mode == "targets") {
      if (section_keys.count(u) || stop_keys.count(u)) {
        mode = u;
        continue;
      }
      if (!tok.empty() && project_import::plausible_cmake_target_name_token(tok))
        rule.targets.push_back(project_import::sanitize_id(tok));
      continue;
    }
    if (u == "DESTINATION") {
      if (i + 1 < toks.size()) {
        std::string dest = resolve_cmake_var_token(toks[++i], vars);
        if (mode == "RUNTIME")
          rule.runtime_dest = normalize_rel_install_path(dest);
        else if (mode == "LIBRARY")
          rule.library_dest = normalize_rel_install_path(dest);
        else if (mode == "ARCHIVE")
          rule.archive_dest = normalize_rel_install_path(dest);
        else if (mode == "INCLUDES")
          rule.include_dest = normalize_rel_install_path(dest);
      }
      continue;
    }
    if (section_keys.count(u) || stop_keys.count(u))
      mode = u;
  }
  if (rule.targets.empty())
    return std::nullopt;
  return rule;
}

void collect_install_target_rules(const std::string& text, const std::map<std::string, std::string>& vars,
                                  std::vector<InstallTargetRule>& out_rules) {
  const std::string needle = "install(";
  size_t p = 0;
  while ((p = text.find(needle, p)) != std::string::npos) {
    if (p > 0 && (std::isalnum(static_cast<unsigned char>(text[p - 1])) || text[p - 1] == '_')) {
      ++p;
      continue;
    }
    const size_t lparen = p + needle.size() - 1;
    auto inner_opt = balanced_paren_content(text, lparen);
    if (!inner_opt) {
      p = lparen + 1;
      continue;
    }
    auto rule = parse_install_targets_rule(*inner_opt, vars);
    if (rule.has_value())
      out_rules.push_back(std::move(*rule));
    p = lparen + 1;
  }
}

void collect_install_directory_include(const std::string& text, const std::map<std::string, std::string>& vars,
                                       std::string& include_dir) {
  std::regex dir_re(R"rx(install\s*\(\s*DIRECTORY\s+[^)]*DESTINATION\s+([^\s\)]+)[^)]*\))rx",
                    std::regex::icase);
  for (std::sregex_iterator it(text.begin(), text.end(), dir_re), end; it != end; ++it) {
    std::string dst = resolve_cmake_var_token((*it)[1].str(), vars);
    dst = normalize_rel_install_path(dst);
    if (dst == "include" || dst.rfind("include/", 0) == 0) {
      include_dir = dst;
      return;
    }
  }
}

void collect_install_files_include(const std::string& text, const std::map<std::string, std::string>& vars,
                                   std::string& include_dir) {
  std::regex files_re(R"rx(install\s*\(\s*FILES\s+[^)]*DESTINATION\s+([^\s\)]+)[^)]*\))rx", std::regex::icase);
  for (std::sregex_iterator it(text.begin(), text.end(), files_re), end; it != end; ++it) {
    std::string dst = resolve_cmake_var_token((*it)[1].str(), vars);
    dst = normalize_rel_install_path(dst);
    if (dst == "include" || dst.rfind("include/", 0) == 0) {
      include_dir = dst;
      return;
    }
  }
}

void import_cmake_wrappers_recursive(const std::filesystem::path& cmake_file,
                                     std::set<std::string>& seen_cmake_files,
                                     std::map<std::string, CmakeLibraryInfo>& libs,
                                     std::set<std::string>& exe_names,
                                     std::map<std::string, std::string>& scalar_vars,
                                     std::vector<InstallTargetRule>& install_rules, std::string& include_dir,
                                     std::string& error, int depth) {
  if (depth > 16)
    return;
  std::error_code ec;
  const std::filesystem::path canon = std::filesystem::weakly_canonical(cmake_file, ec);
  const std::string canon_s = project_import::posix_str(canon);
  if (seen_cmake_files.count(canon_s))
    return;
  seen_cmake_files.insert(canon_s);

  const std::string text = project_import::read_file_text(cmake_file, error);
  if (!error.empty())
    return;
  collect_set_scalar_vars(text, scalar_vars);
  collect_library_targets_from_cmake(text, libs);
  collect_executable_targets_from_cmake(text, exe_names);
  collect_install_target_rules(text, scalar_vars, install_rules);
  if (include_dir.empty()) {
    collect_install_directory_include(text, scalar_vars, include_dir);
    if (include_dir.empty())
      collect_install_files_include(text, scalar_vars, include_dir);
  }

  for (const auto& child : collect_subdirectory_cmakes(cmake_file.parent_path(), text))
    import_cmake_wrappers_recursive(child, seen_cmake_files, libs, exe_names, scalar_vars, install_rules, include_dir,
                                   error, depth + 1);
}

void push_installed_wrappers_for_lib(const CmakeLibraryInfo& lib, const std::string& runtime_dest,
                                     const std::string& archive_dest, const std::string& library_dest,
                                     const std::string& iface_dir, std::set<std::string>& emitted,
                                     ImportedPackage& out) {
  const bool build_static =
      (lib.kind == CmakeLibraryInfo::Kind::StaticOnly || lib.kind == CmakeLibraryInfo::Kind::Unknown);
  const bool build_shared =
      (lib.kind == CmakeLibraryInfo::Kind::SharedOnly || lib.kind == CmakeLibraryInfo::Kind::Unknown);

  if (build_static) {
    TargetDesc td;
    td.name = lib.name;
    td.type = "imported_installed_static_library";
    TargetDesc::InstalledWrapDesc iw;
#if defined(_WIN32)
    iw.artifact = join_rel_install_path(archive_dest, lib.name + ".lib");
#else
    iw.artifact = join_rel_install_path(archive_dest, "lib" + lib.name + ".a");
#endif
    iw.interface_include = iface_dir;
    td.installed_wrap = std::move(iw);
    const std::string key = "." + td.name + ":" + td.type;
    if (emitted.insert(key).second)
      out.targets.push_back({".targets/" + td.name, std::move(td)});
  }
  if (build_shared) {
    TargetDesc td;
    td.name = (build_static ? (lib.name + "_shared") : lib.name);
    td.type = "imported_installed_shared_library";
    TargetDesc::InstalledWrapDesc iw;
#if defined(_WIN32)
    iw.artifact = join_rel_install_path(runtime_dest, lib.name + ".dll");
    iw.implib = join_rel_install_path(archive_dest, lib.name + ".lib");
#elif defined(__APPLE__)
    iw.artifact = join_rel_install_path(library_dest, "lib" + lib.name + ".dylib");
#else
    iw.artifact = join_rel_install_path(library_dest, "lib" + lib.name + ".so");
#endif
    iw.interface_include = iface_dir;
    td.installed_wrap = std::move(iw);
    const std::string key = "." + td.name + ":" + td.type;
    if (emitted.insert(key).second)
      out.targets.push_back({".targets/" + td.name, std::move(td)});
  }
}

void collect_find_package_names(const std::string& text, std::map<std::string, bool>& out_names) {
  std::string lower = text;
  for (char& c : lower)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  const std::string needle = "find_package(";
  size_t p = 0;
  while ((p = lower.find(needle, p)) != std::string::npos) {
    if (p > 0 && (std::isalnum(static_cast<unsigned char>(lower[p - 1])) || lower[p - 1] == '_')) {
      ++p;
      continue;
    }
    const size_t lparen = p + needle.size() - 1;
    auto inner_opt = balanced_paren_content(text, lparen);
    if (!inner_opt) {
      p = lparen + 1;
      continue;
    }
    std::vector<std::string> toks;
    split_cmake_args(*inner_opt, toks);
    if (!toks.empty()) {
      std::string pkg = toks[0];
      strip_quotes(pkg);
      if (!pkg.empty() && pkg.find('$') == std::string::npos) {
        pkg = project_import::normalize_dep_package_name(std::move(pkg));
        if (!pkg.empty() && pkg != "required" && pkg != "quiet") {
          bool required = false;
          for (size_t i = 1; i < toks.size(); ++i) {
            std::string tk = toks[i];
            strip_quotes(tk);
            if (upper_ascii(tk) == "REQUIRED") {
              required = true;
              break;
            }
          }
          auto it = out_names.find(pkg);
          if (it == out_names.end())
            out_names[pkg] = required;
          else if (required)
            it->second = true;
        }
      }
    }
    p = lparen + 1;
  }
}

void import_cmake_find_packages_recursive(const std::filesystem::path& cmake_file,
                                          std::set<std::string>& seen_cmake_files,
                                          std::map<std::string, bool>& deps,
                                          std::string& error,
                                          int depth);

void probe_find_package_dep_map(const std::filesystem::path& cmake_file,
                                std::set<std::string>& seen_cmake_files,
                                std::map<std::string, bool>& deps,
                                std::string& error,
                                int depth) {
  import_cmake_find_packages_recursive(cmake_file, seen_cmake_files, deps, error, depth);
}

void normalize_find_package_dep_map(std::map<std::string, bool>& deps) {
  // Normalize names through shared rules, and merge required flags.
  std::map<std::string, bool> normalized;
  for (const auto& kv : deps) {
    const std::string key = project_import::normalize_dep_package_name(kv.first);
    if (key.empty())
      continue;
    auto it = normalized.find(key);
    if (it == normalized.end())
      normalized[key] = kv.second;
    else if (kv.second)
      it->second = true;
  }
  deps.swap(normalized);
}

void emit_find_package_dep_list(const std::map<std::string, bool>& deps,
                                std::vector<std::pair<std::string, bool>>& out_deps) {
  out_deps.clear();
  out_deps.reserve(deps.size());
  for (const auto& kv : deps)
    out_deps.push_back(kv);
}

void replace_all_inplace(std::string& s, const std::string& from, const std::string& to) {
  if (from.empty())
    return;
  size_t pos = 0;
  while ((pos = s.find(from, pos)) != std::string::npos) {
    s.replace(pos, from.size(), to);
    pos += to.size();
  }
}

bool is_link_scope_keyword(const std::string& u) {
  return u == "PUBLIC" || u == "PRIVATE" || u == "INTERFACE" || u == "LINK_PUBLIC" || u == "LINK_PRIVATE";
}

bool is_link_config_keyword(const std::string& u) {
  return u == "DEBUG" || u == "OPTIMIZED" || u == "GENERAL";
}

bool is_include_dir_keyword(const std::string& u) {
  return u == "PUBLIC" || u == "PRIVATE" || u == "INTERFACE" || u == "BEFORE" || u == "AFTER" || u == "SYSTEM" ||
         u == "FILE_SET" || u == "TYPE" || u == "ITEMS" || u == "FILES" || u == "BASE_DIRS";
}

bool target_type_is_importable_link_dep(const std::string& ty) {
  return ty == "static_library" || ty == "shared_library" || ty == "imported_static_library" ||
         ty == "imported_shared_library" || ty == "imported_installed_static_library" ||
         ty == "imported_installed_shared_library" || ty == "asset_bundle";
}

void expand_static_cmake_path_vars(std::string& tok, const std::filesystem::path& current_list_dir,
                                   const std::filesystem::path& project_source_root) {
  const std::string cur = project_import::posix_str(current_list_dir);
  const std::string root = project_import::posix_str(project_source_root);
  replace_all_inplace(tok, "${CMAKE_CURRENT_SOURCE_DIR}", cur);
  replace_all_inplace(tok, "${CMAKE_CURRENT_LIST_DIR}", cur);
  replace_all_inplace(tok, "${PROJECT_SOURCE_DIR}", root);
  replace_all_inplace(tok, "${CMAKE_SOURCE_DIR}", root);
}

void collect_target_link_library_items(const std::vector<std::string>& parts, std::vector<std::string>& out_items) {
  out_items.clear();
  if (parts.size() < 2)
    return;
  for (size_t i = 1; i < parts.size(); ++i) {
    std::string t = parts[i];
    strip_quotes(t);
    if (t.empty())
      continue;
    std::string u;
    u.reserve(t.size());
    for (char c : t)
      u += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (is_link_scope_keyword(u))
      continue;
    if (is_link_config_keyword(u)) {
      if (i + 1 < parts.size())
        ++i;
      continue;
    }
    out_items.push_back(std::move(t));
  }
}

void collect_target_include_path_tokens(const std::vector<std::string>& parts, std::vector<std::string>& out_paths) {
  out_paths.clear();
  if (parts.size() < 2)
    return;
  for (size_t i = 1; i < parts.size(); ++i) {
    std::string t = parts[i];
    strip_quotes(t);
    if (t.empty())
      continue;
    std::string u;
    u.reserve(t.size());
    for (char c : t)
      u += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (is_include_dir_keyword(u))
      continue;
    out_paths.push_back(std::move(t));
  }
}

struct CmakeStaticGraphMaps {
  std::map<std::string, TargetDesc*> by_name;
  std::map<std::string, std::string> subdir_by_name;
};

void import_cmake_static_graph_recursive(const std::filesystem::path& cmake_file,
                                         const std::filesystem::path& write_root,
                                         const std::filesystem::path& project_source_root, CmakeStaticGraphMaps& maps,
                                         std::vector<std::string>& warnings, std::string& error,
                                         std::set<std::string>& seen_cmake_files, int depth) {
  if (depth > 16)
    return;
  std::error_code ec;
  const std::filesystem::path canon = std::filesystem::weakly_canonical(cmake_file, ec);
  const std::string canon_s = project_import::posix_str(canon);
  if (seen_cmake_files.count(canon_s))
    return;
  seen_cmake_files.insert(canon_s);

  const std::string text = project_import::read_file_text(cmake_file, error);
  if (!error.empty())
    return;

  const std::filesystem::path cmake_dir = cmake_file.parent_path();

  const std::string link_needle = "target_link_libraries(";
  size_t lp = 0;
  while ((lp = text.find(link_needle, lp)) != std::string::npos) {
    if (lp > 0 && (std::isalnum(static_cast<unsigned char>(text[lp - 1])) || text[lp - 1] == '_')) {
      ++lp;
      continue;
    }
    const size_t lparen = lp + link_needle.size() - 1;
    auto inner_opt = balanced_paren_content(text, lparen);
    if (!inner_opt) {
      lp = lparen + 1;
      continue;
    }
    std::vector<std::string> parts;
    split_cmake_args(*inner_opt, parts);
    if (parts.empty()) {
      lp = lparen + 1;
      continue;
    }
    strip_quotes(parts[0]);
    const std::string consumer = project_import::sanitize_id(parts[0]);
    auto cons_it = maps.by_name.find(consumer);
    if (cons_it == maps.by_name.end() || !cons_it->second) {
      lp = lparen + 1;
      continue;
    }
    TargetDesc* consumer_td = cons_it->second;
    std::vector<std::string> items;
    collect_target_link_library_items(parts, items);
    for (const std::string& raw_item : items) {
      if (raw_item.find('$') != std::string::npos || raw_item.find('<') != std::string::npos)
        continue;
      if (!project_import::plausible_cmake_target_name_token(raw_item))
        continue;
      const std::string dep_name = project_import::sanitize_id(raw_item);
      if (dep_name.empty() || dep_name == consumer)
        continue;
      auto dep_it = maps.by_name.find(dep_name);
      if (dep_it == maps.by_name.end() || !dep_it->second)
        continue;
      if (!target_type_is_importable_link_dep(dep_it->second->type))
        continue;
      project_import::add_target_dependency(*consumer_td, dep_name);
    }
    lp = lparen + 1;
  }

  const std::string inc_needle = "target_include_directories(";
  size_t ip = 0;
  while ((ip = text.find(inc_needle, ip)) != std::string::npos) {
    if (ip > 0 && (std::isalnum(static_cast<unsigned char>(text[ip - 1])) || text[ip - 1] == '_')) {
      ++ip;
      continue;
    }
    const size_t lparen = ip + inc_needle.size() - 1;
    auto inner_opt = balanced_paren_content(text, lparen);
    if (!inner_opt) {
      ip = lparen + 1;
      continue;
    }
    std::vector<std::string> parts;
    split_cmake_args(*inner_opt, parts);
    if (parts.empty()) {
      ip = lparen + 1;
      continue;
    }
    strip_quotes(parts[0]);
    const std::string consumer = project_import::sanitize_id(parts[0]);
    auto cons_it = maps.by_name.find(consumer);
    if (cons_it == maps.by_name.end() || !cons_it->second) {
      ip = lparen + 1;
      continue;
    }
    TargetDesc* consumer_td = cons_it->second;
    const auto sub_it = maps.subdir_by_name.find(consumer);
    if (sub_it == maps.subdir_by_name.end()) {
      ip = lparen + 1;
      continue;
    }
    std::vector<std::string> path_toks;
    collect_target_include_path_tokens(parts, path_toks);
    for (std::string tok : path_toks) {
      expand_static_cmake_path_vars(tok, cmake_dir, project_source_root);
      if (tok.find('$') != std::string::npos)
        continue;
      if (project_import::plausible_cmake_target_name_token(tok) && tok.find('/') == std::string::npos &&
          tok.find('\\') == std::string::npos && tok.find('.') == std::string::npos) {
        // Single identifier without path separators: could be an INTERFACE target (skip).
        auto maybe_tgt = maps.by_name.find(project_import::sanitize_id(tok));
        if (maybe_tgt != maps.by_name.end())
          continue;
      }
      std::filesystem::path rel(tok);
      std::filesystem::path abs_p = rel.is_absolute() ? rel : (cmake_dir / rel);
      std::error_code ecp;
      abs_p = std::filesystem::weakly_canonical(abs_p, ecp);
      if (ecp)
        abs_p = std::filesystem::absolute(abs_p);
      if (!std::filesystem::exists(abs_p))
        continue;
      project_import::add_target_include_dir(*consumer_td, write_root, sub_it->second, abs_p, warnings);
    }
    ip = lparen + 1;
  }

  for (const auto& child : collect_subdirectory_cmakes(cmake_dir, text))
    import_cmake_static_graph_recursive(child, write_root, project_source_root, maps, warnings, error, seen_cmake_files,
                                        depth + 1);
}

void import_cmake_apply_static_target_graph(const std::filesystem::path& top_cmake_file,
                                            const std::filesystem::path& write_root, ImportedPackage& out,
                                            std::vector<std::string>& warnings, std::string& error) {
  if (out.targets.empty())
    return;
  CmakeStaticGraphMaps maps;
  for (auto& pr : out.targets) {
    maps.by_name[pr.second.name] = &pr.second;
    maps.subdir_by_name[pr.second.name] = pr.first;
  }
  std::set<std::string> seen;
  const std::filesystem::path project_root = top_cmake_file.parent_path();
  import_cmake_static_graph_recursive(top_cmake_file, write_root, project_root, maps, warnings, error, seen, 0);
  error.clear();
}

void import_cmake_find_packages_recursive(const std::filesystem::path& cmake_file, std::set<std::string>& seen_cmake_files,
                                         std::map<std::string, bool>& deps, std::string& error, int depth) {
  if (depth > 16)
    return;
  std::error_code ec;
  const std::filesystem::path canon = std::filesystem::weakly_canonical(cmake_file, ec);
  const std::string canon_s = project_import::posix_str(canon);
  if (seen_cmake_files.count(canon_s))
    return;
  seen_cmake_files.insert(canon_s);

  const std::string text = project_import::read_file_text(cmake_file, error);
  if (!error.empty())
    return;
  collect_find_package_names(text, deps);
  for (const auto& child : collect_subdirectory_cmakes(cmake_file.parent_path(), text))
    import_cmake_find_packages_recursive(child, seen_cmake_files, deps, error, depth + 1);
}

void import_cmake_recursive(const std::filesystem::path& cmake_file, const std::filesystem::path& write_root,
                            ImportedPackage& out, std::vector<std::string>& warnings, std::string& error,
                            std::map<std::string, std::vector<std::filesystem::path>>& var_paths,
                            std::set<std::string>& seen_cmake_files, std::set<std::string>& seen_target_names,
                            std::map<std::string, int>& bucket_claims, int depth) {
  if (depth > 16)
    return;
  std::error_code ec;
  const std::filesystem::path canon = std::filesystem::weakly_canonical(cmake_file, ec);
  const std::string canon_s = project_import::posix_str(canon);
  if (seen_cmake_files.count(canon_s))
    return;
  seen_cmake_files.insert(canon_s);

  const std::string text = project_import::read_file_text(cmake_file, error);
  if (!error.empty())
    return;

  const std::filesystem::path cmake_dir = cmake_file.parent_path();
  collect_set_source_lists(cmake_dir, text, var_paths);

  const char* kws[] = {"add_executable", "add_library"};
  for (const char* key : kws) {
    size_t p = 0;
    const std::string needle = std::string(key) + "(";
    while ((p = text.find(needle, p)) != std::string::npos) {
      if (p > 0 && (std::isalnum(static_cast<unsigned char>(text[p - 1])) || text[p - 1] == '_')) {
        ++p;
        continue;
      }
      const size_t lparen = p + needle.size() - 1;
      auto inner_opt = balanced_paren_content(text, lparen);
      if (!inner_opt) {
        p = lparen + 1;
        continue;
      }
      std::vector<std::string> toks;
      split_cmake_args(*inner_opt, toks);
      if (toks.empty()) {
        p = lparen + 1;
        continue;
      }
      strip_quotes(toks[0]);
      if (!project_import::plausible_cmake_target_name_token(toks[0])) {
        p = lparen + 1;
        continue;
      }
      const std::string tname = project_import::sanitize_id(toks[0]);
      if (seen_target_names.count(tname)) {
        p = lparen + 1;
        continue;
      }

      if (std::string(key) == "add_library") {
        bool skip = false;
        for (const auto& t : toks) {
          std::string u = t;
          for (char& c : u)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
          if (u == "INTERFACE" || u == "IMPORTED" || u == "ALIAS" || u == "OBJECT")
            skip = true;
        }
        if (skip) {
          p = lparen + 1;
          continue;
        }
      }

      std::vector<std::string> rest(toks.begin() + 1, toks.end());
      rest.erase(std::remove_if(rest.begin(), rest.end(), is_cmake_keyword_skip), rest.end());

      std::vector<std::filesystem::path> abs;
      for (const auto& t : rest)
        append_resolved_paths(cmake_dir, t, var_paths, abs);
      std::sort(abs.begin(), abs.end());
      abs.erase(std::unique(abs.begin(), abs.end()), abs.end());

      std::string ty;
      if (std::string(key) == "add_executable")
        ty = "executable";
      else
        ty = map_lib_type(toks);

      const std::string bucket =
          project_import::resolve_target_xml_bucket(write_root, cmake_dir, tname, abs, bucket_claims);
      project_import::push_target(out, write_root, bucket, tname, ty, abs, warnings);
      if (!abs.empty())
        seen_target_names.insert(tname);
      p = lparen + 1;
    }
  }

  for (const auto& child : collect_subdirectory_cmakes(cmake_dir, text))
    import_cmake_recursive(child, write_root, out, warnings, error, var_paths, seen_cmake_files, seen_target_names,
                           bucket_claims, depth + 1);
}

std::optional<std::string> json_string_field(const std::string& blob, const char* key) {
  const std::string k = std::string("\"") + key + "\"";
  size_t pos = 0;
  while ((pos = blob.find(k, pos)) != std::string::npos) {
    if (pos > 0 && (std::isalnum(static_cast<unsigned char>(blob[pos - 1])) || blob[pos - 1] == '_')) {
      pos += k.size();
      continue;
    }
    pos = blob.find(':', pos + k.size());
    if (pos == std::string::npos)
      return std::nullopt;
    ++pos;
    while (pos < blob.size() && std::isspace(static_cast<unsigned char>(blob[pos])))
      ++pos;
    if (pos >= blob.size() || blob[pos] != '"')
      return std::nullopt;
    ++pos;
    std::string out;
    while (pos < blob.size()) {
      const char c = blob[pos];
      if (c == '"')
        return out;
      if (c == '\\' && pos + 1 < blob.size()) {
        out += blob[pos + 1];
        pos += 2;
        continue;
      }
      out += c;
      ++pos;
    }
    return std::nullopt;
  }
  return std::nullopt;
}

bool json_bool_field_default_false(const std::string& blob, const char* key) {
  const std::string k = std::string("\"") + key + "\"";
  size_t pos = blob.find(k);
  if (pos == std::string::npos)
    return false;
  pos = blob.find(':', pos + k.size());
  if (pos == std::string::npos)
    return false;
  ++pos;
  while (pos < blob.size() && std::isspace(static_cast<unsigned char>(blob[pos])))
    ++pos;
  if (pos + 4 <= blob.size() && blob.compare(pos, 4, "true") == 0)
    return true;
  return false;
}

bool run_cmake_configure_for_file_api(const std::filesystem::path& source_dir, const std::filesystem::path& build_dir,
                                      std::string& error) {
  std::error_code ec;
  // CMake File API: owned stateless query is an empty file at
  // `.cmake/api/v1/query/client-<id>/codemodel-v<major>` (not `client-*.json` in `query/`).
  const std::filesystem::path query_dir = build_dir / ".cmake" / "api" / "v1" / "query" / "client-up-0";
  std::filesystem::create_directories(query_dir, ec);
  if (ec) {
    error = "cannot create cmake file-api query directory: " + ec.message();
    return false;
  }
  {
    const std::filesystem::path marker = query_dir / "codemodel-v2";
    std::ofstream out(marker, std::ios::binary);
    if (!out) {
      error = "cannot write cmake file-api codemodel query marker";
      return false;
    }
  }

  const std::string s = to_posix_path_string(std::filesystem::absolute(source_dir));
  const std::string b = to_posix_path_string(std::filesystem::absolute(build_dir));
#if defined(_WIN32)
  const std::string cmd = "cmake -S \"" + s + "\" -B \"" + b + "\"";
#else
  const std::string cmd = "cmake -S \"" + s + "\" -B \"" + b + "\"";
#endif
  std::cout << "project: [cmake] file_api: running `cmake` configure for File API (source=" << s << " build=" << b << ")\n"
            << std::flush;
  if (cli_verbose())
    std::cerr << "project: [cmake] [verbose] exec: " << cmd << "\n" << std::flush;
  const int code = std::system(cmd.c_str());
  if (code != 0) {
    error = "cmake configure failed (exit " + std::to_string(code) + "); is `cmake` on PATH? command: " + cmd;
    return false;
  }
  return true;
}

bool file_api_query_impl(const std::filesystem::path& source_dir, const std::filesystem::path& build_dir,
                         ImportedPackage& out, std::string& error) {
  out.targets.clear();
  out.warnings.clear();
  {
    std::error_code ec_path;
    const auto abs_src = std::filesystem::absolute(source_dir, ec_path);
    const auto abs_bd = std::filesystem::absolute(build_dir, ec_path);
    if (ec_path) {
      error = "cannot resolve CMake file-api paths: " + ec_path.message();
      return false;
    }
    const auto can_src = std::filesystem::weakly_canonical(abs_src, ec_path);
    const auto can_bd = std::filesystem::weakly_canonical(abs_bd, ec_path);
    if (!ec_path && can_src == can_bd) {
      error =
          "CMake file-api query build directory must not be the same as the CMake source directory "
          "(refusing to wipe the source tree). Use a dedicated build directory (default: "
          "<write_root>/.up/cmake_file_api_query or --cmake-query-build-dir).";
      return false;
    }
  }
  std::error_code ec;
  std::filesystem::remove_all(build_dir, ec);
  ec.clear();
  std::filesystem::create_directories(build_dir, ec);
  if (ec) {
    error = "cannot prepare cmake query build directory: " + ec.message();
    return false;
  }
  if (!run_cmake_configure_for_file_api(source_dir, build_dir, error))
    return false;

  const std::filesystem::path reply = build_dir / ".cmake" / "api" / "v1" / "reply";
  if (!std::filesystem::is_directory(reply, ec)) {
    error = "cmake file-api reply directory missing: " + to_posix_path_string(reply);
    return false;
  }

  std::set<std::string> emitted;
  std::set<std::string> seen_lib_names;
  const std::string iface;
  const std::string def_runtime = "bin";
  const std::string def_archive = "lib";
  const std::string def_library = "lib";
  size_t skipped = 0;

  for (const auto& ent : std::filesystem::directory_iterator(reply, ec)) {
    if (ec)
      break;
    if (!ent.is_regular_file(ec))
      continue;
    const std::string fn = ent.path().filename().string();
    if (fn.size() < 12 || fn.rfind(".json") != fn.size() - 5)
      continue;
    if (fn.rfind("target-", 0) != 0)
      continue;
    std::string blob = project_import::read_file_text(ent.path(), error);
    if (!error.empty())
      return false;
    if (json_bool_field_default_false(blob, "imported")) {
      ++skipped;
      continue;
    }
    const auto name_opt = json_string_field(blob, "name");
    const auto type_opt = json_string_field(blob, "type");
    if (!name_opt.has_value() || !type_opt.has_value())
      continue;
    const std::string name_raw = *name_opt;
    const std::string type_raw = *type_opt;
    if (!project_import::plausible_cmake_target_name_token(name_raw))
      continue;
    const std::string name = project_import::sanitize_id(name_raw);
    if (name.empty())
      continue;
    std::string type_u = type_raw;
    for (char& c : type_u)
      c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    if (type_u == "INTERFACE_LIBRARY" || type_u == "OBJECT_LIBRARY" || type_u == "UTILITY" || type_u == "ALIAS_LIBRARY")
      continue;
    if (type_u == "EXECUTABLE") {
      ++skipped;
      continue;
    }

    CmakeLibraryInfo lib;
    lib.name = name;
    if (type_u == "STATIC_LIBRARY")
      lib.kind = CmakeLibraryInfo::Kind::StaticOnly;
    else if (type_u == "SHARED_LIBRARY" || type_u == "MODULE_LIBRARY")
      lib.kind = CmakeLibraryInfo::Kind::SharedOnly;
    else if (type_u == "UNKNOWN_LIBRARY")
      lib.kind = CmakeLibraryInfo::Kind::Unknown;
    else {
      ++skipped;
      continue;
    }

    if (!seen_lib_names.insert(name).second)
      continue;
    push_installed_wrappers_for_lib(lib, def_runtime, def_archive, def_library, iface, emitted, out);
  }

  if (skipped > 0)
    out.warnings.push_back("CMake file-api: skipped " + std::to_string(skipped) +
                           " non-library / imported / unsupported codemodel targets.");
  if (out.targets.empty()) {
    error = "cmake file-api codemodel produced no usable STATIC_LIBRARY/SHARED_LIBRARY targets under " +
           to_posix_path_string(reply);
    return false;
  }
  out.warnings.push_back(
      "CMake file-api: target list comes from a real `cmake` configure; imported_installed_* artifact paths are "
      "still heuristics under CMAKE_INSTALL_PREFIX — verify after install.");
  error.clear();
  return true;
}

}  // namespace

bool import_cmake_targets_from_file_api_query(const std::filesystem::path& source_dir,
                                              const std::filesystem::path& query_build_dir, ImportedPackage& out,
                                              std::string& error) {
  return file_api_query_impl(source_dir, query_build_dir, out, error);
}

void import_cmake_file(const std::filesystem::path& cmake_file, const std::filesystem::path& write_root,
                       ImportedPackage& out, std::vector<std::string>& warnings, std::string& error) {
  std::map<std::string, std::vector<std::filesystem::path>> var_paths;
  std::set<std::string> seen_cmake_files;
  std::set<std::string> seen_target_names;
  std::map<std::string, int> bucket_claims;
  import_cmake_recursive(cmake_file, write_root, out, warnings, error, var_paths, seen_cmake_files, seen_target_names,
                         bucket_claims, 0);
  import_cmake_apply_static_target_graph(cmake_file, write_root, out, warnings, error);

  if (out.targets.empty()) {
    warnings.push_back("CMake: no add_executable/add_library with recognizable sources (trying source-tree fallback).");
    error.clear();
  } else
    error.clear();
}

void import_cmake_installed_wrappers(const std::filesystem::path& cmake_file, ImportedPackage& out,
                                     std::vector<std::string>& warnings, std::string& error) {
  std::set<std::string> seen_cmake_files;
  std::map<std::string, CmakeLibraryInfo> libs;
  std::set<std::string> exe_names;
  std::map<std::string, std::string> scalar_vars;
  std::vector<InstallTargetRule> install_rules;
  std::string include_dir;
  import_cmake_wrappers_recursive(cmake_file, seen_cmake_files, libs, exe_names, scalar_vars, install_rules, include_dir,
                                  error, 0);
  if (!error.empty())
    return;

  std::set<std::string> emitted;
  std::set<std::string> libs_emitted_via_install;
  for (const auto& rule : install_rules) {
    for (const auto& tgt : rule.targets) {
      const auto it = libs.find(tgt);
      if (it == libs.end())
        continue;
      const CmakeLibraryInfo& lib = it->second;
      const std::string runtime_dest = rule.runtime_dest.empty() ? "bin" : rule.runtime_dest;
      const std::string archive_dest = rule.archive_dest.empty() ? "lib" : rule.archive_dest;
      const std::string library_dest = rule.library_dest.empty() ? "lib" : rule.library_dest;
      const std::string iface_dir = !rule.include_dest.empty() ? rule.include_dest : include_dir;
      push_installed_wrappers_for_lib(lib, runtime_dest, archive_dest, library_dest, iface_dir, emitted, out);
      libs_emitted_via_install.insert(tgt);
    }
  }

  const std::string def_runtime = "bin";
  const std::string def_archive = "lib";
  const std::string def_library = "lib";
  const std::string iface_fallback = include_dir;
  size_t fallback_libs = 0;
  for (const auto& kv : libs) {
    if (libs_emitted_via_install.count(kv.first))
      continue;
    push_installed_wrappers_for_lib(kv.second, def_runtime, def_archive, def_library, iface_fallback, emitted, out);
    ++fallback_libs;
  }

  if (fallback_libs > 0) {
    warnings.push_back(
        "CMake: inferred imported_installed_* for " + std::to_string(fallback_libs) +
        " add_library target(s) without a matching install(TARGETS ...) rule; "
        "artifact paths assume lib/ and bin/ under CMAKE_INSTALL_PREFIX — verify after install.");
  }

  if (!exe_names.empty()) {
    warnings.push_back(
        "CMake: detected " + std::to_string(exe_names.size()) +
        " add_executable target(s); scaffold does not emit target.xml for executables yet "
        "(configure requires sources or install(RUNTIME) support).");
  }

  if (out.targets.empty())
    warnings.push_back("CMake: no add_library targets with emitted imported_installed_* wrappers (check install rules).");
  error.clear();
}

void import_cmake_find_package_deps(const std::filesystem::path& cmake_file,
                                    std::vector<std::pair<std::string, bool>>& out_deps, std::string& error) {
  std::set<std::string> seen_cmake_files;
  std::map<std::string, bool> deps;
  probe_find_package_dep_map(cmake_file, seen_cmake_files, deps, error, 0);
  if (!error.empty())
    return;
  normalize_find_package_dep_map(deps);
  emit_find_package_dep_list(deps, out_deps);
}

}  // namespace up
