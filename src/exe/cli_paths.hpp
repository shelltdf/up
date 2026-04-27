#pragma once

#include <filesystem>
#include <string>

namespace up::exe {

// A single directory name under .intermediate/build or .intermediate/install (no path separators).
bool intermediate_leaf_name_ok(const std::string& s);

std::filesystem::path build_dir_from_leaf(const std::filesystem::path& cwd, const std::string& leaf);

std::filesystem::path install_dir_from_leaf(const std::filesystem::path& cwd, const std::string& leaf);

}  // namespace up::exe
