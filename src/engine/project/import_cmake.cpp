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
  const std::string needle = "add_subdirectory(";
  size_t pos = 0;
  while ((pos = text.find(needle, pos)) != std::string::npos) {
    if (pos > 0 && (std::isalnum(static_cast<unsigned char>(text[pos - 1])) || text[pos - 1] == '_')) {
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

}  // namespace up
