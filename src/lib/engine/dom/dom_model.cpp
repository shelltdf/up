#include "dom_model.hpp"

#include "paths.hpp"
#include "simple_xml.hpp"

#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

namespace up {
namespace {

std::string join_dependencies(const std::vector<std::pair<std::string, bool>>& deps) {
  std::string out;
  for (size_t i = 0; i < deps.size(); ++i) {
    if (i > 0)
      out += ",";
    out += deps[i].first;
    if (deps[i].second)
      out += "?";
  }
  return out;
}

std::string xml_escape(const std::string& s) {
  std::string o;
  o.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
      case '&':
        o += "&amp;";
        break;
      case '<':
        o += "&lt;";
        break;
      case '>':
        o += "&gt;";
        break;
      case '"':
        o += "&quot;";
        break;
      default:
        o += c;
        break;
    }
  }
  return o;
}

std::string json_escape(const std::string& s) {
  std::string o;
  o.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
      case '"':
        o += "\\\"";
        break;
      case '\\':
        o += "\\\\";
        break;
      case '\n':
        o += "\\n";
        break;
      case '\r':
        o += "\\r";
        break;
      case '\t':
        o += "\\t";
        break;
      default:
        o += c;
        break;
    }
  }
  return o;
}

void emit_node_xml(const DomNode& node, std::ostream& out, int depth) {
  const std::string indent(static_cast<size_t>(depth) * 2, ' ');
  if (node.type == DomNodeType::Global) {
    out << indent << "<global name=\"" << xml_escape(node.name) << "\">\n";
  } else if (node.type == DomNodeType::Package) {
    const auto& pkg = static_cast<const PackageNode&>(node);
    out << indent << "<package name=\"" << xml_escape(pkg.name) << "\" version=\"" << xml_escape(pkg.version) << "\"";
    if (!pkg.package_xml_path.empty())
      out << " path=\"" << xml_escape(to_posix_path_string(pkg.package_xml_path)) << "\"";
    out << ">\n";
  } else {
    const auto& target = static_cast<const TargetNode&>(node);
    out << indent << "<target name=\"" << xml_escape(target.name) << "\" type=\"" << xml_escape(target.target_type) << "\"";
    if (!target.target_xml_path.empty())
      out << " path=\"" << xml_escape(to_posix_path_string(target.target_xml_path)) << "\"";
    out << ">\n";
  }

  if (!node.vars.empty()) {
    out << indent << "  <vars>\n";
    for (const auto& v : node.vars) {
      if (v.value.type == VarValueType::Script) {
        out << indent << "    <var name=\"" << xml_escape(v.name)
            << "\" type=\"script\" script_type=\"" << xml_escape(v.value.script.script_type)
            << "\" trigger=\"" << xml_escape(v.value.script.trigger)
            << "\" value=\"" << xml_escape(v.value.script.source) << "\"/>\n";
      } else {
        out << indent << "    <var name=\"" << xml_escape(v.name) << "\" value=\"" << xml_escape(v.value.value) << "\"/>\n";
      }
    }
    out << indent << "  </vars>\n";
  }

  for (const auto& child : node.children)
    emit_node_xml(*child, out, depth + 1);

  if (node.type == DomNodeType::Global)
    out << indent << "</global>\n";
  else if (node.type == DomNodeType::Package)
    out << indent << "</package>\n";
  else
    out << indent << "</target>\n";
}

void emit_node_json(const DomNode& node, std::ostream& out, int depth) {
  const std::string indent(static_cast<size_t>(depth) * 2, ' ');
  const std::string i2(static_cast<size_t>(depth + 1) * 2, ' ');
  out << indent << "{\n";
  out << i2 << "\"name\": \"" << json_escape(node.name) << "\",\n";
  if (node.type == DomNodeType::Global) {
    out << i2 << "\"type\": \"global\"";
  } else if (node.type == DomNodeType::Package) {
    const auto& pkg = static_cast<const PackageNode&>(node);
    out << i2 << "\"type\": \"package\",\n";
    out << i2 << "\"version\": \"" << json_escape(pkg.version) << "\",\n";
    out << i2 << "\"dependencies\": [";
    for (size_t i = 0; i < pkg.dependencies.size(); ++i) {
      out << "{\"name\":\"" << json_escape(pkg.dependencies[i].first) << "\",\"optional\":"
          << (pkg.dependencies[i].second ? "true" : "false") << "}";
      if (i + 1 < pkg.dependencies.size())
        out << ",";
    }
    out << "]";
  } else {
    const auto& target = static_cast<const TargetNode&>(node);
    out << i2 << "\"type\": \"target\",\n";
    out << i2 << "\"targetType\": \"" << json_escape(target.target_type) << "\",\n";
    out << i2 << "\"dependencies\": [";
    for (size_t i = 0; i < target.dependencies.size(); ++i) {
      out << "{\"name\":\"" << json_escape(target.dependencies[i].first) << "\",\"optional\":"
          << (target.dependencies[i].second ? "true" : "false") << "}";
      if (i + 1 < target.dependencies.size())
        out << ",";
    }
    out << "]";
  }
  out << ",\n";
  out << i2 << "\"vars\": [\n";
  for (size_t i = 0; i < node.vars.size(); ++i) {
    const auto& v = node.vars[i];
    out << i2 << "  {\"name\": \"" << json_escape(v.name) << "\", ";
    if (v.value.type == VarValueType::Script) {
      out << "\"valueType\": \"script\", "
          << "\"scriptType\": \"" << json_escape(v.value.script.script_type) << "\", "
          << "\"trigger\": \"" << json_escape(v.value.script.trigger) << "\", "
          << "\"value\": \"" << json_escape(v.value.script.source) << "\"}";
    } else {
      out << "\"valueType\": \"scalar\", "
          << "\"value\": \"" << json_escape(v.value.value) << "\"}";
    }
    if (i + 1 < node.vars.size())
      out << ",";
    out << "\n";
  }
  out << i2 << "],\n";
  out << i2 << "\"children\": [\n";
  for (size_t i = 0; i < node.children.size(); ++i) {
    emit_node_json(*node.children[i], out, depth + 2);
    if (i + 1 < node.children.size())
      out << ",";
    out << "\n";
  }
  out << i2 << "]\n";
  out << indent << "}";
}

void print_node(const DomNode& node, std::ostream& out, int depth) {
  const std::string indent(static_cast<size_t>(depth) * 2, ' ');
  if (node.type == DomNodeType::Global) {
    out << indent << "global \"" << node.name << "\"\n";
  } else if (node.type == DomNodeType::Package) {
    const auto& pkg = static_cast<const PackageNode&>(node);
    out << indent << "package \"" << pkg.name << "\" v" << pkg.version;
    if (!pkg.dependencies.empty())
      out << " deps=" << join_dependencies(pkg.dependencies);
    out << "\n";
  } else {
    const auto& target = static_cast<const TargetNode&>(node);
    out << indent << "target \"" << target.name << "\" (" << target.target_type << ")";
    if (!target.dependencies.empty())
      out << " deps=" << join_dependencies(target.dependencies);
    out << "\n";
  }
  for (const auto& v : node.vars) {
    if (v.value.type == VarValueType::Script) {
      out << indent << "  script-var " << v.name << " [" << v.value.script.script_type << "] trigger="
          << v.value.script.trigger << "\n";
    } else {
      out << indent << "  var " << v.name << "=" << v.value.value << "\n";
    }
  }
  for (const auto& child : node.children)
    print_node(*child, out, depth + 1);
}

ScriptMessageType parse_message_type(const std::string& s) {
  if (s == "sources.preprocess")
    return ScriptMessageType::SourcesPreprocess;
  if (s == "sources.postprocess")
    return ScriptMessageType::SourcesPostprocess;
  if (s == "headers.preprocess")
    return ScriptMessageType::HeadersPreprocess;
  if (s == "headers.postprocess")
    return ScriptMessageType::HeadersPostprocess;
  if (s == "assets.preprocess")
    return ScriptMessageType::AssetsPreprocess;
  if (s == "assets.postprocess")
    return ScriptMessageType::AssetsPostprocess;
  if (s == "manual")
    return ScriptMessageType::Manual;
  return ScriptMessageType::Unknown;
}

std::string message_type_name(ScriptMessageType s) {
  switch (s) {
    case ScriptMessageType::SourcesPreprocess:
      return "sources.preprocess";
    case ScriptMessageType::SourcesPostprocess:
      return "sources.postprocess";
    case ScriptMessageType::HeadersPreprocess:
      return "headers.preprocess";
    case ScriptMessageType::HeadersPostprocess:
      return "headers.postprocess";
    case ScriptMessageType::AssetsPreprocess:
      return "assets.preprocess";
    case ScriptMessageType::AssetsPostprocess:
      return "assets.postprocess";
    case ScriptMessageType::Manual:
      return "manual";
    default:
      return "unknown";
  }
}

}  // namespace

bool build_dom_document(const BuildDomOptions& options, DomDocument& out, std::string& error) {
  out = {};
  auto root = std::make_unique<GlobalNode>();
  root->type = DomNodeType::Global;
  root->name = "global";
  root->workspace_root = options.cwd;

  std::map<std::filesystem::path, PackageNode*> package_by_dir;
  for (const auto& pkg_path : options.package_files) {
    PackageDesc pkg;
    std::string err;
    if (!load_package_xml(pkg_path, pkg, err)) {
      error = to_posix_path_string(pkg_path) + ": " + err;
      return false;
    }
    auto node = std::make_unique<PackageNode>();
    node->type = DomNodeType::Package;
    node->name = pkg.name;
    node->version = pkg.version;
    node->package_xml_path = pkg_path;
    node->dependencies = pkg.dependencies;
    if (pkg.external_cmake.has_value())
      node->external_cmake_source_dir = pkg.external_cmake->source_dir;
    for (const auto& kv : pkg.vars) {
      VarEntry e;
      e.name = kv.first;
      e.value.type = VarValueType::Scalar;
      e.value.value = kv.second;
      node->vars.push_back(std::move(e));
    }
    for (const auto& sv : pkg.scripts) {
      VarEntry e;
      e.name = sv.name;
      e.value.type = VarValueType::Script;
      e.value.script.script_type = sv.script_type.empty() ? "lua" : sv.script_type;
      e.value.script.trigger = sv.trigger;
      e.value.script.source = sv.source;
      node->vars.push_back(std::move(e));
    }
    for (const auto& d : pkg.defines)
      node->defines.push_back(d.value.empty() ? d.name : (d.name + "=" + d.value));
    for (const auto& cf : pkg.config_files)
      node->config_files_in_to.push_back(cf.in + "::" + cf.to);

    package_by_dir[pkg_path.parent_path()] = node.get();
    out.package_by_name[node->name] = node.get();
    root->children.push_back(std::move(node));
  }

  // Nest packages by directory hierarchy.
  std::vector<PackageNode*> ordered;
  for (auto& child : root->children) {
    if (child->type == DomNodeType::Package)
      ordered.push_back(static_cast<PackageNode*>(child.get()));
  }
  std::sort(ordered.begin(), ordered.end(), [](const PackageNode* a, const PackageNode* b) {
    return a->package_xml_path.parent_path().native().size() < b->package_xml_path.parent_path().native().size();
  });
  for (PackageNode* pkg : ordered) {
    PackageNode* best_parent = nullptr;
    const auto my_dir = pkg->package_xml_path.parent_path();
    for (PackageNode* maybe : ordered) {
      if (maybe == pkg)
        continue;
      const auto pdir = maybe->package_xml_path.parent_path();
      const std::string my = to_posix_path_string(my_dir);
      const std::string pd = to_posix_path_string(pdir);
      if (my.rfind(pd, 0) == 0 && my != pd) {
        if (!best_parent ||
            to_posix_path_string(best_parent->package_xml_path.parent_path()).size() < pd.size()) {
          best_parent = maybe;
        }
      }
    }
    if (!best_parent)
      continue;
    for (auto it = root->children.begin(); it != root->children.end(); ++it) {
      if (it->get() == pkg) {
        std::unique_ptr<DomNode> moved = std::move(*it);
        root->children.erase(it);
        moved->parent = best_parent;
        best_parent->children.push_back(std::move(moved));
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
    target->type = DomNodeType::Target;
    target->name = td.name;
    target->target_type = td.type;
    target->target_xml_path = target_path;
    target->parent = owner;
    for (const auto& dep : td.dependencies)
      target->dependencies.push_back({dep.name, false});
    for (const auto& kv : td.vars) {
      VarEntry e;
      e.name = kv.first;
      e.value.type = VarValueType::Scalar;
      e.value.value = kv.second;
      target->vars.push_back(std::move(e));
    }
    for (const auto& sv : td.scripts) {
      VarEntry e;
      e.name = sv.name;
      e.value.type = VarValueType::Script;
      e.value.script.script_type = sv.script_type.empty() ? "lua" : sv.script_type;
      e.value.script.trigger = sv.trigger;
      e.value.script.source = sv.source;
      target->vars.push_back(std::move(e));
    }
    owner->children.push_back(std::move(target));
  }

  out.global = std::move(root);
  error.clear();
  return true;
}

bool write_dom_as_xml(const DomDocument& doc, std::ostream& out) {
  if (!doc.global)
    return false;
  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  out << "<up_dom>\n";
  emit_node_xml(*doc.global, out, 1);
  out << "</up_dom>\n";
  return static_cast<bool>(out);
}

bool write_dom_as_xml_file(const DomDocument& doc, const std::filesystem::path& path, std::string& error) {
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) {
    error = "cannot write: " + to_posix_path_string(path);
    return false;
  }
  if (!write_dom_as_xml(doc, f)) {
    error = "write failed: " + to_posix_path_string(path);
    return false;
  }
  error.clear();
  return true;
}

bool write_dom_as_json(const DomDocument& doc, std::ostream& out) {
  if (!doc.global)
    return false;
  out << "{\n";
  out << "  \"dom\": ";
  emit_node_json(*doc.global, out, 1);
  out << "\n}\n";
  return static_cast<bool>(out);
}

bool write_dom_as_json_file(const DomDocument& doc, const std::filesystem::path& path, std::string& error) {
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) {
    error = "cannot write: " + to_posix_path_string(path);
    return false;
  }
  if (!write_dom_as_json(doc, f)) {
    error = "write failed: " + to_posix_path_string(path);
    return false;
  }
  error.clear();
  return true;
}

void print_dom_tree(const DomDocument& doc, std::ostream& out) {
  if (!doc.global)
    return;
  print_node(*doc.global, out, 0);
}

std::vector<const ScriptValue*> collect_scripts_for_message(const ScriptExecutionContext& context) {
  std::vector<const ScriptValue*> out;
  if (!context.current_node)
    return out;
  const DomNode* node = context.current_node;
  while (node) {
    for (const auto& v : node->vars) {
      if (v.value.type == VarValueType::Script &&
          parse_message_type(v.value.script.trigger) == context.message_type)
        out.push_back(&v.value.script);
    }
    node = node->parent;
  }
  return out;
}

std::string resolve_script_command(const ScriptExecutionContext& context, const std::string& fallback_command) {
  if (!fallback_command.empty())
    return fallback_command;
  const auto scripts = collect_scripts_for_message(context);
  for (const ScriptValue* s : scripts) {
    if (!s)
      continue;
    if (!s->script_type.empty() && s->script_type != "lua")
      continue;
    if (!s->source.empty())
      return s->source;
  }
  return {};
}

ScriptMessageType script_message_type_from_string(const std::string& value) { return parse_message_type(value); }

std::string script_message_type_to_string(ScriptMessageType value) { return message_type_name(value); }

}  // namespace up
