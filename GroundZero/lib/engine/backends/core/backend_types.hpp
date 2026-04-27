#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace gz {

struct BuildBackendContext {
  std::filesystem::path src_dir;
  std::filesystem::path bin_dir;
  std::filesystem::path install_dir;
  std::map<std::string, std::string> opts;
  std::string build_system;
  std::string cmake_generator;
  std::string config_name;
  bool multi_config = false;
  // Extra -DCMAKE_PREFIX_PATH=... for the top-level super-build (optional).
  std::string cmake_prefix_path;
};

struct TestBackendContext {
  std::filesystem::path build_bin_dir;
  std::filesystem::path install_bin_dir;
  std::string config_name;
  std::string test_name;
};

struct PackBackendContext {
  std::filesystem::path src_dir;
  std::filesystem::path dst_dir;
  std::string arch;
  std::string build_system;
  std::filesystem::path build_out_dir;
  std::string config_name;
  std::string pack_backend;
  bool allow_fallback = true;
};

struct ConfigureBackendContext {
  std::filesystem::path source_dir;
  std::filesystem::path out_dir;
  std::string cmake_generator;
  std::string config_name;
  bool multi_config = false;
};

struct ConfigureTargetModel {
  struct SourceRule {
    std::string path;
    std::string preprocess_command;
    std::string postprocess_command;
  };
  std::string name;
  std::string type;
  std::vector<std::string> source_paths;
  std::vector<SourceRule> source_rules;
  std::vector<std::string> include_dirs;
  /** CMake `NAME` or `NAME=value` items for target_compile_definitions / Ninja -D. */
  std::vector<std::string> compile_definitions;
  /** CMake `target_link_libraries`: target name + visibility keyword (private|public|interface). */
  std::vector<std::pair<std::string, std::string>> links;
  bool imported_prebuilt = false;
  // True: IMPORTED_* paths are ${CMAKE_INSTALL_PREFIX}/<rel> (installed artifact layout).
  bool imported_from_install_prefix = false;
  std::string install_rel_artifact;
  std::string install_rel_interface_include;
  // Windows shared IMPORTED: import library path relative to CMAKE_INSTALL_PREFIX.
  std::string install_rel_implib;
  // IMPORTED_LOCATION (shared: primary binary), IMPORTED_IMPLIB (Windows .lib)
  std::string imported_location;
  std::string imported_implib;
  std::string imported_dll;
};

struct ConfigureInstallDirRule {
  std::string src;
  std::string dst;
  std::string preprocess_command;
  std::string postprocess_command;
};

struct ConfigureInstallFileRule {
  std::string src;
  std::string dst;
  std::string preprocess_command;
  std::string postprocess_command;
};

struct ConfigureGraphModel {
  std::string build_system;
  std::string package_name;
  std::string config_mode;
  std::filesystem::path build_root;
  std::filesystem::path out_dir;
  std::filesystem::path install_root;
  /** Parallel jobs for aggregate `cmake --build` / ninja rules (from merged GZ_* options). */
  unsigned parallel_compile_jobs = 1;
  std::vector<ConfigureTargetModel> targets;
  std::vector<std::string> install_exe_names;
  std::vector<ConfigureInstallDirRule> install_dir_rules;
  std::vector<ConfigureInstallFileRule> install_file_rules;
  std::vector<ConfigureInstallDirRule> asset_dir_rules;
  std::vector<ConfigureInstallFileRule> asset_file_rules;
  std::string cmake_prefix_path;
  bool cmake_parent_multi_config = false;
};

}  // namespace gz
