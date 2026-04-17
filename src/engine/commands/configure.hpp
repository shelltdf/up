#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace up {

// Writes backend files under .intermediate/build/<name> (default name: default) and prints a package/target tree.
// opt_kvs format: KEY=VALUE (typically UP_* options).
// build_dir_name_override: single path segment under .intermediate/build (not a full path).
int cmd_configure(const std::filesystem::path& cwd,
                  const std::vector<std::string>& scan_roots,
                  const std::vector<std::string>& opt_kvs = {},
                  const std::optional<std::string>& build_dir_name_override = std::nullopt);

}  // namespace up
