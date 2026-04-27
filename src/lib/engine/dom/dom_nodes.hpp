#pragma once

#include "dom_vars.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace up {

struct DomNode {
  DomNodeType type = DomNodeType::Global;
  std::string name;
  DomNode* parent = nullptr;
  std::vector<VarEntry> vars;
  std::vector<std::unique_ptr<DomNode>> children;

  virtual ~DomNode() = default;
};

struct PackageNode : DomNode {
  std::filesystem::path package_xml_path;
  std::string version = "0.0.0";
  std::vector<std::pair<std::string, bool>> dependencies;
  std::string external_cmake_source_dir;
  std::vector<std::string> config_files_in_to;  // in::to
  std::vector<std::string> defines;              // NAME or NAME=VALUE
};

struct TargetNode : DomNode {
  std::filesystem::path target_xml_path;
  std::string target_type;
  std::vector<std::pair<std::string, bool>> dependencies;  // name, optional
};

struct GlobalNode : DomNode {
  std::filesystem::path workspace_root;
};

}  // namespace up
