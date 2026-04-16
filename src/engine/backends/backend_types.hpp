#pragma once

#include <filesystem>
#include <map>
#include <string>

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

}  // namespace up
