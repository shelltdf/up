#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace up {

struct PackageDesc {
  std::string name;
  std::string version;
  std::vector<std::pair<std::string, bool>> dependencies;  // name, optional
};

struct TargetDesc {
  std::string name;
  std::string type;  // executable | static_library | shared_library
  std::vector<std::string> sources;
  std::vector<std::string> dependencies;  // package:target or target(same package)
  std::vector<std::string> include_dirs;  // relative to target.xml directory
};

// Minimal attribute scanner for root elements (no full XML parser dependency).
bool load_package_xml(const std::filesystem::path& path, PackageDesc& out, std::string& error);
bool load_target_xml(const std::filesystem::path& path, TargetDesc& out, std::string& error);

}  // namespace up
