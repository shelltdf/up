#pragma once

#include "dom_nodes.hpp"

#include <iosfwd>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace up {

struct BuildDomOptions {
  std::filesystem::path cwd;
  std::vector<std::filesystem::path> package_files;
  std::vector<std::filesystem::path> target_files;
};

class DomDocument {
public:
  DomDocument() = default;

  static bool build(const BuildDomOptions& options, DomDocument& out, std::string& error);

  const GlobalNode* global_root() const noexcept { return global_.get(); }
  GlobalNode* global_root() noexcept { return global_.get(); }

  using PackageIndex = std::map<std::string, PackageNode*>;
  const PackageIndex& packages_by_name() const noexcept { return package_by_name_; }

  bool write_xml(std::ostream& out) const;
  bool write_xml_file(const std::filesystem::path& path, std::string& error) const;
  bool write_json(std::ostream& out) const;
  bool write_json_file(const std::filesystem::path& path, std::string& error) const;
  void print_tree(std::ostream& out) const;

private:
  std::unique_ptr<GlobalNode> global_;
  PackageIndex package_by_name_;
};

}  // namespace up
