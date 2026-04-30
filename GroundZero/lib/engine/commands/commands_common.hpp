#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace gz {

/** True if `candidate` is the same path as `root` or strictly inside `root` (after weakly_canonical + absolute). */
bool gz_path_is_same_or_under(const std::filesystem::path& root, const std::filesystem::path& candidate);

/**
 * Drops any scan root that lies under `cwd/.intermediate` (canonical), with stderr warnings.
 * If that would empty the list, inserts `cwd` only and warns. Safe to call when `roots` already includes cwd.
 */
void gz_filter_scan_roots_skip_under_intermediate(const std::filesystem::path& cwd,
                                                  std::vector<std::filesystem::path>& roots,
                                                  const char* tool_label_for_warnings);
/** Remove scan roots that are the same as or a strict subdirectory of another root (avoids scanning e.g. libzip twice
 * when a parent 3rdparty is already a root). */
void gz_dedupe_scan_roots_subsumed(std::vector<std::filesystem::path>& roots);

/** True for keys allowed in the configure/build options map from `gz_cache.txt` / `--opt` (excludes structural lines). */
bool gz_mergeable_option_key(const std::string& k);

std::map<std::string, std::string> load_gz_options_from_build_dir(const std::filesystem::path& build_dir);
std::map<std::string, std::string> load_gz_options(const std::filesystem::path& root, const std::string& arch);
std::string read_plain_cache_value(const std::filesystem::path& cache_file, const std::string& key);
std::string option_or(const std::map<std::string, std::string>& opts, const std::string& key, const std::string& defv);
std::string option_or_compat(const std::map<std::string, std::string>& opts,
                             const std::string& preferred,
                             const std::string& legacy,
                             const std::string& defv);
bool equals_ci(std::string a, std::string b);
bool contains_ci(std::string haystack, std::string needle);
std::string arch_from_target_cpu(std::string v);
std::string lower_ascii(std::string v);
std::string arch_from_options(const std::map<std::string, std::string>& opts);

/** Parallel compile jobs for cmake --build / ninja (>=1). Honors GZ_BUILD_PARALLEL / GZ_BUILD_JOBS in opts, else logical cores (min 1, cap 512). */
unsigned parallel_jobs_for_build(const std::map<std::string, std::string>& opts);
/** Logical CPU count for default parallel jobs: OS API first (Win/Linux/mac), then hardware_concurrency(), else 4; clamped 1..512. */
unsigned default_parallel_jobs_hardware();
/** If no valid GZ_BUILD_PARALLEL / GZ_BUILD_JOBS in opts, sets GZ_BUILD_PARALLEL to default_parallel_jobs_hardware(). */
void ensure_default_build_parallel_options(std::map<std::string, std::string>& opts);

/**
 * `target.xml` <sources> token: path is created under the top generated CMake's `CMAKE_CURRENT_BINARY_DIR`
 * (e.g. configure-time `file(WRITE …)` from gz_reverse with <cmake_prelude>); not a path under the source tree.
 * Prefix must match gz_reverse_cmake output exactly.
 */
inline constexpr const char* gz_cmake_binary_dir_source_prefix() { return "__GZ_CMAKE_BINARY_DIR__/"; }
bool gz_source_path_is_cmake_binary_dir(const std::string& s);
/** Strips the prefix; returns empty if `s` is not a binary-dir source token. */
std::string gz_cmake_binary_dir_source_rel(const std::string& s);

/**
 * Recursively find `package.xml` / `target.xml` under `root`. Skips `.intermediate` and other bulky directory
 * names (see implementation) so large `--scan` trees do not block for minutes with no output.
 * Progress lines use `log_prefix` (e.g. `"configure"` or `"list"`).
 */
void collect_gz_desc_xml_files(const std::filesystem::path& root, std::vector<std::filesystem::path>& package_files,
                               std::vector<std::filesystem::path>& target_files, const char* log_prefix);

}  // namespace gz
