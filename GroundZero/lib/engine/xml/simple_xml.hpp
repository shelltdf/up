#pragma once

#include <filesystem>
#include <iosfwd>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace gz {

/** Optional metadata: which `gz` install tree / host ABI the prebuilt or installed layout matches. */
struct GzBinaryLayout {
  std::string os;
  std::string cpu;
  std::string build_system;
  std::string toolchain;
  std::string link;  // "static" | "dynamic" (from GZ_TARGET_DYNAMIC_LIBRARY)
  std::string config;  // "debug" | "release"
  std::string crt;  // e.g. "dynamic_md"; often empty on non-Windows
  /** Legacy read: deprecated monolithic `arch="..."`; used only to run `try_decompose_compose_arch_tag`, then cleared when possible. */
  std::string arch_legacy;
  bool empty() const {
    return os.empty() && cpu.empty() && build_system.empty() && toolchain.empty() && link.empty() && config.empty() &&
           crt.empty() && arch_legacy.empty();
  }
};

// Compiler macro definitions (package.xml / target.xml `<defines>`; CMake: target_compile_definitions; Ninja: -D /D).
struct DefineEntry {
  std::string name;
  std::string value;  // optional; empty => define name only (#ifdef NAME)
};

/** Template processed at `gz configure` into `.intermediate/generated/<arch 段>/` (package or target subtree; `arch` matches cache; see docs). */
struct ConfigFileEntry {
  std::string in;  // relative to package.xml parent (package) or target.xml directory (target)
  std::string to;  // relative under generated/<package>/_package/ (package) or generated/<package>/<target>/ (target)
};

// Script variable (`<var type="script" .../>`) with trigger + source text.
struct ScriptEntry {
  std::string name;
  std::string script_type = "lua";
  std::string trigger;
  std::string source;
};

struct PackageDesc {
  std::string name;
  std::string version;
  std::vector<std::pair<std::string, bool>> dependencies;  // name, optional
  /** `<vars><var name="KEY" value="VAL"/></vars>` — default for KEY; overridable via `gz configure --opt` / gz_cache (see merge order). */
  std::vector<std::pair<std::string, std::string>> vars;
  std::vector<ScriptEntry> scripts;
  /** `<defines>`; applied to every native compile target in this package (before each target's own `<defines>`). */
  std::vector<DefineEntry> defines;
  std::vector<ConfigFileEntry> config_files;
};

struct TargetDesc {
  struct SourceEntry {
    std::string kind;  // file | glob
    std::string from;  // relative to target.xml directory
    std::string preprocess_command;
    std::string postprocess_command;
    /** Optional: `when="GZ_OS==windows"` etc.; empty means always. */
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
    GzBinaryLayout layout;
  };

  std::string name;
  std::string type;  // executable | library | static_library | shared_library | asset_bundle |
                      // prebuilt_static_library | prebuilt_shared_library
  std::vector<std::string> sources;
  std::vector<SourceEntry> source_entries;
  std::optional<PrebuiltDesc> prebuilt;
  /** `<dependency name="..." visibility="private|public|interface"/>` — `name` is package:target or same-package target. */
  struct DependencyEntry {
    std::string name;
    std::string visibility;  // private | public | interface
  };
  std::vector<DependencyEntry> dependencies;
  /** Public headers: XML element `<headers>` (parsed into this list). */
  std::vector<IncludeEntry> includes;
  std::vector<AssetEntry> assets;
  std::vector<DefineEntry> defines;
  /** Target-level `<vars>` defaults (override package / builtins for same KEY; `--opt` / cache still wins last). */
  std::vector<std::pair<std::string, std::string>> vars;
  std::vector<ScriptEntry> scripts;
  std::vector<ConfigFileEntry> config_files;
  /** `<compile_flags><arg>…</arg>…` — one arg = one `target_compile_options` token; repeated blocks append. */
  std::vector<std::string> compile_flags;
  /** `<link_flags><arg>…</arg>…` — one arg = one `target_link_options` token. */
  std::vector<std::string> link_flags;
};

// Minimal attribute scanner for root elements (no full XML parser dependency).
bool load_package_xml(const std::filesystem::path& path, PackageDesc& out, std::string& error);
bool load_target_xml(const std::filesystem::path& path, TargetDesc& out, std::string& error);

bool write_package_xml(std::ostream& out, const PackageDesc& pkg);
bool write_target_xml(std::ostream& out, const TargetDesc& desc);

bool write_package_xml(const std::filesystem::path& path, const PackageDesc& pkg, std::string& error);
bool write_target_xml(const std::filesystem::path& path, const TargetDesc& desc, std::string& error);

}  // namespace gz
