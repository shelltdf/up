#pragma once

#include "project_import.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace up {

void import_cmake_file(const std::filesystem::path& cmake_file, const std::filesystem::path& write_root,
                       ImportedPackage& out, std::vector<std::string>& warnings, std::string& error);

void import_cmake_installed_wrappers(const std::filesystem::path& cmake_file, ImportedPackage& out,
                                     std::vector<std::string>& warnings, std::string& error);

void import_autotools(const std::filesystem::path& scan_root, const std::filesystem::path& write_root, ImportedPackage& out,
                      std::vector<std::string>& warnings, std::string& error);

void import_qmake(const std::filesystem::path& pro_file, const std::filesystem::path& scan_root,
                  const std::filesystem::path& write_root, ImportedPackage& out, std::vector<std::string>& warnings,
                  std::string& error);

void import_meson_basic(const std::filesystem::path& scan_root, const std::filesystem::path& write_root, ImportedPackage& out,
                        std::vector<std::string>& warnings, std::string& error);

void import_source_fallback(const std::filesystem::path& scan_root, const std::filesystem::path& write_root,
                            ImportedPackage& out, std::vector<std::string>& warnings, std::string& error);

}  // namespace up
