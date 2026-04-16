#include "project_import_internal.hpp"
#include "project_import_common.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
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

}  // namespace

void import_cmake_file(const std::filesystem::path& cmake_file, const std::filesystem::path& write_root,
                       ImportedPackage& out, std::vector<std::string>& warnings, std::string& error) {
  const std::string text = project_import::read_file_text(cmake_file, error);
  if (!error.empty())
    return;

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
      std::vector<std::string> clean;
      for (auto& t : toks) {
        if (t.find('$') == std::string::npos)
          clean.push_back(std::move(t));
      }
      toks = std::move(clean);
      if (toks.empty()) {
        p = lparen + 1;
        continue;
      }
      const std::string tname = project_import::sanitize_id(toks[0]);
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
      for (const auto& t : rest) {
        if (!project_import::looks_like_source_token(t))
          continue;
        std::filesystem::path rel = std::filesystem::path(t);
        std::filesystem::path abs_p = rel.is_absolute() ? rel : (cmake_file.parent_path() / rel);
        std::error_code ec;
        abs_p = std::filesystem::weakly_canonical(abs_p, ec);
        if (ec || !std::filesystem::exists(abs_p))
          continue;
        abs.push_back(abs_p);
      }

      std::string ty;
      if (std::string(key) == "add_executable")
        ty = "executable";
      else
        ty = map_lib_type(toks);

      project_import::push_target(out, write_root, tname, tname, ty, abs, warnings);
      p = lparen + 1;
    }
  }

  if (out.targets.empty()) {
    warnings.push_back("CMake: no add_executable/add_library with recognizable sources (trying source-tree fallback).");
    error.clear();
  } else
    error.clear();
}

}  // namespace up
