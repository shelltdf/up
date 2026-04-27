#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace gz {

// Structured entry (no argv): same semantics as CLI `gz configure`.
struct ConfigureRequest {
  std::filesystem::path cwd;
  std::vector<std::string> scan_roots;
  std::vector<std::string> opt_kvs;
  std::optional<std::string> build_dir_name_override;
};

int run_configure(const ConfigureRequest& req);

// Writes backend files under .intermediate/build/<name> (default name: default) and prints a package/target tree.
// opt_kvs format: KEY=VALUE (`GZ_*`, or any C-style identifier for `<vars>` overrides; see `gz spec`).
// build_dir_name_override: single path segment under .intermediate/build (not a full path).
int cmd_configure(const std::filesystem::path& cwd,
                  const std::vector<std::string>& scan_roots,
                  const std::vector<std::string>& opt_kvs = {},
                  const std::optional<std::string>& build_dir_name_override = std::nullopt);

}  // namespace gz
