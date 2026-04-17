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

}  // namespace up
