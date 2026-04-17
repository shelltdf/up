#include "project_import_internal.hpp"
#include "project_import_common.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
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

std::string lower_ascii(std::string s) {
  for (char& c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
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
      if (!tok.empty() && tok[0] != '$')
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
  collect_install_target_rules(text, scalar_vars, install_rules);
  if (include_dir.empty()) {
    collect_install_directory_include(text, scalar_vars, include_dir);
    if (include_dir.empty())
      collect_install_files_include(text, scalar_vars, include_dir);
  }

  for (const auto& child : collect_subdirectory_cmakes(cmake_file.parent_path(), text))
    import_cmake_wrappers_recursive(child, seen_cmake_files, libs, scalar_vars, install_rules, include_dir, error,
                                   depth + 1);
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
        pkg = lower_ascii(project_import::sanitize_id(pkg));
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

}  // namespace

void import_cmake_file(const std::filesystem::path& cmake_file, const std::filesystem::path& write_root,
                       ImportedPackage& out, std::vector<std::string>& warnings, std::string& error) {
  std::map<std::string, std::vector<std::filesystem::path>> var_paths;
  std::set<std::string> seen_cmake_files;
  std::set<std::string> seen_target_names;
  std::map<std::string, int> bucket_claims;
  import_cmake_recursive(cmake_file, write_root, out, warnings, error, var_paths, seen_cmake_files, seen_target_names,
                         bucket_claims, 0);

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
  std::map<std::string, std::string> scalar_vars;
  std::vector<InstallTargetRule> install_rules;
  std::string include_dir;
  import_cmake_wrappers_recursive(cmake_file, seen_cmake_files, libs, scalar_vars, install_rules, include_dir, error,
                                  0);
  if (!error.empty())
    return;

  std::set<std::string> emitted;
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
      const bool build_static = (lib.kind == CmakeLibraryInfo::Kind::StaticOnly || lib.kind == CmakeLibraryInfo::Kind::Unknown);
      const bool build_shared = (lib.kind == CmakeLibraryInfo::Kind::SharedOnly || lib.kind == CmakeLibraryInfo::Kind::Unknown);

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
  }

  if (out.targets.empty())
    warnings.push_back("CMake: no install(TARGETS ...) library rules resolved for imported_installed_* wrappers.");
  error.clear();
}

void import_cmake_find_package_deps(const std::filesystem::path& cmake_file,
                                    std::vector<std::pair<std::string, bool>>& out_deps, std::string& error) {
  std::set<std::string> seen_cmake_files;
  std::map<std::string, bool> deps;
  import_cmake_find_packages_recursive(cmake_file, seen_cmake_files, deps, error, 0);
  if (!error.empty())
    return;
  out_deps.clear();
  for (const auto& kv : deps)
    out_deps.push_back(kv);
}

}  // namespace up
