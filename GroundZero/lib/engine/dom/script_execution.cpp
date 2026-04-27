#include "script_execution.hpp"

#include "dom_nodes.hpp"

namespace gz {
namespace {

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

std::vector<const ScriptValue*> collect_scripts_for_message(const ScriptExecutionContext& context) {
  std::vector<const ScriptValue*> out;
  if (!context.current_node)
    return out;
  const DomNode* node = context.current_node;
  while (node) {
    for (const auto& v : node->vars()) {
      if (v.value.type == VarValueType::Script &&
          parse_message_type(v.value.script.trigger) == context.message_type)
        out.push_back(&v.value.script);
    }
    node = node->parent();
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

}  // namespace gz
