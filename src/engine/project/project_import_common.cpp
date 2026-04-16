#include "project_import_common.hpp"

#include "path_check.hpp"
#include "paths.hpp"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace up::project_import {

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

std::string sanitize_id(std::string s) {
  if (s.empty())
    s = "target";
  for (char& c : s) {
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_'))
      c = '_';
  }
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

}  // namespace up::project_import
