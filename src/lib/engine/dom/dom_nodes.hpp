#pragma once

#include "dom_vars.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace up {

class DomDocument;

class DomNode {
  friend class DomDocument;

public:
  virtual ~DomNode() = default;

  DomNodeType type() const noexcept { return type_; }
  const std::string& name() const noexcept { return name_; }
  DomNode* parent() const noexcept { return parent_; }
  const std::vector<VarEntry>& vars() const noexcept { return vars_; }
  const std::vector<std::unique_ptr<DomNode>>& children() const noexcept { return children_; }

protected:
  DomNode() = default;

private:
  DomNodeType type_ = DomNodeType::Global;
  std::string name_;
  DomNode* parent_ = nullptr;
  std::vector<VarEntry> vars_;
  std::vector<std::unique_ptr<DomNode>> children_;
};

class PackageNode : public DomNode {
  friend class DomDocument;

public:
  ~PackageNode() override = default;

  const std::filesystem::path& package_xml_path() const noexcept { return package_xml_path_; }
  const std::string& version() const noexcept { return version_; }
  const std::vector<std::pair<std::string, bool>>& dependencies() const noexcept { return dependencies_; }
  const std::vector<std::string>& config_files_in_to() const noexcept { return config_files_in_to_; }
  const std::vector<std::string>& defines() const noexcept { return defines_; }

private:
  std::filesystem::path package_xml_path_;
  std::string version_ = "0.0.0";
  std::vector<std::pair<std::string, bool>> dependencies_;
  std::vector<std::string> config_files_in_to_;
  std::vector<std::string> defines_;
};

class TargetNode : public DomNode {
  friend class DomDocument;

public:
  ~TargetNode() override = default;

  const std::filesystem::path& target_xml_path() const noexcept { return target_xml_path_; }
  const std::string& target_type() const noexcept { return target_type_; }
  const std::vector<std::pair<std::string, bool>>& dependencies() const noexcept { return dependencies_; }

private:
  std::filesystem::path target_xml_path_;
  std::string target_type_;
  std::vector<std::pair<std::string, bool>> dependencies_;
};

class GlobalNode : public DomNode {
  friend class DomDocument;

public:
  ~GlobalNode() override = default;

  const std::filesystem::path& workspace_root() const noexcept { return workspace_root_; }

private:
  std::filesystem::path workspace_root_;
};

}  // namespace up
