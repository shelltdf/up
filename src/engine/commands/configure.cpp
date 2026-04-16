#include "configure.hpp"

#include "core/backend_dispatch.hpp"
#include "lang.hpp"
#include "path_check.hpp"
#include "paths.hpp"
#include "simple_xml.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <vector>

namespace up {

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
  std::filesystem::path best;
  for (const auto& pkg : package_files) {
    const auto parent = pkg.parent_path();
    if (t.string().rfind(parent.string(), 0) == 0) {
      if (best.empty() || parent.string().size() > best.string().size())
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
    std::cerr << lang::configure_path_non_ascii() << p << "\n";
    return false;
  }
  return true;
}

std::string arch_from_target_cpu(std::string v) {
  return normalize_cpu_arch_tag(v);
}

std::string option_or_compat(const std::map<std::string, std::string>& opts,
                             const std::string& preferred,
                             const std::string& legacy,
                             const std::string& defv) {
  const auto it = opts.find(preferred);
  if (it != opts.end())
    return it->second;
  const auto it2 = opts.find(legacy);
  if (it2 != opts.end())
    return it2->second;
  return defv;
}

std::string lower_ascii(std::string v) {
  for (char& c : v) {
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c - 'A' + 'a');
  }
  return v;
}

bool equals_ci(std::string a, std::string b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i] >= 'A' && a[i] <= 'Z')
      a[i] = static_cast<char>(a[i] - 'A' + 'a');
    if (b[i] >= 'A' && b[i] <= 'Z')
      b[i] = static_cast<char>(b[i] - 'A' + 'a');
  }
  return a == b;
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
    std::cerr << "configure: warning: could not write " << cache_path << "\n";
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
    if (k.rfind("UP_", 0) == 0)
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
    if (k.rfind("UP_", 0) == 0)
      out[k] = v;
  }
  return out;
}

}  // namespace

int cmd_configure(const std::filesystem::path& cwd,
                  const std::vector<std::string>& scan_roots,
                  const std::vector<std::string>& opt_kvs) {
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

  if (!require_ascii_path(cwd))
    return 6;
  for (const auto& r : roots) {
    if (!require_ascii_path(r))
      return 6;
  }

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

  std::vector<std::pair<std::filesystem::path, PackageDesc>> loaded_packages;
  std::map<std::string, std::filesystem::path> package_name_to_dir;
  for (const auto& pkg_path : package_files) {
    PackageDesc pkg;
    std::string err;
    if (!load_package_xml(pkg_path, pkg, err)) {
      std::cerr << pkg_path << ": " << err << "\n";
      return 3;
    }
    if (package_name_to_dir.count(pkg.name)) {
      std::cerr << "configure: duplicate package name: " << pkg.name << "\n";
      return 3;
    }
    package_name_to_dir[pkg.name] = pkg_path.parent_path();
    loaded_packages.push_back({pkg_path, pkg});
  }

  std::vector<LoadedTarget> all_targets;
  std::map<std::string, size_t> target_index;  // package:target -> all_targets idx
  for (const auto& tpath : target_files) {
    const auto anchor = nearest_package_parent(tpath, package_files);
    if (anchor.empty()) {
      std::cerr << "warning: target without package parent in scan set: " << tpath << "\n";
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
      std::cerr << "warning: target parent package not resolved: " << tpath << "\n";
      continue;
    }
    TargetDesc td;
    std::string err;
    if (!load_target_xml(tpath, td, err)) {
      std::cerr << tpath << ": " << err << "\n";
      return 3;
    }
    LoadedTarget lt;
    lt.target_dir = tpath.parent_path();
    lt.package_name = pkg_name;
    lt.desc = std::move(td);
    const std::string key = pkg_name + ":" + lt.desc.name;
    if (target_index.count(key)) {
      std::cerr << "configure: duplicate target key: " << key << " at " << tpath << "\n";
      return 3;
    }
    target_index[key] = all_targets.size();
    all_targets.push_back(std::move(lt));
  }

  std::cout << "Package / target graph (conceptual tree):\n";
  for (const auto& pkg_pair : loaded_packages) {
    const auto& pkg_path = pkg_pair.first;
    const auto& pkg = pkg_pair.second;
    std::cout << "  package \"" << pkg.name << "\" v" << pkg.version << " @ " << pkg_path << "\n";
    for (const auto& d : pkg.dependencies)
      std::cout << "    (dep) " << d.first << (d.second ? " [optional]" : "") << "\n";
    for (const auto& lt : all_targets) {
      if (lt.package_name != pkg.name)
        continue;
      std::filesystem::path tpath = lt.target_dir / "target.xml";
      std::cout << "    target \"" << lt.desc.name << "\" (" << lt.desc.type << ") @ " << tpath << "\n";
    }
  }

  const std::string host_arch = detect_arch_tag();
  std::map<std::string, std::string> seed_opts = merge_up_options({}, opt_kvs);
  std::string seed_build_system = lower_ascii(option_or_compat(
      seed_opts, "UP_TARGET_BUILD_SYSTEM", "UP_BUILD_SYSTEM", "cmake"));
  if (!equals_ci(seed_build_system, "cmake") && !equals_ci(seed_build_system, "ninja")) {
    std::cerr << "configure: unsupported UP_TARGET_BUILD_SYSTEM=" << seed_build_system << " (expected cmake/ninja)\n";
    return 7;
  }
  auto opts = merge_up_options(
      load_cached_up_options(default_build_root(cwd, seed_build_system) / host_arch / "up_cache.txt"),
      opt_kvs);
  const std::string cpu = arch_from_target_cpu(option_or_compat(opts, "UP_TARGET_CPU_ARCH", "UP_CPU_ARCH", host_arch));
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
  const auto build_root = default_build_root(cwd, build_system) / arch;
  if (!require_ascii_path(build_root))
    return 6;
  std::filesystem::create_directories(build_root);
  const auto cache_path = build_root / "up_cache.txt";
  if (arch != host_arch) {
    opts = merge_up_options(load_cached_up_options(cache_path), opt_kvs);
  }

  PackageDesc primary_pkg;
  std::vector<LoadedTarget> pkg_targets;
  std::vector<LoadedTarget> extra_lib_targets;
  std::map<std::string, std::vector<std::string>> exe_extra_links;  // primary target name -> lib target names
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
    for (const auto& s : lt.desc.sources) {
      const auto sp = (lt.target_dir / s).lexically_normal();
      if (!require_ascii_path(sp))
        return 6;
    }
  }

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
    for (const auto& dep_ref : lt.desc.dependencies) {
      std::string dep_pkg;
      std::string dep_tgt;
      if (!split_dep_ref(dep_ref, primary_pkg.name, dep_pkg, dep_tgt)) {
        std::cerr << "configure: invalid target dependency \"" << dep_ref << "\" in " << self_key << "\n";
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
      const bool dep_is_link_lib = (dep_lt.desc.type == "static_library" || dep_lt.desc.type == "shared_library");
      const bool dep_is_asset_bundle = (dep_lt.desc.type == "asset_bundle");
      if (!(dep_is_link_lib || dep_is_asset_bundle)) {
        std::cerr << "configure: target dependency must reference a library or asset_bundle target: " << dep_key << "\n";
        return 3;
      }
      if (dep_key != self_key && dep_is_link_lib)
        exe_extra_links[lt.desc.name].push_back(dep_lt.desc.name);
      if (dep_pkg != primary_pkg.name)
        extra_target_keys.insert(dep_key);
    }
  }
  for (const auto& k : extra_target_keys) {
    const auto it = target_index.find(k);
    if (it != target_index.end())
      extra_lib_targets.push_back(all_targets[it->second]);
  }

  std::vector<std::string> install_exe_names;
  for (const auto& lt : pkg_targets) {
    if (lt.desc.type == "executable")
      install_exe_names.push_back(lt.desc.name);
  }
  if (primary_pkg.name.empty() || install_exe_names.empty()) {
    std::cerr << "configure: need at least one package and one executable target under the same directory tree.\n";
    return 4;
  }

  const auto rel_to_cwd = [](std::filesystem::path base, std::filesystem::path p) {
    p = std::filesystem::weakly_canonical(std::filesystem::absolute(p));
    base = std::filesystem::weakly_canonical(std::filesystem::absolute(base));
    std::filesystem::path out;
    auto b = base.begin();
    auto i = p.begin();
    for (; b != base.end() && i != p.end() && *b == *i; ++b, ++i) {
    }
    for (; b != base.end(); ++b)
      out /= "..";
    for (; i != p.end(); ++i)
      out /= *i;
    return out.lexically_normal().generic_string();
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
    return t.type == "static_library" || t.type == "shared_library";
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
  for (const auto& lt : build_targets) {
    for (const auto& inc_entry : lt.desc.includes) {
      if (inc_entry.kind == "dir") {
        const auto inc = (lt.target_dir / inc_entry.from).lexically_normal();
        install_dirs.insert({rel_to_cwd(build_root, inc), install_dest(inc_entry.to), inc_entry.preprocess_command,
                             inc_entry.postprocess_command});
      } else if (inc_entry.kind == "file") {
        const auto f = (lt.target_dir / inc_entry.from).lexically_normal();
        install_files.insert({rel_to_cwd(build_root, f), install_dest(inc_entry.to), inc_entry.preprocess_command,
                              inc_entry.postprocess_command});
      } else if (inc_entry.kind == "glob") {
        const auto files = glob_matches(lt.target_dir, inc_entry.from);
        if (files.empty()) {
          std::cerr << "configure: warning: include glob matched no files: " << inc_entry.from
                    << " in target " << lt.desc.name << "\n";
          continue;
        }
        for (const auto& f : files)
          install_files.insert({rel_to_cwd(build_root, f), install_dest(inc_entry.to), inc_entry.preprocess_command,
                                inc_entry.postprocess_command});
      }
    }
    for (const auto& ae : lt.desc.assets) {
      if (ae.kind == "dir") {
        const auto d = (lt.target_dir / ae.from).lexically_normal();
        asset_dirs.insert(
            {rel_to_cwd(build_root, d), asset_install_dest(ae.to), ae.preprocess_command, ae.postprocess_command});
      } else if (ae.kind == "file") {
        const auto f = (lt.target_dir / ae.from).lexically_normal();
        asset_files.insert(
            {rel_to_cwd(build_root, f), asset_install_dest(ae.to), ae.preprocess_command, ae.postprocess_command});
      } else if (ae.kind == "glob") {
        const auto files = glob_matches(lt.target_dir, ae.from);
        if (files.empty()) {
          std::cerr << "configure: warning: asset glob matched no files: " << ae.from << " in target " << lt.desc.name
                    << "\n";
          continue;
        }
        for (const auto& f : files)
          asset_files.insert(
              {rel_to_cwd(build_root, f), asset_install_dest(ae.to), ae.preprocess_command, ae.postprocess_command});
      }
    }
  }

  ConfigureGraphModel graph_model;
  graph_model.build_system = build_system;
  graph_model.package_name = primary_pkg.name;
  graph_model.config_mode = config_mode;
  graph_model.build_root = build_root;
  graph_model.out_dir = build_root / "out";
  graph_model.install_root = default_install_root(cwd) / arch;
  graph_model.install_exe_names = install_exe_names;
  for (const auto& lt : build_targets) {
    ConfigureTargetModel tm;
    tm.name = lt.desc.name;
    tm.type = lt.desc.type;
    for (const auto& s : lt.desc.source_entries) {
      if (s.kind == "glob") {
        const auto files = glob_matches(lt.target_dir, s.from);
        for (const auto& sf : files) {
          tm.source_paths.push_back(std::filesystem::absolute(sf).generic_string());
          tm.source_rules.push_back({std::filesystem::absolute(sf).generic_string(), s.preprocess_command, s.postprocess_command});
        }
      } else {
        const auto src = (lt.target_dir / s.from).lexically_normal();
        tm.source_paths.push_back(std::filesystem::absolute(src).generic_string());
        tm.source_rules.push_back({std::filesystem::absolute(src).generic_string(), s.preprocess_command, s.postprocess_command});
      }
    }
    std::set<std::string> inc_dirs;
    for (const auto& inc_entry : lt.desc.includes) {
      const auto inc = include_base_dir(lt.target_dir, inc_entry);
      if (!inc.empty())
        inc_dirs.insert(std::filesystem::absolute(inc).generic_string());
    }
    tm.include_dirs.assign(inc_dirs.begin(), inc_dirs.end());
    if (lt.desc.type == "executable") {
      tm.links = local_lib_names;
      auto eit = exe_extra_links.find(lt.desc.name);
      if (eit != exe_extra_links.end()) {
        for (const auto& n : eit->second)
          tm.links.push_back(n);
      }
      std::sort(tm.links.begin(), tm.links.end());
      tm.links.erase(std::unique(tm.links.begin(), tm.links.end()), tm.links.end());
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

  const int gen_code = run_generate_backend(graph_model);
  if (gen_code != 0)
    return gen_code;

  const auto generated_file = equals_ci(build_system, "ninja") ? (graph_model.out_dir / "build.ninja")
                                                                : (graph_model.build_root / "CMakeLists.txt");
  write_up_cache(cache_path, cwd, arch, primary_pkg.name, generated_file, roots, opts);
  if (equals_ci(build_system, "ninja"))
    return 0;

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
  return run_configure_backend(backend_ctx);
}

}  // namespace up
