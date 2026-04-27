#pragma once

#include "dom_nodes.hpp"

#include <iosfwd>

namespace gz {

struct DomNodeVisitor {
  virtual ~DomNodeVisitor() = default;
  virtual void visit(const GlobalNode&) = 0;
  virtual void visit(const PackageNode&) = 0;
  virtual void visit(const TargetNode&) = 0;
};

void traverse_dom(const DomNode& node, DomNodeVisitor& visitor);

void dom_write_xml_subtree(const DomNode& root, std::ostream& out, int initial_depth);
void dom_write_json_subtree(const DomNode& root, std::ostream& out, int initial_depth);
void dom_print_tree_subtree(const DomNode& root, std::ostream& out, int initial_depth);

}  // namespace gz
