#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace up {

// Structured entry (no argv): same semantics as CLI `up configure`.
struct ConfigureRequest {
  std::filesystem::path cwd;
  std::vector<std::string> scan_roots;
  std::vector<std::string> opt_kvs;
  std::optional<std::string> build_dir_name_override;
};

int run_configure(const ConfigureRequest& req);

// Writes backend files under .intermediate/build/<name> (default name: default) and prints a package/target tree.
// opt_kvs format: KEY=VALUE (`UP_*` / `UPSTREAM_*`, or any C-style identifier for `<vars>` overrides; see `up spec`).
// build_dir_name_override: single path segment under .intermediate/build (not a full path).
int cmd_configure(const std::filesystem::path& cwd,
                  const std::vector<std::string>& scan_roots,
                  const std::vector<std::string>& opt_kvs = {},
                  const std::optional<std::string>& build_dir_name_override = std::nullopt);

}  // namespace up
