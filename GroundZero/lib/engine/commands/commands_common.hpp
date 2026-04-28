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

}  // namespace gz
