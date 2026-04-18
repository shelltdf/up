#include "configure.hpp"

#include "commands_common.hpp"
#include "cli_verbose.hpp"
#include "core/backend_dispatch.hpp"
#include "lang.hpp"
#include "path_check.hpp"
#include "paths.hpp"
#include "simple_xml.hpp"
#include "var_subst.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <chrono>
#include <ctime>
#include <optional>
#include <vector>

namespace up {

void ensure_default_build_parallel_options(std::map<std::string, std::string>& opts);
unsigned parallel_jobs_for_build(const std::map<std::string, std::string>& opts);

namespace {

void collect_desc_files(const std::filesystem::path& root, std::vector<std::filesystem::path>& packages,
                        std::vector<std::filesystem::path>& targets) {
  if (!std::filesystem::exists(root))
    return;
  for (std::filesystem::recursive_directory_iterator it(root,
                                                         std::filesystem::directory_options::skip_permission_denied),
       end;
       it != end; ++it) {
    if (it->is_directory() && it->path().filename() == ".intermediate") {
      it.disable_recursion_pending();
      continue;
    }
    if (!it->is_regular_file())
      continue;
    const auto p = it->path();
    if (p.filename() == "package.xml")
      packages.push_back(p);
    else if (p.filename() == "target.xml")
      targets.push_back(p);
  }
}

std::filesystem::path nearest_package_parent(const std::filesystem::path& target_xml_path,
                                             const std::vector<std::filesystem::path>& package_files) {
  const auto t = target_xml_path.parent_path();
  const std::string ts = to_posix_path_string(t);
  std::filesystem::path best;
  for (const auto& pkg : package_files) {
    const auto parent = pkg.parent_path();
    const std::string ps = to_posix_path_string(parent);
    if (ts.rfind(ps, 0) == 0) {
      if (best.empty() || ps.size() > to_posix_path_string(best).size())
        best = parent;
    }
  }
  return best;
}

struct LoadedTarget {
  std::filesystem::path target_dir;
  std::string package_name;
  TargetDesc desc;
};

struct DepImportedHint {
  std::string library_abs;
  std::string include_abs;
};

DepImportedHint dep_imported_hint_from_targets(const std::string& dep_pkg_name,
                                               const std::map<std::string, std::filesystem::path>& package_name_to_dir,
                                               const std::vector<LoadedTarget>& all_targets,
                                               const std::string& arch);

bool split_dep_ref(const std::string& ref, const std::string& self_pkg, std::string& out_pkg, std::string& out_tgt) {
  const auto p = ref.find(':');
  if (p == std::string::npos) {
    out_pkg = self_pkg;
    out_tgt = ref;
  } else {
    out_pkg = ref.substr(0, p);
    out_tgt = ref.substr(p + 1);
  }
  return !out_pkg.empty() && !out_tgt.empty();
}

bool require_ascii_path(const std::filesystem::path& p) {
  if (path_has_non_ascii(p)) {
    std::cerr << lang::configure_path_non_ascii() << to_posix_path_string(p) << "\n";
    return false;
  }
  return true;
}

// True if candidate is root or strictly inside root (canonical paths).
bool path_is_under_tree(const std::filesystem::path& root, const std::filesystem::path& candidate) {
  std::error_code ec;
  const std::filesystem::path rc = std::filesystem::weakly_canonical(std::filesystem::absolute(root), ec);
  const std::filesystem::path cc = std::filesystem::weakly_canonical(std::filesystem::absolute(candidate), ec);
  if (ec || rc.empty())
    return false;
  const std::filesystem::path rel = std::filesystem::relative(cc, rc, ec);
  if (ec)
    return false;
  if (rel.empty() || rel == ".")
    return true;
  for (const auto& seg : rel) {
    if (seg == "..")
      return false;
  }
  return true;
}

std::string trim_ascii_ws(std::string s) {
  size_t b = 0;
  while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b])))
    ++b;
  size_t e = s.size();
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])))
    --e;
  return s.substr(b, e - b);
}

// Paths relative to CMAKE_INSTALL_PREFIX (forward slashes, no leading slash).
std::string normalize_install_rel_path(std::string s) {
  s = trim_ascii_ws(std::move(s));
  for (char& c : s) {
    if (c == '\\')
      c = '/';
  }
  while (!s.empty() && s.front() == '/')
    s.erase(s.begin());
  return s;
}

std::string cmake_var_prefix_from_dep_name(const std::string& dep_name) {
  std::string out;
  out.reserve(dep_name.size());
  for (char c : dep_name) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
      out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    else
      out.push_back('_');
  }
  return out;
}

bool is_lib_rel_path(std::string abs_like) {
  for (char& c : abs_like)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return abs_like.size() >= 4 && abs_like.substr(abs_like.size() - 4) == ".lib";
}

std::string infer_implib_from_dll(const std::filesystem::path& dep_prefix, const std::string& dll_abs_like) {
  std::filesystem::path dll_path(dll_abs_like);
  const std::filesystem::path stem = dll_path.stem();
  const std::filesystem::path guess1 = std::filesystem::absolute(dep_prefix / "lib" / (stem.string() + ".lib")).lexically_normal();
  const std::filesystem::path guess2 = std::filesystem::absolute(dll_path.parent_path() / (stem.string() + ".lib")).lexically_normal();
  if (!stem.empty())
    return to_posix_path_string(guess1);
  return to_posix_path_string(guess2);
}

std::string sanitize_ep_base_name(std::string s, const char* fallback) {
  for (char& c : s) {
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'))
      c = '_';
  }
  if (s.empty())
    s = fallback;
  return s;
}

void append_upstream_opts_from_map(ConfigureExternalCmake& ec, const std::map<std::string, std::string>& opts) {
  for (const auto& kv : opts) {
    if (kv.first.rfind("UPSTREAM_", 0) != 0)
      continue;
    std::string k = kv.first.substr(9);
    if (!k.empty())
      ec.upstream_cmake_args.emplace_back(std::move(k), kv.second);
  }
}

void append_dep_imported_hints(ConfigureExternalCmake& ec,
                               const std::set<std::string>& declared_dep_pkgs,
                               const std::map<std::string, std::filesystem::path>& package_name_to_dir,
                               const std::vector<LoadedTarget>& all_targets,
                               const std::string& arch) {
  for (const auto& dep_pkg : declared_dep_pkgs) {
    const auto hint = dep_imported_hint_from_targets(dep_pkg, package_name_to_dir, all_targets, arch);
    const std::string prefix = cmake_var_prefix_from_dep_name(dep_pkg);
    if (prefix.empty())
      continue;
    if (!hint.library_abs.empty()) {
      ec.upstream_cmake_args.emplace_back(prefix + "_LIBRARY", hint.library_abs);
      ec.upstream_cmake_args.emplace_back(prefix + "_LIBRARY_RELEASE", hint.library_abs);
      ec.upstream_cmake_args.emplace_back(prefix + "_LIBRARY_DEBUG", hint.library_abs);
      ec.upstream_cmake_args.emplace_back(prefix + "_LIBRARIES", hint.library_abs);
    }
    if (!hint.include_abs.empty()) {
      ec.upstream_cmake_args.emplace_back(prefix + "_INCLUDE_DIR", hint.include_abs);
      ec.upstream_cmake_args.emplace_back(prefix + "_INCLUDE_DIRS", hint.include_abs);
    }
  }
}

bool resolve_external_cmake_source_dir(const std::filesystem::path& package_dir,
                                       const std::string& source_dir,
                                       const char* invalid_message_prefix,
                                       const std::string& invalid_message_detail,
                                       std::filesystem::path& out_src_abs) {
  std::filesystem::path src_abs = (package_dir / std::filesystem::path(source_dir)).lexically_normal();
  std::error_code ec_path;
  src_abs = std::filesystem::weakly_canonical(std::filesystem::absolute(src_abs), ec_path);
  if (ec_path || !std::filesystem::is_directory(src_abs)) {
    std::cerr << invalid_message_prefix << invalid_message_detail << "\n";
    return false;
  }
  if (!std::filesystem::exists(src_abs / "CMakeLists.txt", ec_path)) {
    std::cerr << "configure: <cmake> directory has no CMakeLists.txt: " << to_posix_path_string(src_abs) << "\n";
    return false;
  }
  out_src_abs = std::move(src_abs);
  return true;
}

bool configure_installed_import_target_model(const LoadedTarget& lt,
                                             bool has_primary_external_cmake,
                                             bool imported_installed_shared,
                                             ConfigureTargetModel& tm) {
  if (!has_primary_external_cmake) {
    std::cerr << "configure: target \"" << lt.desc.name << "\" (" << lt.desc.type << ") ";
#if UP_DISABLE_PACKAGE_XML_CMAKE
    std::cerr << "uses imported_installed_* which needs an upstream install prefix; package.xml `<cmake/>` is "
                 "**disabled in this build** (UP_DISABLE_PACKAGE_XML_CMAKE).\n";
#else
    std::cerr << "requires package.xml <cmake> so the upstream project installs into the same prefix first.\n";
#endif
    return false;
  }
  if (!lt.desc.installed_wrap.has_value()) {
    std::cerr << "configure: target \"" << lt.desc.name << "\" (" << lt.desc.type << ") requires <install artifact=\"...\"/> in "
              << to_posix_path_string(lt.target_dir / "target.xml") << "\n";
    return false;
  }
  const std::string rel_art = normalize_install_rel_path(lt.desc.installed_wrap->artifact);
  if (rel_art.empty()) {
    std::cerr << "configure: target \"" << lt.desc.name << "\": <install artifact=\"...\"/> must not be empty\n";
    return false;
  }
  tm.imported_prebuilt = true;
  tm.imported_from_install_prefix = true;
  tm.install_rel_artifact = rel_art;
  tm.imported_location = std::string("${CMAKE_INSTALL_PREFIX}/") + rel_art;
  const std::string iface = normalize_install_rel_path(lt.desc.installed_wrap->interface_include);
  if (!iface.empty())
    tm.install_rel_interface_include = iface;
  if (imported_installed_shared) {
#if defined(_WIN32)
    const std::string rel_imp = normalize_install_rel_path(lt.desc.installed_wrap->implib);
    if (rel_imp.empty()) {
      std::cerr << "configure: imported_installed_shared_library \"" << lt.desc.name
                << "\" on Windows requires <install implib=\"...\"/> (import .lib under install prefix).\n";
      return false;
    }
    tm.install_rel_implib = rel_imp;
    tm.imported_implib = std::string("${CMAKE_INSTALL_PREFIX}/") + rel_imp;
    tm.imported_dll = tm.imported_location;
#endif
  }
  return true;
}

DepImportedHint dep_imported_hint_from_targets(const std::string& dep_pkg_name,
                                               const std::map<std::string, std::filesystem::path>& package_name_to_dir,
                                               const std::vector<LoadedTarget>& all_targets,
                                               const std::string& arch) {
  DepImportedHint hint;
  const auto pit = package_name_to_dir.find(dep_pkg_name);
  if (pit == package_name_to_dir.end())
    return hint;
  const std::filesystem::path dep_prefix = std::filesystem::absolute(default_install_root(pit->second) / arch);
  auto resolve_rel_abs = [&](const std::string& rel_raw) -> std::string {
    const std::string rel = normalize_install_rel_path(rel_raw);
    if (rel.empty())
      return {};
    const std::filesystem::path abs = std::filesystem::absolute(dep_prefix / std::filesystem::path(rel)).lexically_normal();
    return to_posix_path_string(abs);
  };
  for (const auto& lt : all_targets) {
    if (lt.package_name != dep_pkg_name)
      continue;
    if (lt.desc.type != "imported_installed_static_library" && lt.desc.type != "imported_installed_shared_library")
      continue;
    if (!lt.desc.installed_wrap.has_value() || lt.desc.installed_wrap->artifact.empty())
      continue;
    if (hint.include_abs.empty() && !lt.desc.installed_wrap->interface_include.empty())
      hint.include_abs = resolve_rel_abs(lt.desc.installed_wrap->interface_include);
    if (!lt.desc.installed_wrap->implib.empty()) {
      const std::string implib_abs = resolve_rel_abs(lt.desc.installed_wrap->implib);
      if (!implib_abs.empty()) {
        hint.library_abs = implib_abs;
        return hint;
      }
    }
    const std::string art_abs = resolve_rel_abs(lt.desc.installed_wrap->artifact);
    if (!art_abs.empty()) {
      if (is_lib_rel_path(art_abs)) {
        hint.library_abs = art_abs;
        return hint;
      }
      std::string low = art_abs;
      for (char& c : low)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      if (low.size() >= 4 && low.substr(low.size() - 4) == ".dll") {
        const std::string guessed = infer_implib_from_dll(dep_prefix, art_abs);
        if (!guessed.empty()) {
          hint.library_abs = guessed;
          return hint;
        }
      }
    }
  }
  return hint;
}

void append_cmake_prefix_unique(std::vector<std::filesystem::path>& dedup,
                                const std::filesystem::path& candidate,
                                const std::filesystem::path& cwd) {
  std::error_code ec;
  std::filesystem::path c = candidate;
  if (c.is_relative())
    c = std::filesystem::weakly_canonical(std::filesystem::absolute(cwd / c), ec);
  else
    c = std::filesystem::weakly_canonical(std::filesystem::absolute(c), ec);
  if (ec)
    c = std::filesystem::absolute(candidate.is_relative() ? cwd / candidate : candidate);
  for (const auto& p : dedup) {
    if (p == c)
      return;
  }
  dedup.push_back(std::move(c));
}

// Primary install prefix first, then extra entries from UP_CMAKE_PREFIX_PATH (semicolon-separated on all platforms).
std::string merge_cmake_prefix_path_value(const std::filesystem::path& primary_install_abs,
                                          const std::vector<std::filesystem::path>& dep_install_prefixes,
                                          const std::map<std::string, std::string>& opts,
                                          const std::filesystem::path& cwd) {
  std::vector<std::filesystem::path> dedup;
  append_cmake_prefix_unique(dedup, primary_install_abs, cwd);
  for (const auto& p : dep_install_prefixes)
    append_cmake_prefix_unique(dedup, p, cwd);
  const auto it = opts.find("UP_CMAKE_PREFIX_PATH");
  if (it != opts.end() && !it->second.empty()) {
    const std::string& raw = it->second;
    size_t start = 0;
    while (start <= raw.size()) {
      const size_t sep = raw.find(';', start);
      const std::string piece =
          trim_ascii_ws(raw.substr(start, sep == std::string::npos ? std::string::npos : sep - start));
      if (!piece.empty() && piece != ".")
        append_cmake_prefix_unique(dedup, std::filesystem::path(piece), cwd);
      if (sep == std::string::npos)
        break;
      start = sep + 1;
    }
  }
  std::ostringstream oss;
  for (size_t i = 0; i < dedup.size(); ++i) {
    if (i)
      oss << ';';
    oss << to_posix_path_string(dedup[i]);
  }
  return oss.str();
}

// 写入缓存的 scan_roots 只表示「额外 --scan 根」；cwd 已由 cwd= 字段表示，不再重复写入。
std::vector<std::filesystem::path> scan_roots_for_cache_file(const std::filesystem::path& cwd,
                                                            const std::vector<std::filesystem::path>& roots) {
  std::vector<std::filesystem::path> out;
  std::error_code ec_cwd;
  const auto cwd_abs = std::filesystem::weakly_canonical(std::filesystem::absolute(cwd), ec_cwd);
  const std::string cwd_key = std::filesystem::absolute(cwd).lexically_normal().generic_string();
  for (const auto& r : roots) {
    std::error_code ec_r;
    const auto r_abs = std::filesystem::weakly_canonical(std::filesystem::absolute(r), ec_r);
    const bool same_canon = !ec_cwd && !ec_r && r_abs == cwd_abs;
    const bool same_key =
        std::filesystem::absolute(r).lexically_normal().generic_string() == cwd_key;
    if (same_canon || same_key)
      continue;
    out.push_back(r);
  }
  return out;
}

void write_up_cache(const std::filesystem::path& cache_path,
                    const std::filesystem::path& cwd,
                    const std::string& arch,
                    const std::string& package_name,
                    const std::filesystem::path& generated_file,
                    const std::vector<std::filesystem::path>& scan_roots,
                    const std::map<std::string, std::string>& options) {
  std::ofstream f(cache_path);
  if (!f) {
    std::cerr << "configure: warning: could not write " << to_posix_path_string(cache_path) << "\n";
    return;
  }
  f << "up.cache.version=1\n";
  f << "cwd=" << std::filesystem::absolute(cwd).generic_string() << "\n";
  f << "arch=" << arch << "\n";
  f << "package=" << package_name << "\n";
  f << "generated_file=" << std::filesystem::absolute(generated_file).generic_string() << "\n";
  f << "scan_roots=";
  for (size_t i = 0; i < scan_roots.size(); ++i) {
    if (i)
      f << ';';
    f << std::filesystem::absolute(scan_roots[i]).generic_string();
  }
  f << "\n";
  for (const auto& kv : options)
    f << kv.first << "=" << kv.second << "\n";
}

std::string mermaid_sanitize_id_chars(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    const unsigned char uc = static_cast<unsigned char>(c);
    if ((uc >= 'a' && uc <= 'z') || (uc >= 'A' && uc <= 'Z') || (uc >= '0' && uc <= '9'))
      out.push_back(c);
    else
      out.push_back('_');
  }
  return out;
}

std::string mermaid_node_id(const std::string& package_name) {
  return std::string("pkg_") + mermaid_sanitize_id_chars(package_name);
}

std::string mermaid_target_node_id(const std::string& package_name, const std::string& target_name) {
  return std::string("t_") + mermaid_sanitize_id_chars(package_name) + "__" + mermaid_sanitize_id_chars(target_name);
}

// Index-only diagrams: separate ids so they never collide with `t_*` link graphs.
std::string mermaid_exe_index_node_id(const std::string& package_name, const std::string& target_name) {
  return std::string("ex_") + mermaid_sanitize_id_chars(package_name) + "__" + mermaid_sanitize_id_chars(target_name);
}

std::string mermaid_lib_index_node_id(const std::string& package_name, const std::string& target_name) {
  return std::string("lb_") + mermaid_sanitize_id_chars(package_name) + "__" + mermaid_sanitize_id_chars(target_name);
}

// Text inside Mermaid node labels `["..."]` must not break quoting or bracket parsing.
std::string mermaid_escape_node_label(const std::string& s) {
  std::string o;
  o.reserve(s.size());
  for (unsigned char uc : s) {
    const char c = static_cast<char>(uc);
    if (c == '"' || c == '[' || c == ']' || c == '\n' || c == '\r')
      o.push_back('?');
    else
      o.push_back(c);
  }
  return o;
}

void emit_mermaid_target_index_diagrams(std::ofstream& f, const std::string& intra_pkg,
                                        const std::vector<std::string>& names_sorted, bool as_executable_role,
                                        std::size_t names_per_block) {
  if (names_sorted.empty())
    return;
  const std::size_t n = names_sorted.size();
  const std::size_t num_parts = (n + names_per_block - 1) / names_per_block;
  for (std::size_t part = 0; part < num_parts; ++part) {
    const std::size_t off = part * names_per_block;
    const std::size_t end = std::min(off + names_per_block, n);
    if (num_parts > 1)
      f << "_Index block " << (part + 1) << " / " << num_parts << " (" << (end - off) << " targets)_\n\n";
    f << "```mermaid\n";
    f << "flowchart LR\n";
    if (as_executable_role)
      f << "  subgraph EXG" << part << " [Executables]\n";
    else
      f << "  subgraph LBG" << part << " [Libraries]\n";
    f << "    direction TB\n";
    for (std::size_t j = off; j < end; ++j) {
      const std::string& nm = names_sorted[j];
      const std::string nid =
          as_executable_role ? mermaid_exe_index_node_id(intra_pkg, nm) : mermaid_lib_index_node_id(intra_pkg, nm);
      const std::string role = as_executable_role ? "exe" : "lib";
      f << "    " << nid << "[\"" << role << ": " << mermaid_escape_node_label(nm) << "\"]\n";
    }
    f << "  end\n";
    f << "```\n\n";
  }
}

void emit_target_link_mermaid_diagrams(std::ofstream& f, const std::string& intra_pkg,
                                       const std::vector<std::pair<std::string, std::string>>& edges_in,
                                       std::size_t edges_per_block) {
  if (edges_in.empty())
    return;
  std::vector<std::pair<std::string, std::string>> edges = edges_in;
  std::sort(edges.begin(), edges.end());
  edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
  const std::size_t n = edges.size();
  const std::size_t num_parts = (n + edges_per_block - 1) / edges_per_block;
  for (std::size_t part = 0; part < num_parts; ++part) {
    const std::size_t off = part * edges_per_block;
    const std::size_t end = std::min(off + edges_per_block, n);
    if (num_parts > 1)
      f << "_Mermaid block " << (part + 1) << " / " << num_parts << " (" << (end - off) << " edges)_\n\n";
    std::set<std::string> node_names;
    for (std::size_t j = off; j < end; ++j) {
      node_names.insert(edges[j].first);
      node_names.insert(edges[j].second);
    }
    f << "```mermaid\n";
    f << "flowchart LR\n";
    for (const auto& name : node_names) {
      f << "  " << mermaid_target_node_id(intra_pkg, name) << "[\"" << mermaid_escape_node_label(name) << "\"]\n";
    }
    for (std::size_t j = off; j < end; ++j) {
      const auto& e = edges[j];
      f << "  " << mermaid_target_node_id(intra_pkg, e.first) << " --> " << mermaid_target_node_id(intra_pkg, e.second)
        << "\n";
    }
    f << "```\n\n";
  }
}

void write_packages_md(const std::filesystem::path& out_path,
                       const std::vector<std::pair<std::filesystem::path, PackageDesc>>& loaded_packages,
                       const std::vector<std::filesystem::path>& scan_roots,
                       const std::vector<LoadedTarget>& all_targets,
                       const std::string& intra_package_target_graph_pkg,
                       const ConfigureGraphModel& graph_model,
                       const std::vector<LoadedTarget>& build_targets,
                       const std::map<std::string, std::string>& configure_opts) {
  std::ofstream f(out_path);
  if (!f) {
    std::cerr << "configure: warning: could not write " << to_posix_path_string(out_path) << "\n";
    return;
  }

  struct DepEdge {
    std::string to_pkg;
    bool optional = false;
    bool operator<(const DepEdge& other) const {
      if (to_pkg != other.to_pkg)
        return to_pkg < other.to_pkg;
      return optional < other.optional;
    }
  };
  std::map<std::string, std::set<DepEdge>> deps;
  std::map<std::string, std::filesystem::path> pkg_paths;
  std::map<std::string, std::filesystem::path> pkg_xml_paths;
  std::set<std::string> all_pkgs;
  for (const auto& pkg_pair : loaded_packages) {
    const auto& pkg_path = pkg_pair.first;
    const auto& pkg = pkg_pair.second;
    all_pkgs.insert(pkg.name);
    pkg_paths[pkg.name] = std::filesystem::absolute(pkg_path.parent_path()).lexically_normal();
    pkg_xml_paths[pkg.name] = std::filesystem::absolute(pkg_path).lexically_normal();
    deps[pkg.name];
    for (const auto& dep : pkg.dependencies) {
      if (dep.first.empty())
        continue;
      if (equals_ci(dep.first, "none"))
        continue;
      deps[pkg.name].insert({dep.first, dep.second});
      all_pkgs.insert(dep.first);
    }
  }

  const auto now = std::chrono::system_clock::now();
  const std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm tm_local{};
#if defined(_WIN32)
  localtime_s(&tm_local, &t);
#else
  localtime_r(&t, &tm_local);
#endif
  char time_buf[64]{};
  std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm_local);

  f << "# Package Dependencies\n\n";
  f << "Generated by `up configure` at `" << time_buf << "`.\n\n";
  if (!intra_package_target_graph_pkg.empty()) {
    f << "## Current package\n\n";
    f << "- **Name:** `" << intra_package_target_graph_pkg << "`\n";
    const auto it_dir = pkg_paths.find(intra_package_target_graph_pkg);
    const auto it_xml = pkg_xml_paths.find(intra_package_target_graph_pkg);
    if (it_dir != pkg_paths.end())
      f << "- **Root directory:** `" << it_dir->second.generic_string() << "`\n";
    if (it_xml != pkg_xml_paths.end())
      f << "- **`package.xml`:** `" << it_xml->second.generic_string() << "`\n";
    f << "\n";
  }
  f << "## Scan Roots\n\n";
  for (const auto& root : scan_roots) {
    f << "- `" << std::filesystem::absolute(root).lexically_normal().generic_string() << "`\n";
  }
  f << "\n";
  f << "## Package dependency graph (`package.xml`)\n\n";
  f << "Edges are taken only from `package.xml` files found under **Scan Roots**. An arrow `A --> B` means "
       "`B` is listed as a dependency of package `A` (dashed `A -.-> B` when optional). "
       "If a dependency package is not scanned here, it appears as a node without edges sourced from its own "
       "`package.xml`, so transitive package chains are incomplete unless those packages are also under the scan set.\n\n";
  f << "```mermaid\n";
  f << "flowchart LR\n";
  for (const auto& pkg_name : all_pkgs) {
    const auto pkg_id = mermaid_node_id(pkg_name);
    f << "  " << pkg_id << "[\"" << mermaid_escape_node_label(pkg_name) << "\"]\n";
  }
  for (const auto& kv : deps) {
    const auto src_id = mermaid_node_id(kv.first);
    for (const auto& dep : kv.second) {
      const auto dep_id = mermaid_node_id(dep.to_pkg);
      f << "  " << src_id << (dep.optional ? " -.-> " : " --> ") << dep_id << "\n";
    }
  }
  f << "```\n";

  f << "\n## Package Paths\n\n";
  f << "| Package | Directory | package.xml |\n";
  f << "|---|---|---|\n";
  for (const auto& pkg_name : all_pkgs) {
    const auto it_dir = pkg_paths.find(pkg_name);
    const auto it_xml = pkg_xml_paths.find(pkg_name);
    const std::string dir_text =
        (it_dir != pkg_paths.end()) ? ("`" + it_dir->second.generic_string() + "`") : "`(not in current scan set)`";
    const std::string xml_text =
        (it_xml != pkg_xml_paths.end()) ? ("`" + it_xml->second.generic_string() + "`") : "`(not in current scan set)`";
    f << "| `" << pkg_name << "` | " << dir_text << " | " << xml_text << " |\n";
  }

  auto md_table_cell = [](std::string s) {
    for (char& c : s) {
      if (c == '|' || c == '`' || c == '\n' || c == '\r')
        c = ' ';
    }
    return s;
  };

  f << "\n## Declared template variables (`<vars>`)\n\n";
  f << "Each **name** / **value** pair is a **default** for `@NAME@` substitution and `when=` (after built-ins). "
       "At configure, **package** `<vars>` apply, then **target** `<vars>`, then **`--opt` / `up_cache.txt`** for the "
       "same key (last wins). Allowed override keys include every `UP_*` / `UPSTREAM_*` option plus any **C-style "
       "identifier** except reserved cache lines (`cwd`, `arch`, `package`, …); see `up spec`.\n\n";
  bool any_vars = false;
  for (const auto& pp : loaded_packages) {
    if (!pp.second.vars.empty())
      any_vars = true;
  }
  if (!any_vars) {
    for (const auto& lt : all_targets) {
      if (!lt.desc.vars.empty()) {
        any_vars = true;
        break;
      }
    }
  }
  if (!any_vars) {
    f << "_No `<vars>` in this scan._\n";
  } else {
    f << "### Package `<vars>`\n\n";
    f << "| Package | Name | Default (XML) | Key also in configure opts map |\n";
    f << "|---|---|---|---|\n";
    for (const auto& pp : loaded_packages) {
      for (const auto& v : pp.second.vars) {
        const bool in_opts = configure_opts.find(v.first) != configure_opts.end();
        f << "| `" << md_table_cell(pp.second.name) << "` | `" << md_table_cell(v.first) << "` | `"
          << md_table_cell(v.second) << "` | " << (in_opts ? "yes" : "no") << " |\n";
      }
    }
    f << "\n### Target `<vars>`\n\n";
    f << "| Package | Target | Name | Default (XML) | Key also in configure opts map |\n";
    f << "|---|---|---|---|---|\n";
    for (const auto& lt : all_targets) {
      for (const auto& v : lt.desc.vars) {
        const bool in_opts = configure_opts.find(v.first) != configure_opts.end();
        f << "| `" << md_table_cell(lt.package_name) << "` | `" << md_table_cell(lt.desc.name) << "` | `"
          << md_table_cell(v.first) << "` | `" << md_table_cell(v.second) << "` | " << (in_opts ? "yes" : "no")
          << " |\n";
      }
    }
  }

  bool any_intra_target_dep = false;
  if (!intra_package_target_graph_pkg.empty()) {
    for (const auto& lt : all_targets) {
      if (lt.package_name != intra_package_target_graph_pkg)
        continue;
      for (const auto& dep : lt.desc.dependencies) {
        std::string dep_pkg;
        std::string dep_tgt;
        if (!split_dep_ref(dep.name, lt.package_name, dep_pkg, dep_tgt))
          continue;
        if (dep_pkg == intra_package_target_graph_pkg) {
          any_intra_target_dep = true;
          break;
        }
      }
      if (any_intra_target_dep)
        break;
    }
  }
  if (any_intra_target_dep) {
    f << "\n## Target dependency graph (`target.xml`, package \"" << intra_package_target_graph_pkg << "\")\n\n";
    f << "Only **intra-package** edges are shown: both endpoints are targets under package `" << intra_package_target_graph_pkg
       << "` (from `<dependency name=\"...\"/>` entries in each `target.xml`). Links to targets in other packages are "
          "omitted.\n\n";
    f << "```mermaid\n";
    f << "flowchart LR\n";
    std::set<std::pair<std::string, std::string>> target_nodes;
    for (const auto& lt : all_targets) {
      if (lt.package_name != intra_package_target_graph_pkg)
        continue;
      for (const auto& dep : lt.desc.dependencies) {
        std::string dep_pkg;
        std::string dep_tgt;
        if (!split_dep_ref(dep.name, lt.package_name, dep_pkg, dep_tgt))
          continue;
        if (dep_pkg != intra_package_target_graph_pkg)
          continue;
        target_nodes.insert({lt.package_name, lt.desc.name});
        target_nodes.insert({dep_pkg, dep_tgt});
      }
    }
    for (const auto& pr : target_nodes) {
      f << "  " << mermaid_target_node_id(pr.first, pr.second) << "[\"" << mermaid_escape_node_label(pr.second)
        << "\"]\n";
    }
    for (const auto& lt : all_targets) {
      if (lt.package_name != intra_package_target_graph_pkg)
        continue;
      const auto src_id = mermaid_target_node_id(lt.package_name, lt.desc.name);
      for (const auto& dep : lt.desc.dependencies) {
        std::string dep_pkg;
        std::string dep_tgt;
        if (!split_dep_ref(dep.name, lt.package_name, dep_pkg, dep_tgt))
          continue;
        if (dep_pkg != intra_package_target_graph_pkg)
          continue;
        f << "  " << src_id << " --> " << mermaid_target_node_id(dep_pkg, dep_tgt) << "\n";
      }
    }
    f << "```\n";
  } else if (!intra_package_target_graph_pkg.empty() && graph_model.targets.size() == build_targets.size()) {
    auto is_lib_target = [](const std::string& ty) {
      return ty == "static_library" || ty == "shared_library" || ty == "imported_static_library" ||
             ty == "imported_shared_library" || ty == "imported_installed_static_library" ||
             ty == "imported_installed_shared_library";
    };
    std::set<std::string> primary_lib_names;
    for (const auto& lt : all_targets) {
      if (lt.package_name == intra_package_target_graph_pkg && is_lib_target(lt.desc.type))
        primary_lib_names.insert(lt.desc.name);
    }
    std::vector<std::pair<std::string, std::string>> link_edges;
    for (size_t i = 0; i < graph_model.targets.size(); ++i) {
      const auto& lt = build_targets[i];
      if (lt.package_name != intra_package_target_graph_pkg)
        continue;
      if (lt.desc.type != "executable")
        continue;
      for (const auto& link : graph_model.targets[i].links) {
        if (!primary_lib_names.count(link.first))
          continue;
        if (link.first == lt.desc.name)
          continue;
        link_edges.push_back({lt.desc.name, link.first});
      }
    }
    if (!link_edges.empty()) {
      std::sort(link_edges.begin(), link_edges.end());
      link_edges.erase(std::unique(link_edges.begin(), link_edges.end()), link_edges.end());
      constexpr std::size_t k_link_edges_hard_cap = 8000;
      const std::size_t total_edges = link_edges.size();
      if (total_edges > k_link_edges_hard_cap)
        link_edges.resize(k_link_edges_hard_cap);

      f << "\n## Target link graph (configure-resolved, package \"" << intra_package_target_graph_pkg << "\")\n\n";
      f << "No same-package `<dependency name=\"...\"/>` entries were found in `target.xml`. Output is based on "
            "**resolved executable → library** link lines from `up configure`.\n\n"
            "1. **By target type**: isolated **Executables** and **Libraries** index diagrams (no edges) so previews "
            "do not mix hundreds of unrelated nodes with arrows.\n"
            "2. **By library kind**: actual `exe → lib` edges are split into **compiled libraries** (`static_library` / "
            "`shared_library`) vs **imported / installed** libraries; within each kind, Mermaid output is split only by "
            "**edge count** (≤ 70 edges per block), with no target-name heuristics.\n\n";
      if (total_edges > k_link_edges_hard_cap) {
        f << "_Only the first " << k_link_edges_hard_cap << " edges (of " << total_edges << ") participate in the "
             "split below._\n\n";
      }

      constexpr std::size_t k_edges_per_mermaid = 70;
      constexpr std::size_t k_index_names_per_block = 55;

      std::set<std::string> exe_name_set;
      std::set<std::string> lib_name_set;
      for (const auto& e : link_edges) {
        exe_name_set.insert(e.first);
        lib_name_set.insert(e.second);
      }
      std::vector<std::string> exe_names(exe_name_set.begin(), exe_name_set.end());
      std::vector<std::string> lib_names(lib_name_set.begin(), lib_name_set.end());

      f << "### By type — executable targets (" << exe_names.size() << ")\n\n";
      f << "_Nodes only; link arrows appear under the library-kind sections below._\n\n";
      emit_mermaid_target_index_diagrams(f, intra_package_target_graph_pkg, exe_names, true, k_index_names_per_block);

      f << "### By type — library targets (" << lib_names.size() << ")\n\n";
      f << "_Only libraries that appear as a link target for at least one executable in (1)._\n\n";
      emit_mermaid_target_index_diagrams(f, intra_package_target_graph_pkg, lib_names, false, k_index_names_per_block);

      std::map<std::string, std::string> target_type_by_name;
      for (const auto& lt : all_targets) {
        if (lt.package_name == intra_package_target_graph_pkg)
          target_type_by_name[lt.desc.name] = lt.desc.type;
      }
      auto is_compiled_lib_type = [](const std::string& ty) {
        return ty == "static_library" || ty == "shared_library";
      };

      std::vector<std::pair<std::string, std::string>> edges_compiled;
      std::vector<std::pair<std::string, std::string>> edges_imported;
      edges_compiled.reserve(link_edges.size());
      edges_imported.reserve(link_edges.size());
      for (const auto& e : link_edges) {
        const auto tit = target_type_by_name.find(e.second);
        std::string ty = "static_library";
        if (tit != target_type_by_name.end())
          ty = tit->second;
        if (is_compiled_lib_type(ty))
          edges_compiled.push_back(e);
        else
          edges_imported.push_back(e);
      }

      if (!edges_compiled.empty()) {
        f << "### Link lines — compiled libraries (`static_library` / `shared_library`, " << edges_compiled.size()
          << " edges)\n\n";
        emit_target_link_mermaid_diagrams(f, intra_package_target_graph_pkg, edges_compiled, k_edges_per_mermaid);
      }
      if (!edges_imported.empty()) {
        f << "### Link lines — imported / installed libraries (" << edges_imported.size() << " edges)\n\n";
        emit_target_link_mermaid_diagrams(f, intra_package_target_graph_pkg, edges_imported, k_edges_per_mermaid);
      }
    }
  }
}

std::map<std::string, std::string> load_cached_up_options(const std::filesystem::path& cache_path) {
  std::map<std::string, std::string> out;
  std::ifstream f(cache_path);
  if (!f)
    return out;
  std::string line;
  while (std::getline(f, line)) {
    const auto pos = line.find('=');
    if (pos == std::string::npos || pos == 0)
      continue;
    const std::string k = line.substr(0, pos);
    const std::string v = line.substr(pos + 1);
    if (up_mergeable_option_key(k))
      out[k] = v;
  }
  return out;
}

std::map<std::string, std::string> merge_up_options(const std::map<std::string, std::string>& base,
                                                     const std::vector<std::string>& opt_kvs) {
  std::map<std::string, std::string> out = base;
  for (const auto& kv : opt_kvs) {
    const auto pos = kv.find('=');
    if (pos == std::string::npos || pos == 0)
      continue;
    const std::string k = kv.substr(0, pos);
    const std::string v = kv.substr(pos + 1);
    if (up_mergeable_option_key(k))
      out[k] = v;
  }
  return out;
}

}  // namespace

int cmd_configure(const std::filesystem::path& cwd,
                  const std::vector<std::string>& scan_roots,
                  const std::vector<std::string>& opt_kvs,
                  const std::optional<std::string>& build_dir_name_override) {
  cli_verbose_phase("configure", "start");
  std::vector<std::filesystem::path> roots;
  if (scan_roots.empty())
    roots.push_back(cwd);
  else {
    for (const auto& s : scan_roots)
      roots.push_back(std::filesystem::path(s).lexically_normal());
    bool has_cwd = false;
    std::error_code ec;
    const auto cwd_abs = std::filesystem::weakly_canonical(std::filesystem::absolute(cwd), ec);
    for (const auto& r : roots) {
      std::error_code rc;
      const auto r_abs = std::filesystem::weakly_canonical(std::filesystem::absolute(r), rc);
      if (!ec && !rc && r_abs == cwd_abs) {
        has_cwd = true;
        break;
      }
    }
    if (!has_cwd)
      roots.push_back(cwd);
  }
  cli_verbose_phase("configure", "scan_roots");

  if (!require_ascii_path(cwd))
    return 6;
  for (const auto& r : roots) {
    if (!require_ascii_path(r))
      return 6;
  }
  cli_verbose_phase("configure", "paths_ascii_ok");

  std::vector<std::filesystem::path> package_files;
  std::vector<std::filesystem::path> target_files;
  for (const auto& r : roots) {
    collect_desc_files(r, package_files, target_files);
  }
  {
    auto dedup = [](std::vector<std::filesystem::path>& v) {
      std::set<std::string> seen;
      std::vector<std::filesystem::path> out;
      out.reserve(v.size());
      for (const auto& p : v) {
        const std::string key = std::filesystem::absolute(p).lexically_normal().generic_string();
        if (seen.insert(key).second)
          out.push_back(p);
      }
      v = std::move(out);
    };
    dedup(package_files);
    dedup(target_files);
  }
  cli_verbose_phase("configure", "collect_desc_files");

  for (const auto& p : package_files) {
    if (!require_ascii_path(p))
      return 6;
  }
  for (const auto& p : target_files) {
    if (!require_ascii_path(p))
      return 6;
  }

  if (package_files.empty() && target_files.empty()) {
    std::cerr << "configure: no package.xml or target.xml found under scan roots.\n";
    return 2;
  }
  cli_verbose_phase("configure", "load_xml");

  std::vector<std::pair<std::filesystem::path, PackageDesc>> loaded_packages;
  std::map<std::string, std::filesystem::path> package_name_to_dir;
  std::map<std::string, PackageDesc> package_name_to_desc;
  for (const auto& pkg_path : package_files) {
    PackageDesc pkg;
    std::string err;
    if (!load_package_xml(pkg_path, pkg, err)) {
      std::cerr << to_posix_path_string(pkg_path) << ": " << err << "\n";
      return 3;
    }
    if (package_name_to_dir.count(pkg.name)) {
      std::cerr << "configure: duplicate package name: " << pkg.name << "\n";
      return 3;
    }
    package_name_to_dir[pkg.name] = pkg_path.parent_path();
    package_name_to_desc[pkg.name] = pkg;
    loaded_packages.push_back({pkg_path, pkg});
  }

  std::vector<LoadedTarget> all_targets;
  std::map<std::string, size_t> target_index;  // package:target -> all_targets idx
  for (const auto& tpath : target_files) {
    const auto anchor = nearest_package_parent(tpath, package_files);
    if (anchor.empty()) {
      std::cerr << "warning: target without package parent in scan set: " << to_posix_path_string(tpath) << "\n";
      continue;
    }
    std::string pkg_name;
    for (const auto& pp : loaded_packages) {
      if (pp.first.parent_path() == anchor) {
        pkg_name = pp.second.name;
        break;
      }
    }
    if (pkg_name.empty()) {
      std::cerr << "warning: target parent package not resolved: " << to_posix_path_string(tpath) << "\n";
      continue;
    }
    TargetDesc td;
    std::string err;
    if (!load_target_xml(tpath, td, err)) {
      std::cerr << to_posix_path_string(tpath) << ": " << err << "\n";
      return 3;
    }
    LoadedTarget lt;
    lt.target_dir = tpath.parent_path();
    lt.package_name = pkg_name;
    lt.desc = std::move(td);
    const std::string key = pkg_name + ":" + lt.desc.name;
    if (target_index.count(key)) {
      std::cerr << "configure: duplicate target key: " << key << " at " << to_posix_path_string(tpath) << "\n";
      return 3;
    }
    target_index[key] = all_targets.size();
    all_targets.push_back(std::move(lt));
  }
  cli_verbose_phase("configure", "targets_bound");

  std::cout << "Package / target graph (conceptual tree):\n";
  for (const auto& pkg_pair : loaded_packages) {
    const auto& pkg_path = pkg_pair.first;
    const auto& pkg = pkg_pair.second;
    std::cout << "  package \"" << pkg.name << "\" v" << pkg.version << " @ " << to_posix_path_string(pkg_path) << "\n";
    for (const auto& d : pkg.dependencies)
      std::cout << "    (dep) " << d.first << (d.second ? " [optional]" : "") << "\n";
    for (const auto& lt : all_targets) {
      if (lt.package_name != pkg.name)
        continue;
      std::filesystem::path tpath = lt.target_dir / "target.xml";
      std::cout << "    target \"" << lt.desc.name << "\" (" << lt.desc.type << ") @ " << to_posix_path_string(tpath)
                << "\n";
    }
  }
  cli_verbose_phase("configure", "graph_summary");

  const std::string host_arch = detect_arch_tag();
  std::map<std::string, std::string> seed_opts = merge_up_options({}, opt_kvs);
  std::string seed_build_system = lower_ascii(option_or_compat(
      seed_opts, "UP_TARGET_BUILD_SYSTEM", "UP_BUILD_SYSTEM", "cmake"));
  if (!equals_ci(seed_build_system, "cmake") && !equals_ci(seed_build_system, "ninja")) {
    std::cerr << "configure: unsupported UP_TARGET_BUILD_SYSTEM=" << seed_build_system << " (expected cmake/ninja)\n";
    return 7;
  }
  const std::filesystem::path build_root =
      build_dir_name_override.has_value()
          ? (default_build_root(cwd, seed_build_system) / std::filesystem::u8path(*build_dir_name_override))
          : (default_build_root(cwd, seed_build_system) / "default");
  if (!require_ascii_path(build_root))
    return 6;

  auto opts = merge_up_options(load_cached_up_options(build_root / "up_cache.txt"), opt_kvs);
  ensure_default_build_parallel_options(opts);
  const std::string cpu = up::arch_from_target_cpu(option_or_compat(opts, "UP_TARGET_CPU_ARCH", "UP_CPU_ARCH", host_arch));
  const std::string system = lower_ascii(option_or_compat(opts, "UP_TARGET_SYSTEM", "UP_SYSTEM", detect_host_system_tag()));
  const std::string dyn = lower_ascii(option_or_compat(opts, "UP_TARGET_DYNAMIC_LIBRARY", "UP_DYNAMIC_LIBRARY", "OFF"));
  const std::string link_mode = (dyn == "on" || dyn == "1" || dyn == "true") ? "dynamic" : "static";
  const std::string dbg = lower_ascii(option_or_compat(opts, "UP_TARGET_DEBUG", "UP_DEBUG", "OFF"));
  const std::string config_mode = (dbg == "on" || dbg == "1" || dbg == "true") ? "debug" : "release";
  std::string toolchain = detect_host_toolchain_tag();
  const std::string generator = lower_ascii(option_or_compat(opts, "UP_CMAKE_GENERATOR", "", ""));
  if (generator.find("visual studio") != std::string::npos)
    toolchain = "msvc";
  else if (generator.find("clang") != std::string::npos)
    toolchain = "clang";
  else if (generator.find("mingw") != std::string::npos || generator.find("gcc") != std::string::npos)
    toolchain = "gcc";
  const std::string crt_mode = lower_ascii(option_or_compat(opts, "UP_TARGET_CRT", "UP_CRT", "dynamic_md"));
  const std::string build_system = lower_ascii(option_or_compat(
      opts, "UP_TARGET_BUILD_SYSTEM", "UP_BUILD_SYSTEM", seed_build_system));
  const std::string arch = compose_arch_tag(system, cpu, build_system, toolchain, link_mode, config_mode, crt_mode);
  std::filesystem::create_directories(build_root);
  const auto cache_path = build_root / "up_cache.txt";
  cli_verbose_phase("configure", "build_root_ready");

  PackageDesc primary_pkg;
  std::vector<LoadedTarget> pkg_targets;
  std::vector<LoadedTarget> extra_lib_targets;
  // primary target name -> (lib target name, link visibility: private|public|interface)
  std::map<std::string, std::vector<std::pair<std::string, std::string>>> exe_extra_links;
  std::filesystem::path pkg_dir;
  if (!loaded_packages.empty()) {
    bool found_primary = false;
    for (const auto& pp : loaded_packages) {
      if (pp.first.parent_path() == cwd) {
        primary_pkg = pp.second;
        pkg_dir = pp.first.parent_path();
        found_primary = true;
        break;
      }
    }
    if (!found_primary) {
      primary_pkg = loaded_packages.front().second;
      pkg_dir = loaded_packages.front().first.parent_path();
    }
    for (const auto& lt : all_targets) {
      if (lt.package_name == primary_pkg.name)
        pkg_targets.push_back(lt);
    }
  }

  if (!pkg_dir.empty() && !require_ascii_path(pkg_dir))
    return 6;
  for (const auto& lt : pkg_targets) {
    if (!require_ascii_path(lt.target_dir))
      return 6;
    if (lt.desc.type == "asset_bundle" || lt.desc.type == "imported_static_library" ||
        lt.desc.type == "imported_shared_library" || lt.desc.type == "imported_installed_static_library" ||
        lt.desc.type == "imported_installed_shared_library")
      continue;
    for (const auto& s : lt.desc.sources) {
      const auto sp = (lt.target_dir / s).lexically_normal();
      if (!require_ascii_path(sp))
        return 6;
    }
  }

#if !UP_DISABLE_PACKAGE_XML_CMAKE
  if (primary_pkg.external_cmake.has_value() && equals_ci(build_system, "ninja")) {
    std::cerr << "configure: package.xml <cmake> is not supported with UP_TARGET_BUILD_SYSTEM=ninja (use cmake).\n";
    return 7;
  }
#endif

  std::set<std::string> declared_dep_pkgs;
  for (const auto& d : primary_pkg.dependencies) {
    if (!d.first.empty())
      declared_dep_pkgs.insert(d.first);
    if (!d.second && !d.first.empty() && !package_name_to_dir.count(d.first)) {
      std::cerr << "configure: required dependency package not found in scan set: " << d.first << "\n";
      return 3;
    }
  }

  std::set<std::string> extra_target_keys;
  for (const auto& lt : pkg_targets) {
    const std::string self_key = lt.package_name + ":" + lt.desc.name;
    for (const auto& dep : lt.desc.dependencies) {
      std::string dep_pkg;
      std::string dep_tgt;
      if (!split_dep_ref(dep.name, primary_pkg.name, dep_pkg, dep_tgt)) {
        std::cerr << "configure: invalid target dependency \"" << dep.name << "\" in " << self_key << "\n";
        return 3;
      }
      if (dep_pkg != primary_pkg.name && !declared_dep_pkgs.count(dep_pkg)) {
        std::cerr << "configure: target dependency package \"" << dep_pkg
                  << "\" not declared in package.xml dependencies for package " << primary_pkg.name << "\n";
        return 3;
      }
      const std::string dep_key = dep_pkg + ":" + dep_tgt;
      const auto it = target_index.find(dep_key);
      if (it == target_index.end()) {
        std::cerr << "configure: target dependency not found: " << dep_key << " (referenced by " << self_key << ")\n";
        return 3;
      }
      const auto& dep_lt = all_targets[it->second];
      const bool dep_is_link_lib =
          (dep_lt.desc.type == "static_library" || dep_lt.desc.type == "shared_library" ||
           dep_lt.desc.type == "imported_static_library" || dep_lt.desc.type == "imported_shared_library" ||
           dep_lt.desc.type == "imported_installed_static_library" ||
           dep_lt.desc.type == "imported_installed_shared_library");
      const bool dep_is_asset_bundle = (dep_lt.desc.type == "asset_bundle");
      if (!(dep_is_link_lib || dep_is_asset_bundle)) {
        std::cerr << "configure: target dependency must reference a library or asset_bundle target: " << dep_key << "\n";
        return 3;
      }
      if (lt.desc.type == "executable" && dep.visibility == "interface") {
        std::cerr << "configure: <dependency visibility=\"interface\"> is not supported when the consumer is an "
                     "executable (does not link the library): "
                  << self_key << " -> " << dep_key << "\n";
        return 3;
      }
      if (dep_key != self_key && dep_is_link_lib)
        exe_extra_links[lt.desc.name].emplace_back(dep_lt.desc.name, dep.visibility);
      if (dep_pkg != primary_pkg.name)
        extra_target_keys.insert(dep_key);
    }
  }
  for (auto& kv : exe_extra_links) {
    std::map<std::string, std::string> vis_by_lib;
    for (const auto& pr : kv.second)
      vis_by_lib[pr.first] = pr.second;
    kv.second.clear();
    kv.second.reserve(vis_by_lib.size());
    for (const auto& pr : vis_by_lib)
      kv.second.emplace_back(pr.first, pr.second);
  }
  for (const auto& k : extra_target_keys) {
    const auto it = target_index.find(k);
    if (it != target_index.end())
      extra_lib_targets.push_back(all_targets[it->second]);
  }
  std::vector<std::filesystem::path> dep_install_prefixes;
  for (const auto& dep_pkg : declared_dep_pkgs) {
    const auto it = package_name_to_dir.find(dep_pkg);
    if (it == package_name_to_dir.end())
      continue;
    dep_install_prefixes.push_back(std::filesystem::absolute(default_install_root(it->second) / arch));
  }

  std::vector<std::string> install_exe_names;
  for (const auto& lt : pkg_targets) {
    if (lt.desc.type == "executable")
      install_exe_names.push_back(lt.desc.name);
  }
  if (primary_pkg.name.empty()) {
    std::cerr << "configure: no package resolved for current scan roots.\n";
    return 4;
  }
  if (pkg_targets.empty() && !primary_pkg.external_cmake.has_value()) {
    std::cerr << "configure: package has no target.xml under the same directory tree.\n";
    return 4;
  }

  // Prefer std::filesystem::relative: manual iterator compare breaks on Windows when one path is 8.3 and the other is long.
  const auto rel_to_cwd = [](std::filesystem::path base, std::filesystem::path p) {
    std::error_code ec;
    base = std::filesystem::weakly_canonical(std::filesystem::absolute(base), ec);
    p = std::filesystem::weakly_canonical(std::filesystem::absolute(p), ec);
    const std::filesystem::path rel = std::filesystem::relative(p, base, ec);
    if (!ec)
      return rel.lexically_normal().generic_string();
    return p.lexically_normal().generic_string();
  };

  const auto include_base_dir = [](const std::filesystem::path& target_dir, const TargetDesc::IncludeEntry& inc) {
    const auto src = (target_dir / inc.from).lexically_normal();
    if (inc.kind == "dir")
      return src;
    if (inc.kind == "file")
      return src.parent_path();
    if (inc.kind == "glob")
      return src.parent_path();
    return std::filesystem::path();
  };

  const auto install_dest = [](const std::string& to) {
    if (to.empty())
      return std::string("include");
    std::string out = to;
    for (char& c : out) {
      if (c == '\\')
        c = '/';
    }
    while (!out.empty() && out.front() == '/')
      out.erase(out.begin());
    while (!out.empty() && out.back() == '/')
      out.pop_back();
    if (out.empty())
      return std::string("include");
    return std::string("include/") + out;
  };
  const auto asset_install_dest = [](const std::string& to) {
    std::string out = to;
    for (char& c : out) {
      if (c == '\\')
        c = '/';
    }
    while (!out.empty() && out.front() == '/')
      out.erase(out.begin());
    while (!out.empty() && out.back() == '/')
      out.pop_back();
    if (out.empty())
      return std::string("assets");
    return out;
  };

  const auto wildcard_to_regex = [](const std::string& wildcard) {
    std::string re = "^";
    for (char c : wildcard) {
      switch (c) {
        case '*':
          re += ".*";
          break;
        case '?':
          re += ".";
          break;
        case '.':
          re += "\\.";
          break;
        case '\\':
          re += "\\\\";
          break;
        case '+':
        case '(':
        case ')':
        case '^':
        case '$':
        case '|':
        case '{':
        case '}':
        case '[':
        case ']':
          re.push_back('\\');
          re.push_back(c);
          break;
        default:
          re.push_back(c);
          break;
      }
    }
    re += "$";
    return re;
  };

  auto merged_vars_for_target = [&](const LoadedTarget& lt) {
    std::string pkg_ver = "0.0.0";
    std::vector<std::pair<std::string, std::string>> pkg_vars;
    const auto pd_it = package_name_to_desc.find(lt.package_name);
    if (pd_it != package_name_to_desc.end()) {
      pkg_vars = pd_it->second.vars;
      if (!pd_it->second.version.empty())
        pkg_ver = pd_it->second.version;
    }
    return merge_var_layers(make_builtin_var_map(lt.package_name, pkg_ver, lt.desc.name, build_system, config_mode), opts,
                            pkg_vars, lt.desc.vars);
  };

  // Package.xml `<config_files>`: builtins + package `<vars>` + workspace only (`UP_TARGET_NAME` is empty).
  auto merged_vars_for_package_config_files = [&](const std::string& package_name) {
    std::string pkg_ver = "0.0.0";
    std::vector<std::pair<std::string, std::string>> pkg_vars;
    const auto pd_it = package_name_to_desc.find(package_name);
    if (pd_it != package_name_to_desc.end()) {
      pkg_vars = pd_it->second.vars;
      if (!pd_it->second.version.empty())
        pkg_ver = pd_it->second.version;
    }
    static const std::vector<std::pair<std::string, std::string>> k_empty_target_vars;
    return merge_var_layers(make_builtin_var_map(package_name, pkg_ver, "", build_system, config_mode), opts, pkg_vars,
                            k_empty_target_vars);
  };

  // 1 = include entry, 0 = skip (false), -1 = invalid when= (configure error).
  auto when_tri = [](const std::string& when, const std::map<std::string, std::string>& vars, std::string& err_out) -> int {
    if (when.empty())
      return 1;
    err_out.clear();
    const bool ok = eval_when(when, vars, err_out);
    if (!err_out.empty())
      return -1;
    return ok ? 1 : 0;
  };

  const auto glob_matches = [&](const std::filesystem::path& target_dir, const std::string& wildcard_expr) {
    std::vector<std::filesystem::path> files;
    std::filesystem::path expr = (target_dir / wildcard_expr).lexically_normal();
    const std::filesystem::path parent = expr.parent_path();
    const std::string pat = expr.filename().string();
    std::error_code ec;
    if (!std::filesystem::exists(parent, ec))
      return files;
    std::regex re(wildcard_to_regex(pat));
    for (std::filesystem::directory_iterator it(parent, std::filesystem::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; ++it) {
      if (!it->is_regular_file())
        continue;
      const std::string name = it->path().filename().string();
      if (std::regex_match(name, re))
        files.push_back(it->path());
    }
    std::sort(files.begin(), files.end());
    return files;
  };

  auto is_lib = [](const TargetDesc& t) {
    return t.type == "static_library" || t.type == "shared_library" || t.type == "imported_static_library" ||
           t.type == "imported_shared_library" || t.type == "imported_installed_static_library" ||
           t.type == "imported_installed_shared_library";
  };

  if (!equals_ci(build_system, "cmake") && !equals_ci(build_system, "ninja")) {
    std::cerr << "configure: unsupported UP_TARGET_BUILD_SYSTEM=" << build_system << " (expected cmake/ninja)\n";
    return 7;
  }
  std::vector<LoadedTarget> build_targets = pkg_targets;
  for (const auto& lt : extra_lib_targets)
    build_targets.push_back(lt);
  std::vector<std::string> local_lib_names;
  for (const auto& lt : pkg_targets) {
    if (is_lib(lt.desc))
      local_lib_names.push_back(lt.desc.name);
  }
  struct InstallDirRule {
    std::string src;
    std::string dst;
    std::string preprocess_command;
    std::string postprocess_command;
    bool operator<(const InstallDirRule& other) const {
      if (dst != other.dst)
        return dst < other.dst;
      return src < other.src;
    }
  };
  struct InstallFileRule {
    std::string src;
    std::string dst;
    std::string preprocess_command;
    std::string postprocess_command;
    bool operator<(const InstallFileRule& other) const {
      if (dst != other.dst)
        return dst < other.dst;
      return src < other.src;
    }
  };
  std::set<InstallDirRule> install_dirs;
  std::set<InstallFileRule> install_files;
  std::set<InstallDirRule> asset_dirs;
  std::set<InstallFileRule> asset_files;
  std::error_code ec_skip;
  const std::filesystem::path install_staging_root =
      std::filesystem::weakly_canonical(std::filesystem::absolute(default_install_root(cwd)), ec_skip);
  for (const auto& lt : build_targets) {
    const auto header_vars = merged_vars_for_target(lt);
    for (const auto& inc_entry : lt.desc.includes) {
      std::string when_err;
      const int w = when_tri(inc_entry.when, header_vars, when_err);
      if (w < 0) {
        std::cerr << "configure: invalid when=\"" << inc_entry.when << "\" under <headers> for target \"" << lt.desc.name
                  << "\": " << when_err << "\n";
        return 5;
      }
      if (w == 0)
        continue;
      if (inc_entry.kind == "dir") {
        const auto inc = (lt.target_dir / inc_entry.from).lexically_normal();
        if (!install_staging_root.empty() && path_is_under_tree(install_staging_root, inc)) {
          std::cerr << "configure: warning: skipping headers <dir> under .intermediate/install (would nest installs): "
                    << to_posix_path_string(inc) << "\n";
          continue;
        }
        install_dirs.insert({rel_to_cwd(build_root, inc), install_dest(inc_entry.to), inc_entry.preprocess_command,
                             inc_entry.postprocess_command});
      } else if (inc_entry.kind == "file") {
        const auto f = (lt.target_dir / inc_entry.from).lexically_normal();
        if (!install_staging_root.empty() && path_is_under_tree(install_staging_root, f))
          continue;
        install_files.insert({rel_to_cwd(build_root, f), install_dest(inc_entry.to), inc_entry.preprocess_command,
                              inc_entry.postprocess_command});
      } else if (inc_entry.kind == "glob") {
        const auto files = glob_matches(lt.target_dir, inc_entry.from);
        if (files.empty()) {
          std::cerr << "configure: warning: headers glob matched no files: " << inc_entry.from
                    << " in target " << lt.desc.name << "\n";
          continue;
        }
        for (const auto& f : files) {
          if (!install_staging_root.empty() && path_is_under_tree(install_staging_root, f))
            continue;
          install_files.insert({rel_to_cwd(build_root, f), install_dest(inc_entry.to), inc_entry.preprocess_command,
                                inc_entry.postprocess_command});
        }
      }
    }
    for (const auto& ae : lt.desc.assets) {
      if (ae.kind == "dir") {
        const auto d = (lt.target_dir / ae.from).lexically_normal();
        if (!install_staging_root.empty() && path_is_under_tree(install_staging_root, d)) {
          std::cerr << "configure: warning: skipping asset <dir> under .intermediate/install: "
                    << to_posix_path_string(d) << "\n";
          continue;
        }
        asset_dirs.insert(
            {rel_to_cwd(build_root, d), asset_install_dest(ae.to), ae.preprocess_command, ae.postprocess_command});
      } else if (ae.kind == "file") {
        const auto f = (lt.target_dir / ae.from).lexically_normal();
        if (!install_staging_root.empty() && path_is_under_tree(install_staging_root, f))
          continue;
        asset_files.insert(
            {rel_to_cwd(build_root, f), asset_install_dest(ae.to), ae.preprocess_command, ae.postprocess_command});
      } else if (ae.kind == "glob") {
        const auto files = glob_matches(lt.target_dir, ae.from);
        if (files.empty()) {
          std::cerr << "configure: warning: asset glob matched no files: " << ae.from << " in target " << lt.desc.name
                    << "\n";
          continue;
        }
        for (const auto& f : files) {
          if (!install_staging_root.empty() && path_is_under_tree(install_staging_root, f))
            continue;
          asset_files.insert(
              {rel_to_cwd(build_root, f), asset_install_dest(ae.to), ae.preprocess_command, ae.postprocess_command});
        }
      }
    }
  }

  ConfigureGraphModel graph_model;
  graph_model.build_system = build_system;
  graph_model.package_name = primary_pkg.name;
  graph_model.config_mode = config_mode;
  graph_model.parallel_compile_jobs = parallel_jobs_for_build(opts);
  graph_model.build_root = build_root;
  graph_model.out_dir = build_root / "out";
  graph_model.install_root = default_install_root(cwd) / arch;
  graph_model.install_exe_names = install_exe_names;
  graph_model.cmake_prefix_path =
      merge_cmake_prefix_path_value(std::filesystem::absolute(graph_model.install_root), dep_install_prefixes, opts, cwd);

#if !UP_DISABLE_PACKAGE_XML_CMAKE
  if (primary_pkg.external_cmake.has_value()) {
    const std::string& sd = primary_pkg.external_cmake->source_dir;
    std::filesystem::path src_abs;
    if (!resolve_external_cmake_source_dir(pkg_dir, sd, "configure: <cmake> source_dir does not resolve to a directory: ",
                                           sd, src_abs)) {
      return 5;
    }
    ConfigureExternalCmake ec;
    const std::string ep_base = sanitize_ep_base_name(primary_pkg.name, "pkg");
    ec.ep_target_name = "up_ext_" + ep_base;
    ec.source_dir = src_abs;
    ec.binary_dir = std::filesystem::absolute(build_root / "external" / ep_base);
    ec.install_prefix = std::filesystem::absolute(graph_model.install_root);
    // Some upstream projects use unquoted ${CMAKE_DEBUG_POSTFIX} in SET_TARGET_PROPERTIES.
    // Keep a safe non-empty default to avoid "incorrect number of arguments" when unset.
    ec.upstream_cmake_args.push_back({"CMAKE_DEBUG_POSTFIX", "d"});
    append_dep_imported_hints(ec, declared_dep_pkgs, package_name_to_dir, all_targets, arch);
    append_upstream_opts_from_map(ec, opts);
    graph_model.external_cmake.push_back(std::move(ec));
  }
  for (const auto& dep_pkg : declared_dep_pkgs) {
    if (dep_pkg == primary_pkg.name)
      continue;
    const auto dit = package_name_to_desc.find(dep_pkg);
    const auto pit = package_name_to_dir.find(dep_pkg);
    if (dit == package_name_to_desc.end() || pit == package_name_to_dir.end())
      continue;
    if (!dit->second.external_cmake.has_value())
      continue;
    std::filesystem::path dep_src_abs;
    if (!resolve_external_cmake_source_dir(pit->second, dit->second.external_cmake->source_dir,
                                           "configure: dependency package <cmake> source_dir invalid: ", dep_pkg,
                                           dep_src_abs)) {
      return 5;
    }
    ConfigureExternalCmake ec;
    const std::string ep_base = sanitize_ep_base_name(dep_pkg, "dep");
    ec.ep_target_name = "up_ext_dep_" + ep_base;
    ec.source_dir = dep_src_abs;
    ec.binary_dir = std::filesystem::absolute(build_root / "external" / ("dep_" + ep_base));
    ec.install_prefix = std::filesystem::absolute(default_install_root(pit->second) / arch);
    // Keep the same safe default for dependency external projects.
    ec.upstream_cmake_args.push_back({"CMAKE_DEBUG_POSTFIX", "d"});
    bool dep_wants_static = false;
    bool dep_wants_shared = false;
    for (const auto& lt : all_targets) {
      if (lt.package_name != dep_pkg)
        continue;
      if (lt.desc.type == "imported_installed_static_library")
        dep_wants_static = true;
      else if (lt.desc.type == "imported_installed_shared_library")
        dep_wants_shared = true;
    }
    if (dep_wants_static)
      ec.upstream_cmake_args.push_back({"BUILD_STATIC_LIBRARY", "ON"});
    if (dep_wants_shared)
      ec.upstream_cmake_args.push_back({"BUILD_DYNAMIC_LIBRARY", "ON"});
    append_upstream_opts_from_map(ec, opts);
    graph_model.external_cmake.push_back(std::move(ec));
  }
#endif

  auto resolve_existing_file = [&](const std::filesystem::path& target_dir, const std::string& rel_or_abs,
                                   const char* what) -> std::optional<std::filesystem::path> {
    if (rel_or_abs.empty())
      return std::nullopt;
    std::filesystem::path p(rel_or_abs);
    if (p.is_absolute()) {
      std::error_code ec;
      const auto c = std::filesystem::weakly_canonical(p, ec);
      if (ec || !std::filesystem::is_regular_file(c)) {
        std::cerr << "configure: " << what << " not a file: " << rel_or_abs << "\n";
        return std::nullopt;
      }
      return c;
    }
    std::error_code ec;
    const auto c = std::filesystem::weakly_canonical(std::filesystem::absolute(target_dir / p), ec);
    if (ec || !std::filesystem::is_regular_file(c)) {
      std::cerr << "configure: " << what << " not a file (relative to target dir): " << rel_or_abs << "\n";
      return std::nullopt;
    }
    return c;
  };

  std::map<std::string, std::vector<std::string>> package_config_generated_abs;
  {
    std::set<std::string> pkg_cf_done;
    for (const auto& lt : build_targets) {
      if (!pkg_cf_done.insert(lt.package_name).second)
        continue;
      const auto pd_it = package_name_to_desc.find(lt.package_name);
      if (pd_it == package_name_to_desc.end() || pd_it->second.config_files.empty())
        continue;
      const auto dir_it = package_name_to_dir.find(lt.package_name);
      if (dir_it == package_name_to_dir.end())
        continue;
      const std::filesystem::path pkg_root = dir_it->second;
      const auto src_vars = merged_vars_for_package_config_files(lt.package_name);
      const std::filesystem::path gen_pkg_root =
          std::filesystem::absolute(cwd / ".intermediate" / "generated" / lt.package_name / "_package");
      std::vector<std::string>& out_list = package_config_generated_abs[lt.package_name];
      for (const auto& cf : pd_it->second.config_files) {
        const std::filesystem::path rel_out(cf.to);
        if (!is_safe_relative_config_output(rel_out)) {
          std::cerr << "configure: package.xml config_files to=\"" << cf.to
                    << "\" must be a relative path without '..' segments (package \"" << lt.package_name << "\")\n";
          return 5;
        }
        const auto tmpl_path = pkg_root / cf.in;
        std::ifstream tin(tmpl_path, std::ios::binary);
        if (!tin) {
          std::cerr << "configure: cannot open package config template " << to_posix_path_string(tmpl_path)
                    << " (package \"" << lt.package_name << "\")\n";
          return 5;
        }
        std::ostringstream tbuf;
        tbuf << tin.rdbuf();
        const std::string rendered = apply_cmakedefine_directives(substitute_at_vars(tbuf.str(), src_vars), src_vars);
        const auto out_path = (gen_pkg_root / rel_out).lexically_normal();
        std::error_code mk_ec;
        std::filesystem::create_directories(out_path.parent_path(), mk_ec);
        if (mk_ec) {
          std::cerr << "configure: cannot create directory for package generated config: " << mk_ec.message() << "\n";
          return 5;
        }
        std::ofstream tout(out_path, std::ios::binary | std::ios::trunc);
        if (!tout) {
          std::cerr << "configure: cannot write package generated config " << to_posix_path_string(out_path) << "\n";
          return 5;
        }
        tout << rendered;
        if (!tout) {
          std::cerr << "configure: write failed for package generated config " << to_posix_path_string(out_path) << "\n";
          return 5;
        }
        out_list.push_back(std::filesystem::absolute(out_path).generic_string());
      }
    }
  }

  for (const auto& lt : build_targets) {
    ConfigureTargetModel tm;
    tm.name = lt.desc.name;
    tm.type = lt.desc.type;
    const bool asset_only = (lt.desc.type == "asset_bundle");
    const bool imported_static = (lt.desc.type == "imported_static_library");
    const bool imported_shared = (lt.desc.type == "imported_shared_library");
    const bool imported_installed_static = (lt.desc.type == "imported_installed_static_library");
    const bool imported_installed_shared = (lt.desc.type == "imported_installed_shared_library");

    if (imported_installed_static || imported_installed_shared) {
      if (!configure_installed_import_target_model(
              lt,
#if UP_DISABLE_PACKAGE_XML_CMAKE
              false,
#else
              primary_pkg.external_cmake.has_value(),
#endif
              imported_installed_shared, tm))
        return 5;
    } else if (imported_static || imported_shared) {
      if (!lt.desc.prebuilt.has_value()) {
        std::cerr << "configure: target \"" << lt.desc.name << "\" (" << lt.desc.type << ") requires <prebuilt .../> in "
                  << to_posix_path_string(lt.target_dir / "target.xml") << "\n";
        return 5;
      }
      const TargetDesc::PrebuiltDesc& pb = *lt.desc.prebuilt;
      tm.imported_prebuilt = true;
      if (imported_static) {
        const std::string primary = !pb.import_lib.empty() ? pb.import_lib : pb.location;
        if (primary.empty()) {
          std::cerr << "configure: imported_static_library \"" << lt.desc.name << "\" needs prebuilt import_lib or location\n";
          return 5;
        }
        auto f = resolve_existing_file(lt.target_dir, primary, "imported static library");
        if (!f)
          return 5;
        tm.imported_location = to_posix_path_string(*f);
      } else {
#if defined(_WIN32)
        const std::string dll_path = !pb.dll.empty() ? pb.dll : pb.location;
        if (dll_path.empty()) {
          std::cerr << "configure: imported_shared_library \"" << lt.desc.name
                    << "\" on Windows needs prebuilt dll= or location= (.dll)\n";
          return 5;
        }
        if (pb.import_lib.empty()) {
          std::cerr << "configure: imported_shared_library \"" << lt.desc.name << "\" needs prebuilt import_lib= (.lib)\n";
          return 5;
        }
        auto df = resolve_existing_file(lt.target_dir, dll_path, "imported DLL");
        auto lf = resolve_existing_file(lt.target_dir, pb.import_lib, "import library");
        if (!df || !lf)
          return 5;
        tm.imported_location = to_posix_path_string(*df);
        tm.imported_implib = to_posix_path_string(*lf);
        tm.imported_dll = tm.imported_location;
#else
        const std::string so_path = !pb.location.empty() ? pb.location : pb.import_lib;
        if (so_path.empty()) {
          std::cerr << "configure: imported_shared_library \"" << lt.desc.name << "\" needs prebuilt location= or import_lib=\n";
          return 5;
        }
        auto sf = resolve_existing_file(lt.target_dir, so_path, "imported shared library");
        if (!sf)
          return 5;
        tm.imported_location = to_posix_path_string(*sf);
#endif
      }
    } else {
      const auto src_vars = merged_vars_for_target(lt);
      const std::filesystem::path gen_root =
          std::filesystem::absolute(cwd / ".intermediate" / "generated" / lt.package_name / lt.desc.name);
      const auto pkg_cfg_it = package_config_generated_abs.find(lt.package_name);
      if (pkg_cfg_it != package_config_generated_abs.end()) {
        for (const std::string& abs_out : pkg_cfg_it->second) {
          tm.source_paths.push_back(abs_out);
          tm.source_rules.push_back({abs_out, "", ""});
        }
      }
      for (const auto& cf : lt.desc.config_files) {
        const std::filesystem::path rel_out(cf.to);
        if (!is_safe_relative_config_output(rel_out)) {
          std::cerr << "configure: config_files to=\"" << cf.to
                    << "\" must be a relative path without '..' segments (target \"" << lt.desc.name << "\")\n";
          return 5;
        }
        const auto tmpl_path = lt.target_dir / cf.in;
        std::ifstream tin(tmpl_path, std::ios::binary);
        if (!tin) {
          std::cerr << "configure: cannot open config template " << to_posix_path_string(tmpl_path) << " (target \""
                    << lt.desc.name << "\")\n";
          return 5;
        }
        std::ostringstream tbuf;
        tbuf << tin.rdbuf();
        const std::string rendered = apply_cmakedefine_directives(substitute_at_vars(tbuf.str(), src_vars), src_vars);
        const auto out_path = (gen_root / rel_out).lexically_normal();
        std::error_code mk_ec;
        std::filesystem::create_directories(out_path.parent_path(), mk_ec);
        if (mk_ec) {
          std::cerr << "configure: cannot create directory for generated config: " << mk_ec.message() << "\n";
          return 5;
        }
        std::ofstream tout(out_path, std::ios::binary | std::ios::trunc);
        if (!tout) {
          std::cerr << "configure: cannot write generated config " << to_posix_path_string(out_path) << "\n";
          return 5;
        }
        tout << rendered;
        if (!tout) {
          std::cerr << "configure: write failed for generated config " << to_posix_path_string(out_path) << "\n";
          return 5;
        }
        const std::string abs_out = std::filesystem::absolute(out_path).generic_string();
        tm.source_paths.push_back(abs_out);
        tm.source_rules.push_back({abs_out, "", ""});
      }
      for (const auto& s : lt.desc.source_entries) {
        std::string when_err;
        const int w = when_tri(s.when, src_vars, when_err);
        if (w < 0) {
          std::cerr << "configure: invalid when=\"" << s.when << "\" in <sources> for target \"" << lt.desc.name
                    << "\": " << when_err << "\n";
          return 5;
        }
        if (w == 0)
          continue;
        if (s.kind == "glob") {
          const auto files = glob_matches(lt.target_dir, s.from);
          for (const auto& sf : files) {
            tm.source_paths.push_back(std::filesystem::absolute(sf).generic_string());
            tm.source_rules.push_back(
                {std::filesystem::absolute(sf).generic_string(), s.preprocess_command, s.postprocess_command});
          }
        } else {
          const auto src = (lt.target_dir / s.from).lexically_normal();
          tm.source_paths.push_back(std::filesystem::absolute(src).generic_string());
          tm.source_rules.push_back(
              {std::filesystem::absolute(src).generic_string(), s.preprocess_command, s.postprocess_command});
        }
      }
      if (tm.source_paths.empty() && !lt.desc.sources.empty()) {
        for (const auto& rel : lt.desc.sources) {
          const auto src = (lt.target_dir / rel).lexically_normal();
          tm.source_paths.push_back(std::filesystem::absolute(src).generic_string());
          tm.source_rules.push_back({std::filesystem::absolute(src).generic_string(), "", ""});
        }
      }
      if (tm.source_paths.empty() && !asset_only) {
        std::cerr << "configure: target \"" << lt.desc.name
                  << "\" has no resolved source files (check <sources> / globs in "
                  << to_posix_path_string(lt.target_dir / "target.xml") << ")\n";
        return 5;
      }
      if (asset_only && tm.source_paths.empty() && lt.desc.assets.empty() && lt.desc.includes.empty()) {
        std::cerr << "configure: asset_bundle \"" << lt.desc.name << "\" has no sources, assets, or <headers>\n";
        return 5;
      }
    }

    std::set<std::string> inc_dirs;
    const auto inc_vars = merged_vars_for_target(lt);
    for (const auto& inc_entry : lt.desc.includes) {
      std::string when_err;
      const int w = when_tri(inc_entry.when, inc_vars, when_err);
      if (w < 0) {
        std::cerr << "configure: invalid when=\"" << inc_entry.when << "\" under <headers> for target \"" << lt.desc.name
                  << "\": " << when_err << "\n";
        return 5;
      }
      if (w == 0)
        continue;
      const auto inc = include_base_dir(lt.target_dir, inc_entry);
      if (!inc.empty())
        inc_dirs.insert(std::filesystem::absolute(inc).generic_string());
    }
    const auto pd_cf_it = package_name_to_desc.find(lt.package_name);
    const bool pkg_has_config_files =
        pd_cf_it != package_name_to_desc.end() && !pd_cf_it->second.config_files.empty();
    if ((lt.desc.type == "executable" || lt.desc.type == "static_library" || lt.desc.type == "shared_library")) {
      if (!lt.desc.config_files.empty()) {
        const std::filesystem::path gen_inc =
            std::filesystem::absolute(cwd / ".intermediate" / "generated" / lt.package_name / lt.desc.name);
        inc_dirs.insert(gen_inc.generic_string());
      }
      if (pkg_has_config_files) {
        const std::filesystem::path gen_pkg_inc =
            std::filesystem::absolute(cwd / ".intermediate" / "generated" / lt.package_name / "_package");
        inc_dirs.insert(gen_pkg_inc.generic_string());
      }
    }
    tm.include_dirs.assign(inc_dirs.begin(), inc_dirs.end());
    if (!tm.imported_prebuilt && (tm.type == "executable" || tm.type == "static_library" || tm.type == "shared_library")) {
      const auto pd_it = package_name_to_desc.find(lt.package_name);
      if (pd_it != package_name_to_desc.end()) {
        for (const auto& de : pd_it->second.defines) {
          if (de.name.empty())
            continue;
          tm.compile_definitions.push_back(de.value.empty() ? de.name : (de.name + "=" + de.value));
        }
      }
      for (const auto& de : lt.desc.defines) {
        if (de.name.empty())
          continue;
        tm.compile_definitions.push_back(de.value.empty() ? de.name : (de.name + "=" + de.value));
      }
    }
    if (lt.desc.type == "executable") {
      auto eit = exe_extra_links.find(lt.desc.name);
      if (eit != exe_extra_links.end() && !eit->second.empty()) {
        tm.links = eit->second;
      } else {
        // Avoid linking every library to every executable (can break when multiple library variants coexist).
        // When UP_TARGET_DYNAMIC_LIBRARY implies static link preference, only link static_library targets; vice versa.
        tm.links.clear();
        for (const auto& pl : pkg_targets) {
          if (!is_lib(pl.desc))
            continue;
          if (link_mode == "static" &&
              (pl.desc.type == "static_library" || pl.desc.type == "imported_static_library" ||
               pl.desc.type == "imported_installed_static_library"))
            tm.links.emplace_back(pl.desc.name, "private");
          else if (link_mode == "dynamic" &&
                   (pl.desc.type == "shared_library" || pl.desc.type == "imported_shared_library" ||
                    pl.desc.type == "imported_installed_shared_library"))
            tm.links.emplace_back(pl.desc.name, "private");
        }
        if (tm.links.empty()) {
          for (const auto& pl : pkg_targets) {
            if (is_lib(pl.desc))
              tm.links.emplace_back(pl.desc.name, "private");
          }
        }
      }
      std::sort(tm.links.begin(), tm.links.end(), [](const std::pair<std::string, std::string>& a,
                                                    const std::pair<std::string, std::string>& b) {
        if (a.first != b.first)
          return a.first < b.first;
        return a.second < b.second;
      });
      tm.links.erase(std::unique(tm.links.begin(), tm.links.end(),
                                 [](const std::pair<std::string, std::string>& a,
                                    const std::pair<std::string, std::string>& b) {
                                   return a.first == b.first && a.second == b.second;
                                 }),
                     tm.links.end());
    }
    graph_model.targets.push_back(std::move(tm));
  }
  for (const auto& rule : install_dirs)
    graph_model.install_dir_rules.push_back({rule.src, rule.dst, rule.preprocess_command, rule.postprocess_command});
  for (const auto& rule : install_files)
    graph_model.install_file_rules.push_back({rule.src, rule.dst, rule.preprocess_command, rule.postprocess_command});
  for (const auto& rule : asset_dirs)
    graph_model.asset_dir_rules.push_back({rule.src, rule.dst, rule.preprocess_command, rule.postprocess_command});
  for (const auto& rule : asset_files)
    graph_model.asset_file_rules.push_back({rule.src, rule.dst, rule.preprocess_command, rule.postprocess_command});

  {
    const std::string cmake_generator = option_or_compat(opts, "UP_CMAKE_GENERATOR", "", "");
    const std::string gen_lc = lower_ascii(cmake_generator);
    if (gen_lc.find("visual studio") != std::string::npos || gen_lc.find("multi-config") != std::string::npos)
      graph_model.cmake_parent_multi_config = true;
#if defined(_WIN32)
    if (cmake_generator.empty())
      graph_model.cmake_parent_multi_config = true;
#endif
  }

  cli_verbose_phase("configure", "generate_backend");
  const int gen_code = run_generate_backend(graph_model);
  if (gen_code != 0)
    return gen_code;

  const auto generated_file = equals_ci(build_system, "ninja") ? (graph_model.out_dir / "build.ninja")
                                                                : (graph_model.build_root / "CMakeLists.txt");
  const auto roots_cached = scan_roots_for_cache_file(cwd, roots);
  std::map<std::string, std::string> cache_opts = opts;
  if (!graph_model.cmake_prefix_path.empty())
    cache_opts["UP_CMAKE_PREFIX_PATH"] = graph_model.cmake_prefix_path;
  cli_verbose_phase("configure", "write_cache");
  write_up_cache(cache_path, cwd, arch, primary_pkg.name, generated_file, roots_cached, cache_opts);
  write_packages_md(cache_path.parent_path() / "packages.md", loaded_packages, roots_cached, all_targets,
                    primary_pkg.name, graph_model, build_targets, cache_opts);
  if (equals_ci(build_system, "ninja")) {
    cli_verbose_phase("configure", "done_ninja");
    return 0;
  }

  // In cmake mode, configure backend files immediately (e.g. .sln on Windows).
  const auto out_dir = graph_model.out_dir;
  std::filesystem::create_directories(out_dir);
  const std::string cmake_generator = option_or_compat(opts, "UP_CMAKE_GENERATOR", "", "");
  const std::string dbg_cfg = lower_ascii(option_or_compat(opts, "UP_TARGET_DEBUG", "UP_DEBUG", "OFF"));
  const std::string config_name = (dbg_cfg == "on" || dbg_cfg == "1" || dbg_cfg == "true") ? "Debug" : "Release";

  bool multi_config = false;
  const std::string gen_lc = lower_ascii(cmake_generator);
  if (gen_lc.find("visual studio") != std::string::npos || gen_lc.find("multi-config") != std::string::npos)
    multi_config = true;
#if defined(_WIN32)
  if (cmake_generator.empty())
    multi_config = true;
#endif
  const ConfigureBackendContext backend_ctx{
      build_root, out_dir, cmake_generator, config_name, multi_config};
  cli_verbose_phase("configure", "cmake_configure_backend");
  return run_configure_backend(backend_ctx);
}

}  // namespace up
