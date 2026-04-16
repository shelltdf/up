#pragma once

#include <filesystem>
#include <map>
#include <string>

namespace up {

std::map<std::string, std::string> load_up_options_from_build_dir(const std::filesystem::path& build_dir);
std::map<std::string, std::string> load_up_options(const std::filesystem::path& root, const std::string& arch);
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

}  // namespace up
