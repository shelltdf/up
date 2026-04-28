#pragma once

#include "simple_xml.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace gz {

struct GzRedistManifestDep {
  std::string name;
  bool optional = false;
};

struct GzRedistManifestTarget {
  std::string original_name;
  std::string emit_name;
  std::string emit_subdir;
  std::string emit_type;
  /** Prebuilt: relative to install root (POSIX). */
  std::string install_rel_import_lib;
  std::string install_rel_location;
  std::string install_rel_dll;
  /** imported_installed_*: `<install artifact=` / `implib=` (install-prefix-relative, POSIX). */
  bool use_installed_wrap = false;
  std::string install_rel_artifact;
  std::string install_rel_implib;
  std::string installed_iface_include;
  /** Values already remapped for `<dependency name="…"/>` in emitted target.xml */
  std::vector<std::string> dependency_names;
};

struct GzRedistManifest {
  int schema_version = 3;
  std::string package_name;
  std::string package_version;
  GzBinaryLayout layout;
  std::vector<GzRedistManifestDep> package_dependencies;
  std::vector<GzRedistManifestTarget> targets;
};

bool write_gz_redist_manifest_json(const std::filesystem::path& path, const GzRedistManifest& m, std::string& error);
bool read_gz_redist_manifest_json(const std::filesystem::path& path, GzRedistManifest& m, std::string& error);
/** Returns 0 on success, non-zero on failure (message in error). */
int emit_gz_redistribution_xml(const std::filesystem::path& install_root, const GzRedistManifest& m, std::string& error);

}  // namespace gz
