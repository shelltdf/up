#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace up {

// Writes build/<arch>/backend files and prints a simple package/target tree.
// opt_kvs format: KEY=VALUE (typically UP_* options).
int cmd_configure(const std::filesystem::path& cwd,
                  const std::vector<std::string>& scan_roots,
                  const std::vector<std::string>& opt_kvs = {});

}  // namespace up
