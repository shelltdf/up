#pragma once

#include "simple_xml.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace up {

enum class BuildProbeKind {
  CMake,
  Autotools,
  QMake,
  Meson,
  SourceTreeFallback
};

struct ProbeResult {
  BuildProbeKind kind = BuildProbeKind::SourceTreeFallback;
  std::filesystem::path anchor;  // e.g. CMakeLists.txt or Makefile.am
};

struct ImportedPackage {
  std::string package_name;
  // Each entry: path under write_root (posix, may contain '/') where target.xml lives, plus TargetDesc whose paths are
  // relative to write_root / that path.
  std::vector<std::pair<std::string, TargetDesc>> targets;
  std::vector<std::string> warnings;
};

ProbeResult probe_build_system(const std::filesystem::path& scan_root);

// Fills ImportedPackage from scan_root; paths in TargetDesc are relative to write_root/<subdir>/.
bool import_from_probe(const std::filesystem::path& scan_root, const std::filesystem::path& write_root, const ProbeResult& probe,
                       ImportedPackage& out, std::string& error);

// Parse CMake install(TARGETS ...) and emit imported_installed_* wrapper targets.
bool import_cmake_installed_from_probe(const std::filesystem::path& cmake_file, ImportedPackage& out, std::string& error);

// Run `cmake -S <source_dir> -B <query_build_dir>` with CMake File API (codemodel v2) and emit
// imported_installed_* wrappers from real codemodel targets (requires a working `cmake` on PATH).
bool import_cmake_targets_from_file_api_query(const std::filesystem::path& source_dir,
                                              const std::filesystem::path& query_build_dir, ImportedPackage& out,
                                              std::string& error);

// Parse CMake find_package(...) and return normalized package dependency names + required flag.
bool import_cmake_dependencies_from_probe(const std::filesystem::path& cmake_file,
                                          std::vector<std::pair<std::string, bool>>& deps, std::string& error);

}  // namespace up
