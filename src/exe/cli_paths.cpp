#include "cli_paths.hpp"

#include "paths.hpp"

namespace up::exe {

bool intermediate_leaf_name_ok(const std::string& s) {
  if (s.empty() || s == "." || s == "..")
    return false;
  if (s.find("..") != std::string::npos)
    return false;
  for (unsigned char uc : s) {
    const char c = static_cast<char>(uc);
    if (c == '/' || c == '\\')
      return false;
#if defined(_WIN32)
    if (c == ':' || c == '<' || c == '>' || c == '|' || c == '?' || c == '*')
      return false;
#endif
  }
  return true;
}

std::filesystem::path build_dir_from_leaf(const std::filesystem::path& cwd, const std::string& leaf) {
  return up::default_build_root(cwd, "cmake") / std::filesystem::u8path(leaf);
}

std::filesystem::path install_dir_from_leaf(const std::filesystem::path& cwd, const std::string& leaf) {
  return up::default_install_root(cwd) / std::filesystem::u8path(leaf);
}

}  // namespace up::exe
