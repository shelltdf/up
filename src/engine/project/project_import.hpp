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
  // Each entry: subdirectory name (under package root) and target description; paths in desc are relative to that subdir.
  std::vector<std::pair<std::string, TargetDesc>> targets;
  std::vector<std::string> warnings;
};

ProbeResult probe_build_system(const std::filesystem::path& scan_root);

// Fills ImportedPackage from scan_root; paths in TargetDesc are relative to write_root/<subdir>/.
bool import_from_probe(const std::filesystem::path& scan_root, const std::filesystem::path& write_root, const ProbeResult& probe,
                       ImportedPackage& out, std::string& error);

}  // namespace up
