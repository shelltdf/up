#pragma once

#include "project_import.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace up::project_import {

std::string read_file_text(const std::filesystem::path& path, std::string& error);
bool is_src_ext(const std::string& ext);
bool skip_dir_name(const std::string& name);
std::string sanitize_id(std::string s);
std::string posix_str(const std::filesystem::path& p);
std::optional<std::filesystem::path> try_relative(const std::filesystem::path& base, const std::filesystem::path& target);
bool looks_like_source_token(const std::string& t);
void push_target(ImportedPackage& out, const std::filesystem::path& write_root, const std::string& subdir,
                 const std::string& name, const std::string& type, const std::vector<std::filesystem::path>& abs_sources,
                 std::vector<std::string>& warnings);

}  // namespace up::project_import
