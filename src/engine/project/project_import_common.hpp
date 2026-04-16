#pragma once

#include "project_import.hpp"

#include <filesystem>
#include <map>
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

// Directory under write_root where target.xml will live: prefer relpath(write_root, preferred_dir_abs); if sources
// cannot be expressed relative to that directory, or the slot is already taken, use ".targets/<sanitized_name>".
// bucket_claims keys: "." for package root, else posix relpath including ".targets/foo".
std::string resolve_target_xml_bucket(const std::filesystem::path& write_root,
                                      const std::filesystem::path& preferred_dir_abs, const std::string& target_name,
                                      const std::vector<std::filesystem::path>& abs_sources,
                                      std::map<std::string, int>& bucket_claims);

void push_target(ImportedPackage& out, const std::filesystem::path& write_root, const std::string& subdir,
                 const std::string& name, const std::string& type, const std::vector<std::filesystem::path>& abs_sources,
                 std::vector<std::string>& warnings);

}  // namespace up::project_import
