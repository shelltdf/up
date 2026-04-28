#include "redist_emit.hpp"

#include "paths.hpp"
#include "simple_xml.hpp"

#include <cctype>
#include <fstream>
#include <sstream>

namespace gz {

namespace {

std::string json_escape_string(const std::string& s) {
  std::string o;
  o.reserve(s.size() + 8);
  for (unsigned char c : s) {
    if (c == '\\')
      o += "\\\\";
    else if (c == '"')
      o += "\\\"";
    else if (c == '\n')
      o += "\\n";
    else if (c == '\r')
      o += "\\r";
    else if (c == '\t')
      o += "\\t";
    else
      o.push_back(static_cast<char>(c));
  }
  return o;
}

bool extract_json_string(const std::string& raw, const std::string& key, std::string& out) {
  const std::string needle = "\"" + key + "\":\"";
  const size_t p = raw.find(needle);
  if (p == std::string::npos)
    return false;
  size_t i = p + needle.size();
  std::string v;
  while (i < raw.size()) {
    const char c = raw[i];
    if (c == '\\' && i + 1 < raw.size()) {
      const char n = raw[i + 1];
      if (n == 'n')
        v.push_back('\n');
      else if (n == 'r')
        v.push_back('\r');
      else if (n == 't')
        v.push_back('\t');
      else if (n == '\\' || n == '"')
        v.push_back(n);
      else
        v.push_back(n);
      i += 2;
      continue;
    }
    if (c == '"')
      break;
    v.push_back(c);
    ++i;
  }
  out = std::move(v);
  return true;
}

bool extract_json_int(const std::string& raw, const std::string& key, int& out) {
  const std::string needle = "\"" + key + "\":";
  const size_t p = raw.find(needle);
  if (p == std::string::npos)
    return false;
  size_t i = p + needle.size();
  while (i < raw.size() && std::isspace(static_cast<unsigned char>(raw[i])))
    ++i;
  int v = 0;
  if (i >= raw.size() || !std::isdigit(static_cast<unsigned char>(raw[i])))
    return false;
  while (i < raw.size() && std::isdigit(static_cast<unsigned char>(raw[i]))) {
    v = v * 10 + (raw[i] - '0');
    ++i;
  }
  out = v;
  return true;
}

void skip_ws_comma(const std::string& s, size_t& i) {
  while (i < s.size() && (std::isspace(static_cast<unsigned char>(s[i])) || s[i] == ','))
    ++i;
}

bool parse_one_target_object(const std::string& obj, GzRedistManifestTarget& t, std::string& err) {
  (void)err;
  extract_json_string(obj, "original_name", t.original_name);
  extract_json_string(obj, "emit_name", t.emit_name);
  extract_json_string(obj, "emit_subdir", t.emit_subdir);
  extract_json_string(obj, "emit_type", t.emit_type);
  extract_json_string(obj, "install_rel_import_lib", t.install_rel_import_lib);
  extract_json_string(obj, "install_rel_location", t.install_rel_location);
  extract_json_string(obj, "install_rel_dll", t.install_rel_dll);
  std::string u;
  if (extract_json_string(obj, "use_installed_wrap", u))
    t.use_installed_wrap = (u == "1" || u == "true");
  extract_json_string(obj, "install_rel_artifact", t.install_rel_artifact);
  extract_json_string(obj, "install_rel_implib", t.install_rel_implib);
  extract_json_string(obj, "installed_iface_include", t.installed_iface_include);
  std::string deps;
  const std::string dk = "\"dependency_names\":[";
  const size_t dp = obj.find(dk);
  if (dp != std::string::npos) {
    size_t j = dp + dk.size();
    skip_ws_comma(obj, j);
    while (j < obj.size() && obj[j] != ']') {
      if (obj[j] == '"') {
        ++j;
        std::string name;
        while (j < obj.size() && obj[j] != '"') {
          if (obj[j] == '\\' && j + 1 < obj.size()) {
            name.push_back(obj[j + 1]);
            j += 2;
          } else {
            name.push_back(obj[j++]);
          }
        }
        if (j < obj.size() && obj[j] == '"')
          ++j;
        if (!name.empty())
          t.dependency_names.push_back(std::move(name));
      } else
        ++j;
      skip_ws_comma(obj, j);
    }
  }
  return !t.emit_name.empty() && !t.emit_type.empty();
}

bool parse_targets_array(const std::string& raw, std::vector<GzRedistManifestTarget>& targets, std::string& err) {
  const std::string key = "\"targets\":[";
  const size_t p = raw.find(key);
  if (p == std::string::npos) {
    err = "missing \"targets\" array";
    return false;
  }
  size_t i = p + key.size();
  skip_ws_comma(raw, i);
  if (i < raw.size() && raw[i] == ']')
    return true;
  while (i < raw.size()) {
    skip_ws_comma(raw, i);
    if (i < raw.size() && raw[i] == ']')
      break;
    if (i >= raw.size() || raw[i] != '{') {
      err = "malformed targets array";
      return false;
    }
    int depth = 1;
    const size_t start = i;
    ++i;
    while (i < raw.size() && depth > 0) {
      if (raw[i] == '{')
        ++depth;
      else if (raw[i] == '}')
        --depth;
      ++i;
    }
    const std::string obj = raw.substr(start, i - start);
    GzRedistManifestTarget t;
    if (!parse_one_target_object(obj, t, err)) {
      err = "invalid target object in manifest";
      return false;
    }
    targets.push_back(std::move(t));
    skip_ws_comma(raw, i);
  }
  return true;
}

bool parse_deps_array(const std::string& raw, std::vector<GzRedistManifestDep>& deps) {
  const std::string key = "\"package_dependencies\":[";
  const size_t p = raw.find(key);
  if (p == std::string::npos)
    return true;
  size_t i = p + key.size();
  skip_ws_comma(raw, i);
  if (i < raw.size() && raw[i] == ']')
    return true;
  while (i < raw.size()) {
    skip_ws_comma(raw, i);
    if (i < raw.size() && raw[i] == ']')
      break;
    if (i >= raw.size() || raw[i] != '{') {
      return false;
    }
    int depth = 1;
    const size_t start = i;
    ++i;
    while (i < raw.size() && depth > 0) {
      if (raw[i] == '{')
        ++depth;
      else if (raw[i] == '}')
        --depth;
      ++i;
    }
    const std::string obj = raw.substr(start, i - start);
    GzRedistManifestDep d;
    extract_json_string(obj, "name", d.name);
    std::string opt;
    if (extract_json_string(obj, "optional", opt))
      d.optional = (opt == "1" || opt == "true");
    if (!d.name.empty())
      deps.push_back(std::move(d));
    skip_ws_comma(raw, i);
  }
  return true;
}

std::string rebase_install_rel_to_target_dir(const std::filesystem::path& install_root,
                                             const std::filesystem::path& target_xml_dir,
                                             const std::string& install_rel) {
  if (install_rel.empty())
    return {};
  std::error_code ec;
  const std::filesystem::path abs = install_root / std::filesystem::path(install_rel);
  const std::filesystem::path rel = std::filesystem::relative(abs, target_xml_dir, ec);
  if (ec)
    return install_rel;
  return to_posix_path_string(rel.lexically_normal());
}

bool file_exists_under(const std::filesystem::path& install_root, const std::string& install_rel) {
  if (install_rel.empty())
    return true;
  std::error_code ec;
  return std::filesystem::is_regular_file(install_root / std::filesystem::path(install_rel), ec);
}

}  // namespace

bool write_gz_redist_manifest_json(const std::filesystem::path& path, const GzRedistManifest& m, std::string& error) {
  std::ostringstream o;
  o << "{\"schema\":" << m.schema_version << ",\"package\":\"" << json_escape_string(m.package_name) << "\",\"version\":\""
    << json_escape_string(m.package_version) << "\",\"os\":\"" << json_escape_string(m.layout.os) << "\",\"cpu\":\""
    << json_escape_string(m.layout.cpu) << "\",\"build_system\":\"" << json_escape_string(m.layout.build_system) << "\",\"toolchain\":\""
    << json_escape_string(m.layout.toolchain) << "\",\"link\":\"" << json_escape_string(m.layout.link) << "\",\"config\":\""
    << json_escape_string(m.layout.config) << "\",\"crt\":\"" << json_escape_string(m.layout.crt) << "\",\"package_dependencies\":[";
  for (size_t i = 0; i < m.package_dependencies.size(); ++i) {
    if (i)
      o << ',';
    o << "{\"name\":\"" << json_escape_string(m.package_dependencies[i].name) << "\",\"optional\":\""
      << (m.package_dependencies[i].optional ? "true" : "false") << "\"}";
  }
  o << "],\"targets\":[";
  for (size_t ti = 0; ti < m.targets.size(); ++ti) {
    const auto& t = m.targets[ti];
    if (ti)
      o << ',';
    o << "{\"original_name\":\"" << json_escape_string(t.original_name) << "\",\"emit_name\":\""
      << json_escape_string(t.emit_name) << "\",\"emit_subdir\":\"" << json_escape_string(t.emit_subdir) << "\",\"emit_type\":\""
      << json_escape_string(t.emit_type) << "\",\"install_rel_import_lib\":\"" << json_escape_string(t.install_rel_import_lib)
      << "\",\"install_rel_location\":\"" << json_escape_string(t.install_rel_location) << "\",\"install_rel_dll\":\""
      << json_escape_string(t.install_rel_dll) << "\",\"use_installed_wrap\":\"" << (t.use_installed_wrap ? "true" : "false")
      << "\",\"install_rel_artifact\":\"" << json_escape_string(t.install_rel_artifact) << "\",\"install_rel_implib\":\""
      << json_escape_string(t.install_rel_implib) << "\",\"installed_iface_include\":\""
      << json_escape_string(t.installed_iface_include) << "\",\"dependency_names\":[";
    for (size_t di = 0; di < t.dependency_names.size(); ++di) {
      if (di)
        o << ',';
      o << '"' << json_escape_string(t.dependency_names[di]) << '"';
    }
    o << "]}";
  }
  o << "]}";
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) {
    error = "cannot write manifest: " + to_posix_path_string(path);
    return false;
  }
  f << o.str();
  error.clear();
  return static_cast<bool>(f);
}

bool read_gz_redist_manifest_json(const std::filesystem::path& path, GzRedistManifest& m, std::string& error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    error = "cannot read manifest: " + to_posix_path_string(path);
    return false;
  }
  std::ostringstream buf;
  buf << in.rdbuf();
  const std::string raw = buf.str();
  if (!extract_json_int(raw, "schema", m.schema_version) || m.schema_version < 1 || m.schema_version > 3) {
    error = "manifest schema must be 1, 2, or 3";
    return false;
  }
  if (!extract_json_string(raw, "package", m.package_name) || m.package_name.empty()) {
    error = "manifest missing package";
    return false;
  }
  extract_json_string(raw, "version", m.package_version);
  auto decompose_to_layout = [](const std::string& composed, GzBinaryLayout& lay) {
    std::string os, cpu, bs, tc, link, conf, crt;
    if (try_decompose_compose_arch_tag(composed, os, cpu, bs, tc, link, conf, crt)) {
      lay.os = std::move(os);
      lay.cpu = std::move(cpu);
      lay.build_system = std::move(bs);
      lay.toolchain = std::move(tc);
      lay.link = std::move(link);
      lay.config = std::move(conf);
      lay.crt = std::move(crt);
    }
  };

  if (m.schema_version == 1) {
    std::string composed;
    extract_json_string(raw, "arch", composed);
    decompose_to_layout(composed, m.layout);
  } else {
    extract_json_string(raw, "os", m.layout.os);
    extract_json_string(raw, "cpu", m.layout.cpu);
    extract_json_string(raw, "build_system", m.layout.build_system);
    extract_json_string(raw, "toolchain", m.layout.toolchain);
    extract_json_string(raw, "link", m.layout.link);
    extract_json_string(raw, "config", m.layout.config);
    extract_json_string(raw, "crt", m.layout.crt);
    if (m.layout.os.empty()) {
      std::string idl;
      if (extract_json_string(raw, "install_dir_leaf", idl) && !idl.empty())
        decompose_to_layout(idl, m.layout);
    }
  }
  m.package_dependencies.clear();
  if (!parse_deps_array(raw, m.package_dependencies)) {
    error = "manifest package_dependencies parse error";
    return false;
  }
  m.targets.clear();
  if (!parse_targets_array(raw, m.targets, error))
    return false;
  error.clear();
  return true;
}

int emit_gz_redistribution_xml(const std::filesystem::path& install_root, const GzRedistManifest& m, std::string& error) {
  if (m.targets.empty()) {
    error = "redistribution manifest has no targets";
    return 3;
  }
  const std::filesystem::path redist_root = install_root / "gz-redist";
  std::error_code ec;
  std::filesystem::create_directories(redist_root, ec);
  if (ec) {
    error = "cannot create gz-redist: " + ec.message();
    return 4;
  }

  for (const auto& t : m.targets) {
    if (t.use_installed_wrap) {
      if (!file_exists_under(install_root, t.install_rel_artifact)) {
        error = "missing installed artifact: " + t.install_rel_artifact;
        return 5;
      }
      if (!t.install_rel_implib.empty() && !file_exists_under(install_root, t.install_rel_implib)) {
        error = "missing installed implib: " + t.install_rel_implib;
        return 5;
      }
    } else {
      if (!t.install_rel_import_lib.empty() && !file_exists_under(install_root, t.install_rel_import_lib)) {
        error = "missing prebuilt import_lib: " + t.install_rel_import_lib;
        return 6;
      }
      if (!t.install_rel_location.empty() && !file_exists_under(install_root, t.install_rel_location)) {
        error = "missing prebuilt location: " + t.install_rel_location;
        return 6;
      }
      if (!t.install_rel_dll.empty() && !file_exists_under(install_root, t.install_rel_dll)) {
        error = "missing prebuilt dll: " + t.install_rel_dll;
        return 6;
      }
    }
  }

  PackageDesc pkg;
  pkg.name = m.package_name;
  pkg.version = m.package_version.empty() ? "0.0.0" : m.package_version;
  for (const auto& d : m.package_dependencies)
    pkg.dependencies.emplace_back(d.name, d.optional);
  std::string werr;
  if (!write_package_xml(redist_root / "package.xml", pkg, werr)) {
    error = werr;
    return 7;
  }

  for (const auto& t : m.targets) {
    const std::filesystem::path td = redist_root / t.emit_subdir;
    std::filesystem::create_directories(td, ec);
    if (ec) {
      error = "cannot create target dir: " + ec.message();
      return 8;
    }
    TargetDesc tdsc;
    tdsc.name = t.emit_name;
    tdsc.type = t.emit_type;
    for (const auto& dn : t.dependency_names) {
      TargetDesc::DependencyEntry de;
      de.name = dn;
      de.visibility = "private";
      tdsc.dependencies.push_back(std::move(de));
    }
    if (t.use_installed_wrap) {
      TargetDesc::InstalledWrapDesc iw;
      iw.artifact = t.install_rel_artifact;
      iw.implib = t.install_rel_implib;
      iw.interface_include = t.installed_iface_include;
      iw.layout = m.layout;
      tdsc.installed_wrap = std::move(iw);
    } else {
      TargetDesc::PrebuiltDesc pb;
      pb.import_lib = rebase_install_rel_to_target_dir(install_root, td, t.install_rel_import_lib);
      pb.location = rebase_install_rel_to_target_dir(install_root, td, t.install_rel_location);
      pb.dll = rebase_install_rel_to_target_dir(install_root, td, t.install_rel_dll);
      pb.layout = m.layout;
      tdsc.prebuilt = std::move(pb);
    }
    if (!write_target_xml(td / "target.xml", tdsc, werr)) {
      error = werr;
      return 9;
    }
  }
  error.clear();
  return 0;
}

}  // namespace gz
