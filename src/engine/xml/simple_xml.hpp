#pragma once

#include <filesystem>
#include <iosfwd>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace up {

// Compiler macro definitions (package.xml / target.xml `<defines>`; CMake: target_compile_definitions; Ninja: -D /D).
struct DefineEntry {
  std::string name;
  std::string value;  // optional; empty => define name only (#ifdef NAME)
};

// Optional native CMake subtree (see package.xml <cmake/>).
struct PackageExternalCmake {
  // Directory containing upstream CMakeLists.txt, relative to package.xml parent directory.
  std::string source_dir;
};

struct PackageDesc {
  std::string name;
  std::string version;
  std::vector<std::pair<std::string, bool>> dependencies;  // name, optional
  std::optional<PackageExternalCmake> external_cmake;
  /** `<vars><var name="KEY" value="VAL"/></vars>` — default for KEY; overridable via `up configure --opt` / up_cache (see merge order). */
  std::vector<std::pair<std::string, std::string>> vars;
  /** `<defines>`; applied to every native compile target in this package (before each target's own `<defines>`). */
  std::vector<DefineEntry> defines;
};

struct TargetDesc {
  struct SourceEntry {
    std::string kind;  // file | glob
    std::string from;  // relative to target.xml directory
    std::string preprocess_command;
    std::string postprocess_command;
    /** Optional: `when="UP_OS==windows"` etc.; empty means always. */
    std::string when;
  };

  struct IncludeEntry {
    std::string kind;  // dir | file | glob
    std::string from;  // relative to target.xml directory
    std::string to;    // relative to install include/
    std::string preprocess_command;
    std::string postprocess_command;
    std::string when;
  };

  /** Template processed at `up configure` into `.intermediate/generated/...` (see docs). */
  struct ConfigFileEntry {
    std::string in;   // relative to target.xml directory
    std::string to;   // relative path under generated/<package>/<target>/
  };

  struct AssetEntry {
    std::string kind;  // dir | file | glob
    std::string from;  // relative to target.xml directory
    std::string to;    // relative to install root
    std::string preprocess_command;
    std::string postprocess_command;
  };

  // Prebuilt SDK / binary-only library (paths relative to target.xml directory unless absolute).
  struct PrebuiltDesc {
    // STATIC IMPORTED: path to .lib / .a. SHARED (Windows): implib .lib if dll is set.
    std::string import_lib;
    // SHARED: primary binary (.dll / .so / .dylib). Optional if import_lib alone is enough (STATIC).
    std::string location;
    std::string dll;
  };

  // After <cmake> subtree installs to CMAKE_INSTALL_PREFIX, wrap an artifact path under that prefix.
  struct InstalledWrapDesc {
    std::string artifact;           // e.g. lib/foo.lib — relative to CMAKE_INSTALL_PREFIX
    std::string interface_include;  // e.g. include — optional, INTERFACE include dir under prefix
    std::string implib;             // Windows shared: import .lib (relative to prefix)
  };

  std::string name;
  std::string type;  // executable | static_library | shared_library | asset_bundle |
                      // imported_static_library | imported_shared_library |
                      // imported_installed_static_library | imported_installed_shared_library
  std::vector<std::string> sources;
  std::vector<SourceEntry> source_entries;
  std::optional<PrebuiltDesc> prebuilt;
  std::optional<InstalledWrapDesc> installed_wrap;
  std::vector<std::string> dependencies;  // package:target or target(same package)
  /** Public headers: XML element `<headers>` (parsed into this list). */
  std::vector<IncludeEntry> includes;
  std::vector<AssetEntry> assets;
  std::vector<DefineEntry> defines;
  /** Target-level `<vars>` defaults (override package / builtins for same KEY; `--opt` / cache still wins last). */
  std::vector<std::pair<std::string, std::string>> vars;
  std::vector<ConfigFileEntry> config_files;
};

// Minimal attribute scanner for root elements (no full XML parser dependency).
bool load_package_xml(const std::filesystem::path& path, PackageDesc& out, std::string& error);
bool load_target_xml(const std::filesystem::path& path, TargetDesc& out, std::string& error);

bool write_package_xml(std::ostream& out, const PackageDesc& pkg);
bool write_target_xml(std::ostream& out, const TargetDesc& desc);

bool write_package_xml(const std::filesystem::path& path, const PackageDesc& pkg, std::string& error);
bool write_target_xml(const std::filesystem::path& path, const TargetDesc& desc, std::string& error);

}  // namespace up
