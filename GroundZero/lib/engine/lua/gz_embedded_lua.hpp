#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace gz {

/// Canonical (weakly) path prefixes allowed for `gz.file` read/write. Typically workspace,
/// package dir, build root, generated root.
struct GzLuaFileRoots {
  std::filesystem::path workspace;
  std::filesystem::path build_root;
  /// Optional: primary package source dir (e.g. parent of package.xml). Empty = skip
  std::filesystem::path package_dir;
  /// e.g. workspace/.intermediate/generated/<arch>/...
  std::filesystem::path generated_dir;
};

/// Run a Lua chunk at `gz configure` time: opens embedded Lua, registers `gz.file`, global `GZ`
/// context table, standard libraries (luaL_openlibs), then pcall. Returns empty string on success,
/// or error line on failure.
std::string run_gz_embedded_configure_lua(const std::string& lua_source, const GzLuaFileRoots& roots,
                                          const std::map<std::string, std::string>& extra_env_strings);

}  // namespace gz
