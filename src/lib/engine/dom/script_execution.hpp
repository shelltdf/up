#pragma once

#include "dom_types.hpp"

#include <map>
#include <string>
#include <vector>

namespace up {

class DomDocument;
struct DomNode;
struct ScriptValue;

struct ScriptExecutionContext {
  const DomDocument* dom_root = nullptr;
  const DomNode* current_node = nullptr;
  ScriptMessageType message_type = ScriptMessageType::Unknown;
  std::map<std::string, std::string> vars_view;
};

std::vector<const ScriptValue*> collect_scripts_for_message(const ScriptExecutionContext& context);
std::string resolve_script_command(const ScriptExecutionContext& context, const std::string& fallback_command);
ScriptMessageType script_message_type_from_string(const std::string& value);
std::string script_message_type_to_string(ScriptMessageType value);

}  // namespace up
