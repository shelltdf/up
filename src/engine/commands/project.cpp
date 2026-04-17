#include "project.hpp"

#include "cli_verbose.hpp"
#include "project_import.hpp"
#include "project_import_internal.hpp"
#include "lang.hpp"
#include "path_check.hpp"
#include "paths.hpp"
#include "simple_xml.hpp"

#include <algorithm>
#include <map>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <set>
#include <string>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace up {

enum class CmakeTargetsProvenance { None, InstallScan, FileApi, SourceScan };

namespace {

#if defined(_WIN32)
std::filesystem::path win32_get_current_directory_or_empty() {
  wchar_t buf[32768];
  constexpr DWORD kBufChars = static_cast<DWORD>(sizeof(buf) / sizeof(buf[0]));
  const DWORD n = GetCurrentDirectoryW(kBufChars, buf);
  if (n == 0 || n >= kBufChars)
    return {};
  buf[n] = L'\0';
  return std::filesystem::path(buf);
}
#endif

std::filesystem::path default_project_base_from_os(const std::filesystem::path& fallback) {
#if defined(_WIN32)
  const auto w = win32_get_current_directory_or_empty();
  if (!w.empty())
    return w;
#endif
  std::error_code ec;
  const auto p = std::filesystem::current_path(ec);
  return ec ? fallback : p;
}

bool filesystem_status_error_is_missing(const std::error_code& ec) {
  if (!ec)
    return false;
  if (ec == std::errc::no_such_file_or_directory)
    return true;
#if defined(_WIN32)
  if (ec.category() == std::system_category()) {
    switch (ec.value()) {
      case 2:   // ERROR_FILE_NOT_FOUND
      case 3:   // ERROR_PATH_NOT_FOUND
      case 53:  // ERROR_BAD_NETPATH (e.g. unavailable mapped drive)
        return true;
      default:
        break;
    }
  }
#endif
  return false;
}

std::string cmake_source_dir_for_package_xml(const std::filesystem::path& scan_root,
                                             const std::filesystem::path& write_root,
                                             std::string& rel_warn) {
  rel_warn.clear();
  std::error_code ec1, ec2;
  const auto s = std::filesystem::weakly_canonical(std::filesystem::absolute(scan_root), ec1);
  const auto w = std::filesystem::weakly_canonical(std::filesystem::absolute(write_root), ec2);
  if (ec1 || ec2) {
    rel_warn = "project: could not canonicalize paths; using <cmake source_dir=\".\"/>.";
    return ".";
  }
  std::error_code ec3;
  const auto rel = std::filesystem::relative(s, w, ec3);
  if (!ec3) {
    const auto n = rel.lexically_normal();
    if (!n.empty() && n != std::filesystem::path("."))
      return to_posix_path_string(n);
    return ".";
  }
  rel_warn = "project: could not compute relative path to CMake project; using <cmake source_dir=\".\"/>.";
  return ".";
}

const char* probe_label(BuildProbeKind k) {
  switch (k) {
    case BuildProbeKind::CMake:
      return "CMake (CMakeLists.txt)";
    case BuildProbeKind::Autotools:
      return "Autotools (Makefile.am)";
    case BuildProbeKind::QMake:
      return "QMake (.pro)";
    case BuildProbeKind::Meson:
      return "Meson (meson.build)";
    case BuildProbeKind::SourceTreeFallback:
      return "source tree (heuristic)";
  }
  return "unknown";
}

std::optional<std::string> parse_cmake_project_name(const std::filesystem::path& cmake_lists_path) {
  std::ifstream in(cmake_lists_path, std::ios::binary);
  if (!in)
    return std::nullopt;
  std::ostringstream ss;
  ss << in.rdbuf();
  const std::string raw = ss.str();
  std::string lower = raw;
  for (char& c : lower) {
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c - 'A' + 'a');
  }
  const std::string needle = "project";
  const size_t p = lower.find(needle);
  if (p == std::string::npos)
    return std::nullopt;
  size_t i = p + needle.size();
  while (i < raw.size() && (raw[i] == ' ' || raw[i] == '\t' || raw[i] == '\r' || raw[i] == '\n'))
    ++i;
  if (i >= raw.size() || raw[i] != '(')
    return std::nullopt;
  ++i;
  while (i < raw.size() && (raw[i] == ' ' || raw[i] == '\t' || raw[i] == '\r' || raw[i] == '\n'))
    ++i;
  if (i >= raw.size())
    return std::nullopt;

  size_t end = i;
  if (raw[i] == '"' || raw[i] == '\'') {
    const char q = raw[i];
    ++i;
    end = raw.find(q, i);
    if (end == std::string::npos)
      return std::nullopt;
  } else {
    while (end < raw.size()) {
      const char ch = raw[end];
      if (ch == ')' || ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
        break;
      ++end;
    }
    if (end <= i)
      return std::nullopt;
  }

  std::string name = raw[i] == '"' || raw[i] == '\'' ? raw.substr(i + 1, end - (i + 1)) : raw.substr(i, end - i);
  if (name.size() >= 2 && ((name.front() == '"' && name.back() == '"') || (name.front() == '\'' && name.back() == '\'')))
    name = name.substr(1, name.size() - 2);
  if (name.empty())
    return std::nullopt;
  return name;
}

}  // namespace

namespace {

void merge_cmake_source_overlay_into(ImportedPackage& base, ImportedPackage&& overlay) {
  const auto is_installed_type = [](const std::string& ty) {
    return ty == "imported_installed_static_library" || ty == "imported_installed_shared_library";
  };
  const auto is_native_build = [](const TargetDesc& td) {
    return td.type == "executable" || td.type == "static_library" || td.type == "shared_library";
  };
  std::map<std::string, size_t> index_by_name;
  for (size_t i = 0; i < base.targets.size(); ++i)
    index_by_name[base.targets[i].second.name] = i;

  for (auto& ovr : overlay.targets) {
    const std::string& n = ovr.second.name;
    const auto it = index_by_name.find(n);
    if (it == index_by_name.end()) {
      index_by_name[n] = base.targets.size();
      base.targets.push_back(std::move(ovr));
      continue;
    }
    auto& slot = base.targets[it->second];
    TargetDesc& b = slot.second;
    TargetDesc& o = ovr.second;
    if (is_installed_type(b.type) && is_native_build(o) && !o.sources.empty()) {
      slot.first = std::move(ovr.first);
      b = std::move(o);
    } else if (is_native_build(b) && is_native_build(o)) {
      for (const auto& d : o.dependencies) {
        if (std::find(b.dependencies.begin(), b.dependencies.end(), d) == b.dependencies.end())
          b.dependencies.push_back(d);
      }
      for (const auto& inc : o.includes) {
        bool dup = false;
        for (const auto& bi : b.includes) {
          if (bi.kind == inc.kind && bi.from == inc.from) {
            dup = true;
            break;
          }
        }
        if (!dup)
          b.includes.push_back(inc);
      }
    }
  }
}

}  // namespace

int cmd_project(const std::filesystem::path& cwd, const std::vector<std::string>& args) {
  auto finish = [](int code) {
    std::cout << "project: finished with " << (code == 0 ? "success" : "failure") << " (code " << code << ")\n";
    return code;
  };
  bool dry_run = false;
  bool force = false;
  bool legacy_cmake_parse = false;
  bool cmake_query = false;
  bool cmake_no_file_api = false;
  bool cmake_query_keep_build = false;
  std::optional<std::filesystem::path> output_dir;
  std::optional<std::string> project_dir_arg;
  std::optional<std::string> package_name_opt;
  std::optional<std::string> cmake_query_build_dir_arg;

  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& a = args[i];
    if (a == "--dry-run")
      dry_run = true;
    else if (a == "--force")
      force = true;
    else if (a == "--legacy-cmake-parse")
      legacy_cmake_parse = true;
    else if (a == "--cmake-query")
      cmake_query = true;
    else if (a == "--cmake-no-file-api")
      cmake_no_file_api = true;
    else if (a == "--cmake-query-keep-build")
      cmake_query_keep_build = true;
    else if (a == "--cmake-query-build-dir" && i + 1 < args.size())
      cmake_query_build_dir_arg = args[++i];
    else if ((a == "--project-dir" || a == "-C") && i + 1 < args.size())
      project_dir_arg = args[++i];
    else if (a == "--output-dir" && i + 1 < args.size())
      output_dir = args[++i];
    else if (a == "--package-name" && i + 1 < args.size())
      package_name_opt = args[++i];
    else {
      std::cerr << "project: unknown option: " << a << "\n"
                << "  --dry-run       print probe result and XML; do not write files\n"
                << "  --force         overwrite existing package.xml / target.xml\n"
                << "  --legacy-cmake-parse  for CMake projects: skip <cmake/> scaffold; old import_from_probe (CMake text scan).\n"
                << "  --cmake-query   CMake only: use File API only (fail if cmake/configure or codemodel yields no libs).\n"
                << "                         Default already tries install scan, then File API, then source scan.\n"
                << "  --cmake-no-file-api  CMake scaffold: skip the File API step (no `cmake` configure subprocess).\n"
                << "                         Less accurate than codemodel (no real configure); install scan + source scan only.\n"
                << "  --cmake-query-build-dir <path>  scratch dir for File API (default: <write_root>/.up/cmake_file_api_query)\n"
                << "  --cmake-query-keep-build        keep the File API build directory after use (default: delete)\n"
                << "  --project-dir|-C <path>  probe/write under this dir (override default cwd from the OS)\n"
                << "  --output-dir <path>  package root for generated files (default: same as scan root). target.xml is placed next\n"
                << "                         to each target's CMake/.pro tree when possible, else under .targets/<name>/.\n"
                << "  --package-name <name>  override <package name=\"...\">\n";
      return finish(2);
    }
  }
  if (legacy_cmake_parse && cmake_query) {
    std::cerr << "project: --legacy-cmake-parse cannot be combined with --cmake-query.\n";
    return finish(2);
  }
  if (cmake_query && cmake_no_file_api) {
    std::cerr << "project: --cmake-query cannot be combined with --cmake-no-file-api.\n";
    return finish(2);
  }
  cli_verbose_phase("project", "options_parsed");

  std::filesystem::path base;
  if (project_dir_arg.has_value()) {
    std::error_code ecp;
    base = std::filesystem::absolute(std::filesystem::path(*project_dir_arg), ecp);
    if (ecp || base.empty()) {
      std::cerr << "project: invalid --project-dir / -C path\n";
      return finish(2);
    }
  } else {
    // Prefer Win32 GetCurrentDirectoryW on Windows: some launchers leave std::filesystem::current_path()
    // at a repo root while the shell cwd (what the user cd'd into) matches Win32.
    base = default_project_base_from_os(cwd);
  }

  std::error_code ec;
  const std::filesystem::path scan_root = std::filesystem::absolute(base, ec);
  if (ec) {
    std::cerr << "project: could not resolve project directory path\n";
    return finish(2);
  }
  std::filesystem::path write_root = output_dir.value_or(base);
  write_root = std::filesystem::absolute(write_root, ec);
  if (ec) {
    std::cerr << "project: invalid --output-dir\n";
    return finish(2);
  }
  cli_verbose_phase("project", "paths_resolved");

  if (path_has_non_ascii(scan_root) || path_has_non_ascii(write_root)) {
    std::cerr << lang::configure_path_non_ascii() << to_posix_path_string(write_root) << "\n";
    return finish(2);
  }
  cli_verbose_phase("project", "paths_ascii_ok");
  const std::filesystem::path pkg_xml = write_root / "package.xml";

  std::error_code ec_pkg;
  std::filesystem::file_status pkg_st = std::filesystem::status(pkg_xml, ec_pkg);
  if (ec_pkg) {
    // MSVC/Win32 often reports ERROR_FILE_NOT_FOUND / ERROR_PATH_NOT_FOUND when package.xml
    // simply does not exist yet; treat as not_found instead of aborting.
    if (filesystem_status_error_is_missing(ec_pkg))
      pkg_st = std::filesystem::file_status(std::filesystem::file_type::not_found);
    else {
      std::cerr << "project: cannot stat " << to_posix_path_string(pkg_xml) << ": " << ec_pkg.message() << "\n";
      return finish(2);
    }
  }
  if (cli_verbose()) {
    if (!project_dir_arg.has_value()) {
      std::error_code ecl;
      const auto libcwd = std::filesystem::current_path(ecl);
      std::cerr << "project: [verbose] std::filesystem::current_path()="
                << to_posix_path_string(ecl ? std::filesystem::path{} : libcwd) << "\n";
#if defined(_WIN32)
      const auto w32cwd = win32_get_current_directory_or_empty();
      std::cerr << "project: [verbose] GetCurrentDirectoryW()=" << to_posix_path_string(w32cwd.empty() ? std::filesystem::path{} : w32cwd)
                << "\n";
#endif
    }
    std::cerr << "project: [verbose] scan_root=" << to_posix_path_string(scan_root)
              << " write_root=" << to_posix_path_string(write_root)
              << " (override roots with --project-dir|-C and/or --output-dir)\n"
              << "project: [verbose] package.xml path=" << to_posix_path_string(pkg_xml);
    if (!std::filesystem::exists(pkg_st))
      std::cerr << " status=not_found\n";
    else if (std::filesystem::is_regular_file(pkg_st))
      std::cerr << " status=regular_file\n";
    else if (std::filesystem::is_directory(pkg_st))
      std::cerr << " status=directory\n";
    else
      std::cerr << " status=other(type=" << static_cast<int>(pkg_st.type()) << ")\n";
    std::cerr << std::flush;
  }

  if (!force) {
    if (std::filesystem::is_directory(pkg_st)) {
      if (!dry_run) {
        std::cerr << "project: " << to_posix_path_string(pkg_xml)
                  << " exists as a directory (not a file). Remove or rename it before running `up project`.\n";
        return finish(2);
      }
    } else if (std::filesystem::exists(pkg_st) && !std::filesystem::is_regular_file(pkg_st)) {
      if (!dry_run) {
        std::cerr << "project: " << to_posix_path_string(pkg_xml)
                  << " exists but is not a regular file. Remove it or choose a different --output-dir / --project-dir.\n";
        return finish(2);
      }
    }
  }
  cli_verbose_phase("project", "pre_probe");

  const ProbeResult probe = probe_build_system(scan_root);
  ImportedPackage imported;
  std::string err;
  const bool cmake_scaffold =
      (probe.kind == BuildProbeKind::CMake && !legacy_cmake_parse);
  std::string scaffold_rel_warn;
  std::string cmake_rel_dir;
  CmakeTargetsProvenance cmake_targets_from = CmakeTargetsProvenance::None;
  cli_verbose_phase("project", "probe_done");

  if (cmake_query && probe.kind != BuildProbeKind::CMake) {
    std::cerr << "project: --cmake-query only applies to CMake projects (probe did not select CMake).\n";
    return finish(2);
  }

  if (cmake_scaffold) {
    cmake_rel_dir = cmake_source_dir_for_package_xml(scan_root, write_root, scaffold_rel_warn);
    if (!scaffold_rel_warn.empty())
      std::cout << "project: " << scaffold_rel_warn << "\n";
    std::string default_pkg_name = scan_root.filename().string();
    if (auto cmake_project_name = parse_cmake_project_name(probe.anchor); cmake_project_name.has_value())
      default_pkg_name = *cmake_project_name;
    imported.package_name = package_name_opt.value_or(default_pkg_name);
    if (imported.package_name == "." || imported.package_name == "..")
      imported.package_name = "package";
    imported.warnings.clear();
    imported.targets.clear();
    const std::filesystem::path cmake_source_dir = probe.anchor.parent_path();

    auto resolve_query_build_dir = [&]() -> std::optional<std::filesystem::path> {
      std::filesystem::path q = cmake_query_build_dir_arg.has_value()
                                    ? std::filesystem::path(*cmake_query_build_dir_arg)
                                    : (write_root / ".up" / "cmake_file_api_query");
      std::error_code ecq;
      q = std::filesystem::absolute(q, ecq);
      if (ecq || q.empty()) {
        std::cerr << "project: invalid --cmake-query-build-dir path\n";
        return std::nullopt;
      }
      return q;
    };
    auto cleanup_query_dir = [&](const std::filesystem::path& q) {
      if (!cmake_query_keep_build) {
        std::error_code ecrm;
        std::filesystem::remove_all(q, ecrm);
        if (ecrm)
          std::cout << "project: warning: could not remove cmake query build dir " << to_posix_path_string(q) << ": "
                    << ecrm.message() << "\n";
      }
    };

    if (cmake_query) {
      const std::optional<std::filesystem::path> qbd = resolve_query_build_dir();
      if (!qbd.has_value())
        return finish(2);
      if (!import_cmake_targets_from_file_api_query(cmake_source_dir, *qbd, imported, err)) {
        cleanup_query_dir(*qbd);
        std::cerr << "project: " << err << "\n";
        return finish(1);
      }
      if (!imported.targets.empty())
        cmake_targets_from = CmakeTargetsProvenance::FileApi;
      cleanup_query_dir(*qbd);
    } else {
      if (!import_cmake_installed_from_probe(probe.anchor, imported, err)) {
        std::cerr << "project: " << err << "\n";
        return finish(1);
      }
      std::cout << "project: [cmake] install_scan: " << imported.targets.size()
                << " target(s) (install(TARGETS ...) and add_library heuristics)\n";
      if (!imported.targets.empty())
        cmake_targets_from = CmakeTargetsProvenance::InstallScan;

      if (imported.targets.empty() && !cmake_no_file_api) {
        const std::optional<std::filesystem::path> qbd = resolve_query_build_dir();
        if (!qbd.has_value())
          return finish(2);
        ImportedPackage fa;
        std::string fa_err;
        if (import_cmake_targets_from_file_api_query(cmake_source_dir, *qbd, fa, fa_err)) {
          if (!fa.targets.empty()) {
            imported.targets = std::move(fa.targets);
            imported.warnings.insert(imported.warnings.end(), fa.warnings.begin(), fa.warnings.end());
            cmake_targets_from = CmakeTargetsProvenance::FileApi;
            std::cout << "project: [cmake] file_api: " << imported.targets.size() << " target(s) from codemodel\n";
          }
        } else {
          std::cout << "project: [cmake] file_api: skipped: " << fa_err << "\n";
        }
        cleanup_query_dir(*qbd);
      } else if (imported.targets.empty() && cmake_no_file_api) {
        std::cout << "project: [cmake] file_api: skipped (--cmake-no-file-api)\n";
      }

      if (imported.targets.empty()) {
        std::cout << "project: [cmake] source_scan: parsing CMakeLists (heuristic add_library / add_executable)\n";
        std::vector<std::string> leg_warn;
        std::string leg_err;
        import_cmake_file(probe.anchor, write_root, imported, leg_warn, leg_err);
        for (auto& w : leg_warn)
          imported.warnings.push_back(std::move(w));
        std::cout << "project: [cmake] source_scan: " << imported.targets.size() << " target(s)\n";
        if (!imported.targets.empty())
          cmake_targets_from = CmakeTargetsProvenance::SourceScan;
      } else if (cmake_no_file_api) {
        std::cout << "project: [cmake] source_scan: merging CMakeLists heuristics + static link/include hints "
                     "(--cmake-no-file-api)\n";
        ImportedPackage src_overlay;
        std::vector<std::string> owarn;
        std::string oerr;
        import_cmake_file(probe.anchor, write_root, src_overlay, owarn, oerr);
        for (auto& w : owarn)
          imported.warnings.push_back(std::move(w));
        const size_t overlay_n = src_overlay.targets.size();
        merge_cmake_source_overlay_into(imported, std::move(src_overlay));
        std::cout << "project: [cmake] source_scan: merged overlay; " << imported.targets.size()
                  << " target(s) total (overlay had " << overlay_n << ")\n";
        cmake_targets_from = CmakeTargetsProvenance::SourceScan;
      }
    }
    cli_verbose_phase("project", "cmake_import_done");
    // Keep CMake project() name (or --package-name override) as package.xml name.
    imported.package_name = package_name_opt.value_or(default_pkg_name);
    if (!cmake_query && imported.targets.empty()) {
      std::cout << "project: warning: CMake scaffold found no target stubs after install scan, File API, and source scan.\n";
      std::cout << "project: hint: add install(TARGETS ...) for shipped libs where applicable, or use --legacy-cmake-parse "
                   "for the old non-<cmake/> import path.\n";
    }
  } else if (!import_from_probe(scan_root, write_root, probe, imported, err)) {
    std::cerr << "project: " << err << "\n";
    return finish(1);
  }
  if (!cmake_scaffold)
    cli_verbose_phase("project", "heuristic_import_done");

  if (!cmake_scaffold && imported.targets.empty()) {
    std::cerr << "project: no targets generated.\n";
    return finish(1);
  }

  PackageDesc pkg;
  pkg.name = package_name_opt.value_or(imported.package_name);
  pkg.version = "0.1.0";
  if (cmake_scaffold) {
    PackageExternalCmake ec;
    ec.source_dir = cmake_rel_dir;
    pkg.external_cmake = std::move(ec);
    std::vector<std::pair<std::string, bool>> deps;
    std::string dep_err;
    if (import_cmake_dependencies_from_probe(probe.anchor, deps, dep_err)) {
      std::set<std::string> uniq;
      for (const auto& d : deps) {
        if (d.first.empty() || d.first == pkg.name || d.first == "cmake")
          continue;
        if (uniq.insert(d.first).second)
          pkg.dependencies.emplace_back(d.first, !d.second);
      }
    } else if (!dep_err.empty()) {
      std::cout << "project: warning: failed to parse CMake find_package deps: " << dep_err << "\n";
    }
  }
  cli_verbose_phase("project", "package_desc_ready");

  std::cout << "project: probe=" << probe_label(probe.kind) << "\n";
  if (cmake_scaffold) {
    if (cmake_query) {
      std::cout << "project: CMake scaffold: <cmake/> + target.xml from CMake File API (codemodel).\n";
    } else {
      std::cout << "project: CMake scaffold: <cmake/> + target.xml; ";
      switch (cmake_targets_from) {
        case CmakeTargetsProvenance::InstallScan:
          std::cout << "targets from install rules / add_library install heuristics.\n";
          break;
        case CmakeTargetsProvenance::FileApi:
          std::cout << "targets from CMake File API (codemodel).\n";
          break;
        case CmakeTargetsProvenance::SourceScan:
          std::cout << "targets from CMake source scan (heuristic).\n";
          break;
        default:
          std::cout << "no target stubs (package.xml only unless you edit XML).\n";
          break;
      }
    }
  } else {
    std::cout << "project: heuristic import: CMake/qmake/autotools parsing is incomplete; edit XML as needed.\n";
  }
  for (const auto& w : imported.warnings)
    std::cout << "project: warning: " << w << "\n";

  if (!force && !dry_run && std::filesystem::is_regular_file(pkg_st)) {
    const size_t n_files = 1 + imported.targets.size();
    std::cout << "project: would write " << n_files
              << " file(s) (nothing written; package.xml already exists; use --force or --dry-run for full XML):\n";
    std::cout << "  [package] " << to_posix_path_string(pkg_xml) << "\n";
    for (const auto& pr : imported.targets) {
      const auto tp = write_root / pr.first / "target.xml";
      std::cout << "  [target]  " << to_posix_path_string(tp);
      if (!pr.second.name.empty())
        std::cout << "  name=" << pr.second.name;
      if (!pr.second.type.empty())
        std::cout << "  type=" << pr.second.type;
      std::cout << "\n";
    }
    std::cerr << "project: " << to_posix_path_string(pkg_xml)
              << " already exists. Use --force to overwrite, or --dry-run to preview.\n"
              << "project: hint: write_root is process cwd at launch unless --output-dir; use --project-dir|-C if cwd is wrong.\n";
    return finish(2);
  }

  if (dry_run) {
    cli_verbose_phase("project", "dry_run_preview");
    std::ostringstream oss;
    write_package_xml(oss, pkg);
    std::cout << "--- " << to_posix_path_string(pkg_xml) << " ---\n";
    std::cout << oss.str() << "\n";
    for (const auto& pr : imported.targets) {
      const auto tp = write_root / pr.first / "target.xml";
      std::ostringstream to;
      write_target_xml(to, pr.second);
      std::cout << "--- " << to_posix_path_string(tp) << " ---\n";
      std::cout << to.str() << "\n";
    }
    return finish(0);
  }

  cli_verbose_phase("project", "write_files");
  std::filesystem::create_directories(write_root);
  for (const auto& pr : imported.targets)
    std::filesystem::create_directories(write_root / pr.first);

  std::string werr;
  if (!write_package_xml(pkg_xml, pkg, werr)) {
    std::cerr << "project: " << werr << "\n";
    return finish(1);
  }
  for (const auto& pr : imported.targets) {
    const auto tp = write_root / pr.first / "target.xml";
    if (!write_target_xml(tp, pr.second, werr)) {
      std::cerr << "project: " << werr << "\n";
      return finish(1);
    }
  }

  std::cout << "project: wrote " << to_posix_path_string(pkg_xml) << " and " << imported.targets.size()
            << " target.xml (under " << to_posix_path_string(write_root) << ").\n";
  cli_verbose_phase("project", "done");
  return finish(0);
}

}  // namespace up
