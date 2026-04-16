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
  struct IncludeEntry {
    std::string kind;  // dir | file | glob
    std::string from;  // relative to target.xml directory
    std::string to;    // relative to install include/
  };

  std::string name;
  std::string type;  // executable | static_library | shared_library
  std::vector<std::string> sources;
  std::vector<std::string> dependencies;  // package:target or target(same package)
  std::vector<IncludeEntry> includes;
};

// Minimal attribute scanner for root elements (no full XML parser dependency).
bool load_package_xml(const std::filesystem::path& path, PackageDesc& out, std::string& error);
bool load_target_xml(const std::filesystem::path& path, TargetDesc& out, std::string& error);

}  // namespace up
