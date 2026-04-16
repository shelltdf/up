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

void parse_stage_commands(const std::string& body, std::string& pre, std::string& post) {
  std::smatch m;
  if (std::regex_search(body, m, std::regex(R"rx(<\s*preprocess\s+[^>]*command\s*=\s*"([^"]+)"[^>]*/>)rx")))
    pre = trim_copy(m[1].str());
  else
    pre.clear();
  if (std::regex_search(body, m, std::regex(R"rx(<\s*postprocess\s+[^>]*command\s*=\s*"([^"]+)"[^>]*/>)rx")))
    post = trim_copy(m[1].str());
  else
    post.clear();
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

  const size_t sources_open = raw.find("<sources");
  if (sources_open != std::string::npos) {
    const size_t sources_open_gt = raw.find('>', sources_open);
    const size_t sources_close = raw.find("</sources>", sources_open_gt + 1);
    if (sources_open_gt == std::string::npos || sources_close == std::string::npos) {
      error = "invalid <sources> block";
      return false;
    }
    const std::string body = raw.substr(sources_open_gt + 1, sources_close - sources_open_gt - 1);
    std::regex src_re(R"rx(<\s*(file|glob)\s*([^>]*)>([\s\S]*?)</\s*\1\s*>)rx");
    for (std::sregex_iterator it(body.begin(), body.end(), src_re), end; it != end; ++it) {
      TargetDesc::SourceEntry se;
      se.kind = trim_copy((*it)[1].str());
      const std::string attrs = (*it)[2].str();
      std::string inner = (*it)[3].str();
      parse_stage_commands(inner, se.preprocess_command, se.postprocess_command);
      inner = std::regex_replace(inner, std::regex(R"rx(<\s*preprocess\s+[^>]*/>)rx"), "");
      inner = std::regex_replace(inner, std::regex(R"rx(<\s*postprocess\s+[^>]*/>)rx"), "");
      inner = trim_copy(inner);
      if (!attr_string(attrs, "from", se.from))
        se.from = inner;
      se.from = trim_copy(se.from);
      if (se.from.empty()) {
        error = "sources entry requires file path or from attribute";
        return false;
      }
      out.source_entries.push_back(se);
      if (se.kind == "file")
        out.sources.push_back(se.from);
    }
  } else {
    std::regex file_re(R"rx(<file\s*>\s*([^<]+)\s*</file\s*>)rx");
    for (std::sregex_iterator it(raw.begin(), raw.end(), file_re), end; it != end; ++it) {
      out.sources.push_back((*it)[1].str());
      out.source_entries.push_back({"file", trim_copy((*it)[1].str()), "", ""});
    }
  }

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
    std::regex item_re(R"rx(<\s*([A-Za-z_][A-Za-z0-9_]*)\s*([^>]*?)(?:/>|>([\s\S]*?)</\s*\1\s*>))rx");
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
      parse_stage_commands((*it)[3].str(), inc.preprocess_command, inc.postprocess_command);
      out.includes.push_back(std::move(inc));
    }

    // old style is intentionally not supported anymore
    std::regex old_style_re(R"rx(<\s*dir\s*>\s*[^<]+\s*</\s*dir\s*>)rx");
    if (std::regex_search(body, old_style_re)) {
      error = "old <includes><dir>path</dir></includes> syntax is not supported; use <dir from=\"...\" to=\"...\"/>";
      return false;
    }
  }

  const size_t assets_open = raw.find("<assets");
  if (assets_open != std::string::npos) {
    const size_t assets_open_gt = raw.find('>', assets_open);
    const size_t assets_close = raw.find("</assets>", assets_open_gt + 1);
    if (assets_open_gt == std::string::npos || assets_close == std::string::npos) {
      error = "invalid <assets> block";
      return false;
    }
    const std::string body = raw.substr(assets_open_gt + 1, assets_close - assets_open_gt - 1);
    std::regex item_re(R"rx(<\s*([A-Za-z_][A-Za-z0-9_]*)\s*([^>]*?)(?:/>|>([\s\S]*?)</\s*\1\s*>))rx");
    for (std::sregex_iterator it(body.begin(), body.end(), item_re), end; it != end; ++it) {
      TargetDesc::AssetEntry ae;
      ae.kind = trim_copy((*it)[1].str());
      if (!(ae.kind == "dir" || ae.kind == "file" || ae.kind == "glob")) {
        error = "unsupported assets entry: " + ae.kind + " (expected dir/file/glob)";
        return false;
      }
      const std::string attrs = (*it)[2].str();
      if (!attr_string(attrs, "from", ae.from)) {
        error = "assets entry requires from attribute";
        return false;
      }
      ae.from = trim_copy(ae.from);
      if (!attr_string(attrs, "to", ae.to))
        ae.to.clear();
      else
        ae.to = trim_copy(ae.to);
      parse_stage_commands((*it)[3].str(), ae.preprocess_command, ae.postprocess_command);
      out.assets.push_back(std::move(ae));
    }
  }

  error.clear();
  return true;
}

}  // namespace up
