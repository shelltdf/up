#pragma once

#include <map>
#include <string>
#include <vector>

#include <filesystem>

namespace up {

// Host OS token for UP_OS (windows | linux | darwin).
std::string builtin_host_os();

// Builtins: UP_OS, UP_PACKAGE_NAME, UP_PACKAGE_VERSION, UP_TARGET_NAME, UP_TARGET_BUILD_SYSTEM, UP_CONFIG.
std::map<std::string, std::string> make_builtin_var_map(const std::string& package_name, const std::string& package_version,
                                                        const std::string& target_name, const std::string& build_system,
                                                        const std::string& config_mode);

// Merge: builtins, then package_vars, then target_vars, then opts (configure `--opt` / up_cache); last writer wins.
std::map<std::string, std::string> merge_var_layers(const std::map<std::string, std::string>& builtins,
                                                   const std::map<std::string, std::string>& opts,
                                                   const std::vector<std::pair<std::string, std::string>>& package_vars,
                                                   const std::vector<std::pair<std::string, std::string>>& target_vars);

// Replace @KEY@ using map; unknown keys left unchanged.
std::string substitute_at_vars(const std::string& text, const std::map<std::string, std::string>& vars);

// Replace `${KEY}` (CMake configure_file style) using the same map; unknown keys left unchanged. `KEY` must be a C
// identifier. Does not interpret `$<...>` generator expressions.
std::string substitute_dollar_brace_vars(const std::string& text, const std::map<std::string, std::string>& vars);

// After `@KEY@` substitution: expand CMake-style `#cmakedefine` / `#cmakedefine01` lines using the same `vars` map
// (subset of CMake `configure_file`; unknown macros => false / #undef).
std::string apply_cmakedefine_directives(const std::string& text, const std::map<std::string, std::string>& vars);

// Empty or whitespace => true. Supports KEY==value, KEY!=value (ASCII case-insensitive for comparison), or true/false.
bool eval_when(const std::string& when, const std::map<std::string, std::string>& vars, std::string& error_out);

// Reject absolute paths and `..` segments for config_file `to=` output.
bool is_safe_relative_config_output(const std::filesystem::path& rel);

}  // namespace up
