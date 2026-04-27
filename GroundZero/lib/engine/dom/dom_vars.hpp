#pragma once

#include "dom_types.hpp"

#include <string>

namespace gz {

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

}  // namespace gz
