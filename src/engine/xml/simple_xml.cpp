#include "simple_xml.hpp"

#include "paths.hpp"

#include <cctype>
#include <fstream>
#include <ostream>
#include <regex>
#include <sstream>

namespace up {

namespace {

std::string read_all(const std::filesystem::path& path, std::string& error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    error = "cannot open: " + to_posix_path_string(path);
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

std::string xml_escape_text(const std::string& s) {
  std::string o;
  o.reserve(s.size() + 8);
  for (unsigned char uc : s) {
    const char c = static_cast<char>(uc);
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
    }
  }
  return o;
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

  std::regex cmake_re(R"rx(<cmake\s+([^>]+?)/\s*>)rx");
  for (std::sregex_iterator it(raw.begin(), raw.end(), cmake_re), end; it != end; ++it) {
    const std::string attrs = (*it)[1].str();
    std::string sd;
    if (!attr_string(attrs, "source_dir", sd))
      continue;
    sd = trim_copy(sd);
    if (sd.empty())
      continue;
    PackageExternalCmake c;
    c.source_dir = std::move(sd);
    out.external_cmake = std::move(c);
    break;
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

  const size_t prebuilt_open = raw.find("<prebuilt");
  if (prebuilt_open != std::string::npos) {
    const size_t prebuilt_gt = raw.find('>', prebuilt_open);
    if (prebuilt_gt != std::string::npos) {
      const std::string head = raw.substr(prebuilt_open, prebuilt_gt - prebuilt_open + 1);
      TargetDesc::PrebuiltDesc pb;
      attr_string(head, "import_lib", pb.import_lib);
      attr_string(head, "location", pb.location);
      attr_string(head, "dll", pb.dll);
      pb.import_lib = trim_copy(pb.import_lib);
      pb.location = trim_copy(pb.location);
      pb.dll = trim_copy(pb.dll);
      if (!pb.import_lib.empty() || !pb.location.empty() || !pb.dll.empty())
        out.prebuilt = std::move(pb);
    }
  }

  const size_t install_open = raw.find("<install");
  if (install_open != std::string::npos) {
    const size_t install_gt = raw.find('>', install_open);
    if (install_gt != std::string::npos) {
      const std::string head = raw.substr(install_open, install_gt - install_open + 1);
      std::string art;
      if (attr_string(head, "artifact", art)) {
        TargetDesc::InstalledWrapDesc iw;
        iw.artifact = trim_copy(art);
        attr_string(head, "implib", iw.implib);
        iw.implib = trim_copy(iw.implib);
        const size_t iface_open = raw.find("<interface_include");
        if (iface_open != std::string::npos) {
          const size_t iface_gt = raw.find('>', iface_open);
          if (iface_gt != std::string::npos) {
            const std::string ih = raw.substr(iface_open, iface_gt - iface_open + 1);
            attr_string(ih, "dir", iw.interface_include);
            iw.interface_include = trim_copy(iw.interface_include);
          }
        }
        if (!iw.artifact.empty())
          out.installed_wrap = std::move(iw);
      }
    }
  }

  error.clear();
  return true;
}

bool write_package_xml(std::ostream& out, const PackageDesc& pkg) {
  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  out << "<package name=\"" << xml_escape_text(pkg.name) << "\" version=\"" << xml_escape_text(pkg.version) << "\">\n";
  for (const auto& d : pkg.dependencies) {
    out << "  <dependency name=\"" << xml_escape_text(d.first) << "\" optional=\""
        << (d.second ? "true" : "false") << "\"/>\n";
  }
  if (pkg.external_cmake.has_value()) {
    out << "  <cmake source_dir=\"" << xml_escape_text(pkg.external_cmake->source_dir) << "\"/>\n";
  }
  out << "</package>\n";
  return static_cast<bool>(out);
}

bool write_target_xml(std::ostream& out, const TargetDesc& desc) {
  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  out << "<target name=\"" << xml_escape_text(desc.name) << "\" type=\"" << xml_escape_text(desc.type) << "\">\n";
  const bool skip_sources =
      desc.type == "asset_bundle" || desc.type == "imported_static_library" || desc.type == "imported_shared_library" ||
      desc.type == "imported_installed_static_library" || desc.type == "imported_installed_shared_library";
  if (!skip_sources || !desc.source_entries.empty() || !desc.sources.empty()) {
    out << "  <sources>\n";
    if (!desc.source_entries.empty()) {
      for (const auto& se : desc.source_entries) {
        if (se.kind == "glob") {
          out << "    <glob from=\"" << xml_escape_text(se.from) << "\"/>\n";
        } else {
          out << "    <file>" << xml_escape_text(se.from.empty() ? "" : se.from) << "</file>\n";
        }
      }
    } else {
      for (const auto& s : desc.sources)
        out << "    <file>" << xml_escape_text(s) << "</file>\n";
    }
    out << "  </sources>\n";
  }
  if (desc.prebuilt.has_value()) {
    const auto& pb = *desc.prebuilt;
    out << "  <prebuilt";
    if (!pb.import_lib.empty())
      out << " import_lib=\"" << xml_escape_text(pb.import_lib) << "\"";
    if (!pb.location.empty())
      out << " location=\"" << xml_escape_text(pb.location) << "\"";
    if (!pb.dll.empty())
      out << " dll=\"" << xml_escape_text(pb.dll) << "\"";
    out << "/>\n";
  }
  if (desc.installed_wrap.has_value()) {
    const auto& iw = *desc.installed_wrap;
    out << "  <install artifact=\"" << xml_escape_text(iw.artifact) << "\"";
    if (!iw.implib.empty())
      out << " implib=\"" << xml_escape_text(iw.implib) << "\"";
    out << "/>\n";
    if (!iw.interface_include.empty())
      out << "  <interface_include dir=\"" << xml_escape_text(iw.interface_include) << "\"/>\n";
  }
  if (!desc.dependencies.empty()) {
    for (const auto& d : desc.dependencies)
      out << "  <dependency name=\"" << xml_escape_text(d) << "\"/>\n";
  }
  if (!desc.includes.empty()) {
    out << "  <includes>\n";
    for (const auto& inc : desc.includes) {
      out << "    <" << inc.kind << " from=\"" << xml_escape_text(inc.from) << "\"";
      if (!inc.to.empty())
        out << " to=\"" << xml_escape_text(inc.to) << "\"";
      out << "/>\n";
    }
    out << "  </includes>\n";
  } else if (desc.type == "executable" || desc.type == "static_library" || desc.type == "shared_library") {
    out << "  <includes>\n";
    out << "    <dir from=\".\"/>\n";
    out << "  </includes>\n";
  }
  if (!desc.assets.empty()) {
    out << "  <assets>\n";
    for (const auto& ae : desc.assets) {
      out << "    <" << ae.kind << " from=\"" << xml_escape_text(ae.from) << "\"";
      if (!ae.to.empty())
        out << " to=\"" << xml_escape_text(ae.to) << "\"";
      out << "/>\n";
    }
    out << "  </assets>\n";
  }
  out << "</target>\n";
  return static_cast<bool>(out);
}

bool write_package_xml(const std::filesystem::path& path, const PackageDesc& pkg, std::string& error) {
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) {
    error = "cannot write: " + to_posix_path_string(path);
    return false;
  }
  write_package_xml(f, pkg);
  error.clear();
  return true;
}

bool write_target_xml(const std::filesystem::path& path, const TargetDesc& desc, std::string& error) {
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) {
    error = "cannot write: " + to_posix_path_string(path);
    return false;
  }
  write_target_xml(f, desc);
  error.clear();
  return true;
}

}  // namespace up
