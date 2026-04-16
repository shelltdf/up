#include "simple_xml.hpp"

#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>

namespace up {

namespace {

std::string read_all(const std::filesystem::path& path, std::string& error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    error = "cannot open: " + path.string();
    return {};
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

bool attr_string(const std::string& xml, const char* name, std::string& out) {
  std::regex re(std::string(R"rx(\b)rx") + name + R"rx(\s*=\s*"([^"]*)")rx");
  std::smatch m;
  if (!std::regex_search(xml, m, re))
    return false;
  out = m[1].str();
  return true;
}

std::string trim_copy(std::string s) {
  size_t b = 0;
  while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b])))
    ++b;
  size_t e = s.size();
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])))
    --e;
  return s.substr(b, e - b);
}

}  // namespace

bool load_package_xml(const std::filesystem::path& path, PackageDesc& out, std::string& error) {
  const std::string raw = read_all(path, error);
  if (raw.empty() && !error.empty())
    return false;
  const size_t pkg_pos = raw.find("<package");
  if (pkg_pos == std::string::npos) {
    error = "missing <package> root";
    return false;
  }
  const size_t pkg_gt = raw.find('>', pkg_pos);
  const std::string pkg_head =
      raw.substr(pkg_pos, pkg_gt == std::string::npos ? std::string::npos : pkg_gt - pkg_pos + 1);
  if (!attr_string(pkg_head, "name", out.name)) {
    error = "package name attribute required";
    return false;
  }
  if (!attr_string(pkg_head, "version", out.version))
    out.version = "0.0.0";

  std::regex dep_re(R"rx(<dependency\s+[^>]*name\s*=\s*"([^"]*)"[^>]*/>)rx");
  for (std::sregex_iterator it(raw.begin(), raw.end(), dep_re), end; it != end; ++it) {
    const std::string dep_name = (*it)[1].str();
    bool optional = false;
    const std::string frag = (*it)[0].str();
    std::smatch om;
    if (std::regex_search(frag, om, std::regex(R"rx(optional\s*=\s*"([^"]*)")rx"))) {
      const std::string v = om[1].str();
      optional = (v == "1" || v == "true" || v == "yes");
    }
    out.dependencies.emplace_back(dep_name, optional);
  }
  error.clear();
  return true;
}

bool load_target_xml(const std::filesystem::path& path, TargetDesc& out, std::string& error) {
  const std::string raw = read_all(path, error);
  if (raw.empty() && !error.empty())
    return false;
  const size_t tgt_pos = raw.find("<target");
  if (tgt_pos == std::string::npos) {
    error = "missing <target> root";
    return false;
  }
  const size_t tgt_gt = raw.find('>', tgt_pos);
  const std::string tgt_head =
      raw.substr(tgt_pos, tgt_gt == std::string::npos ? std::string::npos : tgt_gt - tgt_pos + 1);
  if (!attr_string(tgt_head, "name", out.name)) {
    error = "target name attribute required";
    return false;
  }
  if (!attr_string(tgt_head, "type", out.type))
    out.type = "executable";

  std::regex file_re(R"rx(<file\s*>\s*([^<]+)\s*</file\s*>)rx");
  for (std::sregex_iterator it(raw.begin(), raw.end(), file_re), end; it != end; ++it)
    out.sources.push_back((*it)[1].str());

  std::regex dep_re(R"rx(<dependency\s+[^>]*name\s*=\s*"([^"]*)"[^>]*/>)rx");
  for (std::sregex_iterator it(raw.begin(), raw.end(), dep_re), end; it != end; ++it)
    out.dependencies.push_back((*it)[1].str());

  const size_t includes_open = raw.find("<includes");
  if (includes_open != std::string::npos) {
    const size_t includes_open_gt = raw.find('>', includes_open);
    if (includes_open_gt == std::string::npos) {
      error = "invalid <includes> tag";
      return false;
    }
    const size_t includes_close = raw.find("</includes>", includes_open_gt + 1);
    if (includes_close == std::string::npos) {
      error = "missing </includes> closing tag";
      return false;
    }
    const std::string body = raw.substr(includes_open_gt + 1, includes_close - includes_open_gt - 1);
    std::regex item_re(R"rx(<\s*([A-Za-z_][A-Za-z0-9_]*)\s*([^>]*)/>)rx");
    for (std::sregex_iterator it(body.begin(), body.end(), item_re), end; it != end; ++it) {
      TargetDesc::IncludeEntry inc;
      inc.kind = trim_copy((*it)[1].str());
      if (!(inc.kind == "dir" || inc.kind == "file" || inc.kind == "glob")) {
        error = "unsupported includes entry: " + inc.kind + " (expected dir/file/glob)";
        return false;
      }
      const std::string attrs = (*it)[2].str();
      if (!attr_string(attrs, "from", inc.from)) {
        error = "includes entry requires from attribute";
        return false;
      }
      inc.from = trim_copy(inc.from);
      if (inc.from.empty()) {
        error = "includes entry from attribute cannot be empty";
        return false;
      }
      if (!attr_string(attrs, "to", inc.to))
        inc.to.clear();
      else
        inc.to = trim_copy(inc.to);
      out.includes.push_back(std::move(inc));
    }

    // old style is intentionally not supported anymore
    std::regex old_style_re(R"rx(<\s*dir\s*>\s*[^<]+\s*</\s*dir\s*>)rx");
    if (std::regex_search(body, old_style_re)) {
      error = "old <includes><dir>path</dir></includes> syntax is not supported; use <dir from=\"...\" to=\"...\"/>";
      return false;
    }
  }

  error.clear();
  return true;
}

}  // namespace up
