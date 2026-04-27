#include "dom_document.hpp"

#include "dom_node_visitor.hpp"
#include "paths.hpp"
#include "simple_xml.hpp"

#include <algorithm>
#include <fstream>
#include <map>
#include <ostream>

namespace gz {

bool DomDocument::build(const BuildDomOptions& options, DomDocument& out, std::string& error) {
  out = DomDocument{};
  auto root = std::make_unique<GlobalNode>();
  root->type_ = DomNodeType::Global;
  root->name_ = "global";
  root->workspace_root_ = options.cwd;

  std::map<std::filesystem::path, PackageNode*> package_by_dir;
  for (const auto& pkg_path : options.package_files) {
    PackageDesc pkg;
    std::string err;
    if (!load_package_xml(pkg_path, pkg, err)) {
      error = to_posix_path_string(pkg_path) + ": " + err;
      return false;
    }
    auto node = std::make_unique<PackageNode>();
    node->type_ = DomNodeType::Package;
    node->name_ = pkg.name;
    node->version_ = pkg.version;
    node->package_xml_path_ = pkg_path;
    node->dependencies_ = pkg.dependencies;
    for (const auto& kv : pkg.vars) {
      VarEntry e;
      e.name = kv.first;
      e.value.type = VarValueType::Scalar;
      e.value.value = kv.second;
      node->vars_.push_back(std::move(e));
    }
    for (const auto& sv : pkg.scripts) {
      VarEntry e;
      e.name = sv.name;
      e.value.type = VarValueType::Script;
      e.value.script.script_type = sv.script_type.empty() ? "lua" : sv.script_type;
      e.value.script.trigger = sv.trigger;
      e.value.script.source = sv.source;
      node->vars_.push_back(std::move(e));
    }
    for (const auto& d : pkg.defines)
      node->defines_.push_back(d.value.empty() ? d.name : (d.name + "=" + d.value));
    for (const auto& cf : pkg.config_files)
      node->config_files_in_to_.push_back(cf.in + "::" + cf.to);

    package_by_dir[pkg_path.parent_path()] = node.get();
    out.package_by_name_[node->name_] = node.get();
    root->children_.push_back(std::move(node));
  }

  std::vector<PackageNode*> ordered;
  for (auto& child : root->children_) {
    if (child->type() == DomNodeType::Package)
      ordered.push_back(static_cast<PackageNode*>(child.get()));
  }
  std::sort(ordered.begin(), ordered.end(), [](const PackageNode* a, const PackageNode* b) {
    return a->package_xml_path().parent_path().native().size() < b->package_xml_path().parent_path().native().size();
  });
  for (PackageNode* pkg : ordered) {
    PackageNode* best_parent = nullptr;
    const auto my_dir = pkg->package_xml_path_.parent_path();
    for (PackageNode* maybe : ordered) {
      if (maybe == pkg)
        continue;
      const auto pdir = maybe->package_xml_path_.parent_path();
      const std::string my = to_posix_path_string(my_dir);
      const std::string pd = to_posix_path_string(pdir);
      if (my.rfind(pd, 0) == 0 && my != pd) {
        if (!best_parent ||
            to_posix_path_string(best_parent->package_xml_path_.parent_path()).size() < pd.size()) {
          best_parent = maybe;
        }
      }
    }
    if (!best_parent)
      continue;
    for (auto it = root->children_.begin(); it != root->children_.end(); ++it) {
      if (it->get() == pkg) {
        std::unique_ptr<DomNode> moved = std::move(*it);
        root->children_.erase(it);
        moved->parent_ = best_parent;
        best_parent->children_.push_back(std::move(moved));
        break;
      }
    }
  }

  for (const auto& target_path : options.target_files) {
    TargetDesc td;
    std::string err;
    if (!load_target_xml(target_path, td, err)) {
      error = to_posix_path_string(target_path) + ": " + err;
      return false;
    }
    PackageNode* owner = nullptr;
    std::string owner_dir;
    for (const auto& kv : package_by_dir) {
      const std::string tdir = to_posix_path_string(target_path.parent_path());
      const std::string pdir = to_posix_path_string(kv.first);
      if (tdir.rfind(pdir, 0) == 0 && pdir.size() >= owner_dir.size()) {
        owner_dir = pdir;
        owner = kv.second;
      }
    }
    if (!owner)
      continue;
    auto target = std::make_unique<TargetNode>();
    target->type_ = DomNodeType::Target;
    target->name_ = td.name;
    target->target_type_ = td.type;
    target->target_xml_path_ = target_path;
    target->parent_ = owner;
    for (const auto& dep : td.dependencies)
      target->dependencies_.push_back({dep.name, false});
    for (const auto& kv : td.vars) {
      VarEntry e;
      e.name = kv.first;
      e.value.type = VarValueType::Scalar;
      e.value.value = kv.second;
      target->vars_.push_back(std::move(e));
    }
    for (const auto& sv : td.scripts) {
      VarEntry e;
      e.name = sv.name;
      e.value.type = VarValueType::Script;
      e.value.script.script_type = sv.script_type.empty() ? "lua" : sv.script_type;
      e.value.script.trigger = sv.trigger;
      e.value.script.source = sv.source;
      target->vars_.push_back(std::move(e));
    }
    owner->children_.push_back(std::move(target));
  }

  out.global_ = std::move(root);
  error.clear();
  return true;
}

bool DomDocument::write_xml(std::ostream& out) const {
  if (!global_)
    return false;
  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  out << "<gz_dom>\n";
  dom_write_xml_subtree(*global_, out, 1);
  out << "</gz_dom>\n";
  return static_cast<bool>(out);
}

bool DomDocument::write_xml_file(const std::filesystem::path& path, std::string& error) const {
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) {
    error = "cannot write: " + to_posix_path_string(path);
    return false;
  }
  if (!write_xml(f)) {
    error = "write failed: " + to_posix_path_string(path);
    return false;
  }
  error.clear();
  return true;
}

bool DomDocument::write_json(std::ostream& out) const {
  if (!global_)
    return false;
  out << "{\n";
  out << "  \"dom\": ";
  dom_write_json_subtree(*global_, out, 1);
  out << "\n}\n";
  return static_cast<bool>(out);
}

bool DomDocument::write_json_file(const std::filesystem::path& path, std::string& error) const {
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) {
    error = "cannot write: " + to_posix_path_string(path);
    return false;
  }
  if (!write_json(f)) {
    error = "write failed: " + to_posix_path_string(path);
    return false;
  }
  error.clear();
  return true;
}

void DomDocument::print_tree(std::ostream& out) const {
  if (!global_)
    return;
  dom_print_tree_subtree(*global_, out, 0);
}

}  // namespace gz
