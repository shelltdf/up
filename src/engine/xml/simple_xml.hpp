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
  struct SourceEntry {
    std::string kind;  // file | glob
    std::string from;  // relative to target.xml directory
    std::string preprocess_command;
    std::string postprocess_command;
  };

  struct IncludeEntry {
    std::string kind;  // dir | file | glob
    std::string from;  // relative to target.xml directory
    std::string to;    // relative to install include/
    std::string preprocess_command;
    std::string postprocess_command;
  };

  struct AssetEntry {
    std::string kind;  // dir | file | glob
    std::string from;  // relative to target.xml directory
    std::string to;    // relative to install root
    std::string preprocess_command;
    std::string postprocess_command;
  };

  std::string name;
  std::string type;  // executable | static_library | shared_library | asset_bundle
  std::vector<std::string> sources;
  std::vector<SourceEntry> source_entries;
  std::vector<std::string> dependencies;  // package:target or target(same package)
  std::vector<IncludeEntry> includes;
  std::vector<AssetEntry> assets;
};

// Minimal attribute scanner for root elements (no full XML parser dependency).
bool load_package_xml(const std::filesystem::path& path, PackageDesc& out, std::string& error);
bool load_target_xml(const std::filesystem::path& path, TargetDesc& out, std::string& error);

}  // namespace up
