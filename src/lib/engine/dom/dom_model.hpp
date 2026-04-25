#pragma once

#include <filesystem>
#include <iosfwd>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace up {

enum class DomNodeType { Global, Package, Target };
enum class VarValueType { Scalar, Script };
enum class ScriptMessageType {
  Unknown = 0,
  SourcesPreprocess,
  SourcesPostprocess,
  HeadersPreprocess,
  HeadersPostprocess,
  AssetsPreprocess,
  AssetsPostprocess,
  Manual,
};

struct ScriptValue {
  std::string script_type = "lua";
  std::string trigger;
  std::string source;
};

struct VarValue {
  VarValueType type = VarValueType::Scalar;
  std::string value;
  ScriptValue script;
};

struct VarEntry {
  std::string name;
  VarValue value;
};

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
  std::vector<std::string> defines;             // NAME or NAME=VALUE
};

struct TargetNode : DomNode {
  std::filesystem::path target_xml_path;
  std::string target_type;
  std::vector<std::pair<std::string, bool>> dependencies;  // name, optional
};

struct GlobalNode : DomNode {
  std::filesystem::path workspace_root;
};

struct DomDocument {
  std::unique_ptr<GlobalNode> global;
  std::map<std::string, PackageNode*> package_by_name;
};

struct ScriptExecutionContext {
  const DomDocument* dom_root = nullptr;
  const DomNode* current_node = nullptr;
  ScriptMessageType message_type = ScriptMessageType::Unknown;
  std::map<std::string, std::string> vars_view;
};

struct BuildDomOptions {
  std::filesystem::path cwd;
  std::vector<std::filesystem::path> package_files;
  std::vector<std::filesystem::path> target_files;
};

bool build_dom_document(const BuildDomOptions& options, DomDocument& out, std::string& error);

bool write_dom_as_xml(const DomDocument& doc, std::ostream& out);
bool write_dom_as_xml_file(const DomDocument& doc, const std::filesystem::path& path, std::string& error);
bool write_dom_as_json(const DomDocument& doc, std::ostream& out);
bool write_dom_as_json_file(const DomDocument& doc, const std::filesystem::path& path, std::string& error);
void print_dom_tree(const DomDocument& doc, std::ostream& out);
std::vector<const ScriptValue*> collect_scripts_for_message(const ScriptExecutionContext& context);
std::string resolve_script_command(const ScriptExecutionContext& context, const std::string& fallback_command);
ScriptMessageType script_message_type_from_string(const std::string& value);
std::string script_message_type_to_string(ScriptMessageType value);

}  // namespace up
