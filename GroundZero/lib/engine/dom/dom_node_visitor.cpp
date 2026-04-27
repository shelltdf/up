#include "dom_node_visitor.hpp"

#include "paths.hpp"

#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace gz {
namespace detail {

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

void emit_vars_xml(const DomNode& node, std::ostream& out, int depth) {
  const std::string indent(static_cast<size_t>(depth) * 2, ' ');
  if (node.vars().empty())
    return;
  out << indent << "  <vars>\n";
  for (const auto& v : node.vars()) {
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

struct DomXmlEmitVisitor final : DomNodeVisitor {
  std::ostream& out_;
  int depth_;

  DomXmlEmitVisitor(std::ostream& out, int initial_depth) : out_(out), depth_(initial_depth) {}

  void visit(const GlobalNode& g) override { emit_xml_node(static_cast<const DomNode&>(g)); }
  void visit(const PackageNode& p) override { emit_xml_node(static_cast<const DomNode&>(p)); }
  void visit(const TargetNode& t) override { emit_xml_node(static_cast<const DomNode&>(t)); }

  void recurse_children(const DomNode& n) {
    ++depth_;
    for (const auto& c : n.children())
      traverse_dom(*c, *this);
    --depth_;
  }

  void emit_xml_node(const DomNode& node) {
    const std::string indent(static_cast<size_t>(depth_) * 2, ' ');
    if (node.type() == DomNodeType::Global) {
      out_ << indent << "<global name=\"" << xml_escape(node.name()) << "\">\n";
    } else if (node.type() == DomNodeType::Package) {
      const auto& pkg = static_cast<const PackageNode&>(node);
      out_ << indent << "<package name=\"" << xml_escape(pkg.name()) << "\" version=\"" << xml_escape(pkg.version()) << "\"";
      if (!pkg.package_xml_path().empty())
        out_ << " path=\"" << xml_escape(to_posix_path_string(pkg.package_xml_path())) << "\"";
      out_ << ">\n";
    } else {
      const auto& target = static_cast<const TargetNode&>(node);
      out_ << indent << "<target name=\"" << xml_escape(target.name()) << "\" type=\"" << xml_escape(target.target_type()) << "\"";
      if (!target.target_xml_path().empty())
        out_ << " path=\"" << xml_escape(to_posix_path_string(target.target_xml_path())) << "\"";
      out_ << ">\n";
    }

    emit_vars_xml(node, out_, depth_);
    recurse_children(node);

    if (node.type() == DomNodeType::Global)
      out_ << indent << "</global>\n";
    else if (node.type() == DomNodeType::Package)
      out_ << indent << "</package>\n";
    else
      out_ << indent << "</target>\n";
  }
};

struct DomJsonEmitVisitor final : DomNodeVisitor {
  std::ostream& out_;
  int depth_;

  DomJsonEmitVisitor(std::ostream& out, int initial_depth) : out_(out), depth_(initial_depth) {}

  void visit(const GlobalNode& g) override { emit_json_node(static_cast<const DomNode&>(g)); }
  void visit(const PackageNode& p) override { emit_json_node(static_cast<const DomNode&>(p)); }
  void visit(const TargetNode& t) override { emit_json_node(static_cast<const DomNode&>(t)); }

  void emit_json_node(const DomNode& node) {
    const std::string indent(static_cast<size_t>(depth_) * 2, ' ');
    const std::string i2(static_cast<size_t>(depth_ + 1) * 2, ' ');
    out_ << indent << "{\n";
    out_ << i2 << "\"name\": \"" << json_escape(node.name()) << "\",\n";
    if (node.type() == DomNodeType::Global) {
      out_ << i2 << "\"type\": \"global\"";
    } else if (node.type() == DomNodeType::Package) {
      const auto& pkg = static_cast<const PackageNode&>(node);
      out_ << i2 << "\"type\": \"package\",\n";
      out_ << i2 << "\"version\": \"" << json_escape(pkg.version()) << "\",\n";
      out_ << i2 << "\"dependencies\": [";
      for (size_t i = 0; i < pkg.dependencies().size(); ++i) {
        out_ << "{\"name\":\"" << json_escape(pkg.dependencies()[i].first) << "\",\"optional\":"
             << (pkg.dependencies()[i].second ? "true" : "false") << "}";
        if (i + 1 < pkg.dependencies().size())
          out_ << ",";
      }
      out_ << "]";
    } else {
      const auto& target = static_cast<const TargetNode&>(node);
      out_ << i2 << "\"type\": \"target\",\n";
      out_ << i2 << "\"targetType\": \"" << json_escape(target.target_type()) << "\",\n";
      out_ << i2 << "\"dependencies\": [";
      for (size_t i = 0; i < target.dependencies().size(); ++i) {
        out_ << "{\"name\":\"" << json_escape(target.dependencies()[i].first) << "\",\"optional\":"
             << (target.dependencies()[i].second ? "true" : "false") << "}";
        if (i + 1 < target.dependencies().size())
          out_ << ",";
      }
      out_ << "]";
    }
    out_ << ",\n";
    out_ << i2 << "\"vars\": [\n";
    for (size_t i = 0; i < node.vars().size(); ++i) {
      const auto& v = node.vars()[i];
      out_ << i2 << "  {\"name\": \"" << json_escape(v.name) << "\", ";
      if (v.value.type == VarValueType::Script) {
        out_ << "\"valueType\": \"script\", "
             << "\"scriptType\": \"" << json_escape(v.value.script.script_type) << "\", "
             << "\"trigger\": \"" << json_escape(v.value.script.trigger) << "\", "
             << "\"value\": \"" << json_escape(v.value.script.source) << "\"}";
      } else {
        out_ << "\"valueType\": \"scalar\", "
             << "\"value\": \"" << json_escape(v.value.value) << "\"}";
      }
      if (i + 1 < node.vars().size())
        out_ << ",";
      out_ << "\n";
    }
    out_ << i2 << "],\n";
    out_ << i2 << "\"children\": [\n";
    for (size_t i = 0; i < node.children().size(); ++i) {
      depth_ += 2;
      traverse_dom(*node.children()[i], *this);
      depth_ -= 2;
      if (i + 1 < node.children().size())
        out_ << ",";
      out_ << "\n";
    }
    out_ << i2 << "]\n";
    out_ << indent << "}";
  }
};

struct DomTreePrintVisitor final : DomNodeVisitor {
  std::ostream& out_;
  int depth_;

  DomTreePrintVisitor(std::ostream& out, int initial_depth) : out_(out), depth_(initial_depth) {}

  void visit(const GlobalNode& g) override { print_node(static_cast<const DomNode&>(g)); }
  void visit(const PackageNode& p) override { print_node(static_cast<const DomNode&>(p)); }
  void visit(const TargetNode& t) override { print_node(static_cast<const DomNode&>(t)); }

  void recurse_children(const DomNode& n) {
    ++depth_;
    for (const auto& c : n.children())
      traverse_dom(*c, *this);
    --depth_;
  }

  void print_node(const DomNode& node) {
    const std::string indent(static_cast<size_t>(depth_) * 2, ' ');
    if (node.type() == DomNodeType::Global) {
      out_ << indent << "global \"" << node.name() << "\"\n";
    } else if (node.type() == DomNodeType::Package) {
      const auto& pkg = static_cast<const PackageNode&>(node);
      out_ << indent << "package \"" << pkg.name() << "\" v" << pkg.version();
      if (!pkg.dependencies().empty())
        out_ << " deps=" << join_dependencies(pkg.dependencies());
      out_ << "\n";
    } else {
      const auto& target = static_cast<const TargetNode&>(node);
      out_ << indent << "target \"" << target.name() << "\" (" << target.target_type() << ")";
      if (!target.dependencies().empty())
        out_ << " deps=" << join_dependencies(target.dependencies());
      out_ << "\n";
    }
    for (const auto& v : node.vars()) {
      if (v.value.type == VarValueType::Script) {
        out_ << indent << "  script-var " << v.name << " [" << v.value.script.script_type << "] trigger="
             << v.value.script.trigger << "\n";
      } else {
        out_ << indent << "  var " << v.name << "=" << v.value.value << "\n";
      }
    }
    recurse_children(node);
  }
};

}  // namespace detail

void traverse_dom(const DomNode& node, DomNodeVisitor& visitor) {
  switch (node.type()) {
    case DomNodeType::Global:
      visitor.visit(static_cast<const GlobalNode&>(node));
      break;
    case DomNodeType::Package:
      visitor.visit(static_cast<const PackageNode&>(node));
      break;
    case DomNodeType::Target:
      visitor.visit(static_cast<const TargetNode&>(node));
      break;
  }
}

void dom_write_xml_subtree(const DomNode& root, std::ostream& out, int initial_depth) {
  detail::DomXmlEmitVisitor vis(out, initial_depth);
  traverse_dom(root, vis);
}

void dom_write_json_subtree(const DomNode& root, std::ostream& out, int initial_depth) {
  detail::DomJsonEmitVisitor vis(out, initial_depth);
  traverse_dom(root, vis);
}

void dom_print_tree_subtree(const DomNode& root, std::ostream& out, int initial_depth) {
  detail::DomTreePrintVisitor vis(out, initial_depth);
  traverse_dom(root, vis);
}

}  // namespace gz
