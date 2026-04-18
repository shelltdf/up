#include "var_subst.hpp"

#include <cctype>
#include <filesystem>
#include <regex>
#include <sstream>

namespace up {

namespace {

std::string lower_ascii(std::string v) {
  for (char& c : v) {
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c - 'A' + 'a');
  }
  return v;
}

std::string trim_ws(std::string s) {
  size_t b = 0;
  while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b])))
    ++b;
  size_t e = s.size();
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])))
    --e;
  return s.substr(b, e - b);
}

bool truthy_value(const std::string& v) {
  const std::string x = lower_ascii(trim_ws(v));
  if (x.empty() || x == "0" || x == "false" || x == "no" || x == "off")
    return false;
  return true;
}

bool cmake_macro_truthy(const std::map<std::string, std::string>& vars, const std::string& name) {
  const auto it = vars.find(name);
  if (it == vars.end())
    return false;
  return truthy_value(it->second);
}

std::string apply_cmakedefine_directives_impl(const std::string& text, const std::map<std::string, std::string>& vars) {
  static const std::regex re01(R"(^(\s*)#cmakedefine01(\s+)([A-Za-z_][A-Za-z0-9_]*)\s*$)");
  static const std::regex re(R"(^(\s*)#cmakedefine(\s+)([A-Za-z_][A-Za-z0-9_]*)(.*)$)");
  std::string out;
  out.reserve(text.size() + 32);
  size_t cur = 0;
  while (cur < text.size()) {
    const size_t nl = text.find('\n', cur);
    const bool has_nl = nl != std::string::npos;
    std::string line = text.substr(cur, has_nl ? nl - cur : std::string::npos);
    if (!line.empty() && line.back() == '\r')
      line.pop_back();

    std::smatch m;
    if (std::regex_match(line, m, re01)) {
      const std::string indent = m[1].str();
      const std::string var = m[3].str();
      out += indent + "#define " + var + " " + (cmake_macro_truthy(vars, var) ? "1" : "0");
    } else if (std::regex_match(line, m, re)) {
      const std::string indent = m[1].str();
      const std::string var = m[3].str();
      const std::string rest = trim_ws(m[4].str());
      if (cmake_macro_truthy(vars, var)) {
        if (rest.empty())
          out += indent + "#define " + var;
        else
          out += indent + "#define " + var + " " + rest;
      } else {
        out += indent + "/* #undef " + var + " */";
      }
    } else {
      out += line;
    }

    if (has_nl) {
      out += '\n';
      cur = nl + 1;
    } else {
      break;
    }
  }
  return out;
}

}  // namespace

std::string builtin_host_os() {
#if defined(_WIN32)
  return "windows";
#elif defined(__APPLE__)
  return "darwin";
#else
  return "linux";
#endif
}

std::map<std::string, std::string> make_builtin_var_map(const std::string& package_name, const std::string& package_version,
                                                        const std::string& target_name, const std::string& build_system,
                                                        const std::string& config_mode) {
  std::map<std::string, std::string> m;
  m["UP_OS"] = builtin_host_os();
  m["UP_PACKAGE_NAME"] = package_name;
  m["UP_PACKAGE_VERSION"] = package_version;
  m["UP_TARGET_NAME"] = target_name;
  m["UP_TARGET_BUILD_SYSTEM"] = lower_ascii(build_system);
  m["UP_CONFIG"] = lower_ascii(config_mode);
  return m;
}

std::map<std::string, std::string> merge_var_layers(const std::map<std::string, std::string>& builtins,
                                                   const std::map<std::string, std::string>& opts,
                                                   const std::vector<std::pair<std::string, std::string>>& package_vars,
                                                   const std::vector<std::pair<std::string, std::string>>& target_vars) {
  std::map<std::string, std::string> out = builtins;
  for (const auto& kv : package_vars)
    out[kv.first] = kv.second;
  for (const auto& kv : target_vars)
    out[kv.first] = kv.second;
  for (const auto& kv : opts)
    out[kv.first] = kv.second;
  return out;
}

std::string substitute_at_vars(const std::string& text, const std::map<std::string, std::string>& vars) {
  std::string out;
  out.reserve(text.size() + 16);
  for (size_t i = 0; i < text.size();) {
    const size_t a = text.find('@', i);
    if (a == std::string::npos) {
      out.append(text, i, text.size() - i);
      break;
    }
    out.append(text, i, a - i);
    const size_t b = text.find('@', a + 1);
    if (b == std::string::npos) {
      out.append(text, a, text.size() - a);
      break;
    }
    const std::string key = text.substr(a + 1, b - a - 1);
    const auto it = vars.find(key);
    if (it != vars.end())
      out += it->second;
    else
      out.append(text, a, b - a + 1);
    i = b + 1;
  }
  return out;
}

std::string apply_cmakedefine_directives(const std::string& text, const std::map<std::string, std::string>& vars) {
  return apply_cmakedefine_directives_impl(text, vars);
}

bool eval_when(const std::string& when, const std::map<std::string, std::string>& vars, std::string& error_out) {
  error_out.clear();
  const std::string w = trim_ws(when);
  if (w.empty())
    return true;
  if (lower_ascii(w) == "true")
    return true;
  if (lower_ascii(w) == "false")
    return false;

  static const std::regex cmp_re(
      R"rx(^\s*([A-Za-z_][A-Za-z0-9_]*)\s*(==|!=)\s*([A-Za-z0-9_.]+)\s*$)rx");
  std::smatch m;
  if (std::regex_match(w, m, cmp_re)) {
    const std::string lhs = m[1].str();
    const std::string op = m[2].str();
    const std::string rhs_lit = lower_ascii(m[3].str());
    const auto it = vars.find(lhs);
    const std::string lhs_val = lower_ascii(it != vars.end() ? it->second : std::string{});
    if (op == "==")
      return lhs_val == rhs_lit;
    return lhs_val != rhs_lit;
  }

  const auto it = vars.find(w);
  if (it != vars.end())
    return truthy_value(it->second);
  error_out = "unknown when expression (use KEY==value, KEY!=value, true/false, or a defined variable name): " + w;
  return false;
}

bool is_safe_relative_config_output(const std::filesystem::path& rel) {
  if (rel.empty() || rel.is_absolute() || rel.has_root_path())
    return false;
  for (const auto& seg : rel) {
    if (seg == "..")
      return false;
  }
  return true;
}

}  // namespace up
