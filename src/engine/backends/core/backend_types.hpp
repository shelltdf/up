#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace up {

struct BuildBackendContext {
  std::filesystem::path src_dir;
  std::filesystem::path bin_dir;
  std::filesystem::path install_dir;
  std::map<std::string, std::string> opts;
  std::string build_system;
  std::string cmake_generator;
  std::string config_name;
  bool multi_config = false;
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
};

struct ConfigureBackendContext {
  std::filesystem::path source_dir;
  std::filesystem::path out_dir;
  std::string cmake_generator;
  std::string config_name;
  bool multi_config = false;
};

struct ConfigureTargetModel {
  std::string name;
  std::string type;
  std::vector<std::string> source_paths;
  std::vector<std::string> include_dirs;
  std::vector<std::string> links;
};

struct ConfigureInstallDirRule {
  std::string src;
  std::string dst;
};

struct ConfigureInstallFileRule {
  std::string src;
  std::string dst;
};

struct ConfigureGraphModel {
  std::string build_system;
  std::string package_name;
  std::string config_mode;
  std::filesystem::path build_root;
  std::filesystem::path out_dir;
  std::filesystem::path install_root;
  std::vector<ConfigureTargetModel> targets;
  std::vector<std::string> install_exe_names;
  std::vector<ConfigureInstallDirRule> install_dir_rules;
  std::vector<ConfigureInstallFileRule> install_file_rules;
};

}  // namespace up
