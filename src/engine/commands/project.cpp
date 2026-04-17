#include "project.hpp"

#include "project_import.hpp"
#include "lang.hpp"
#include "path_check.hpp"
#include "paths.hpp"
#include "simple_xml.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace up {
namespace {

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
  std::smatch m;
  if (!std::regex_search(raw, m, std::regex(R"rx(project\s*\(\s*([^\s\)]+))rx", std::regex::icase)))
    return std::nullopt;
  std::string name = m[1].str();
  if (name.size() >= 2 && ((name.front() == '"' && name.back() == '"') || (name.front() == '\'' && name.back() == '\'')))
    name = name.substr(1, name.size() - 2);
  if (name.empty())
    return std::nullopt;
  return name;
}

}  // namespace

int cmd_project(const std::filesystem::path& cwd, const std::vector<std::string>& args) {
  bool dry_run = false;
  bool force = false;
  bool legacy_cmake_parse = false;
  std::optional<std::filesystem::path> output_dir;
  std::optional<std::string> package_name_opt;

  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& a = args[i];
    if (a == "--dry-run")
      dry_run = true;
    else if (a == "--force")
      force = true;
    else if (a == "--legacy-cmake-parse")
      legacy_cmake_parse = true;
    else if (a == "--output-dir" && i + 1 < args.size())
      output_dir = args[++i];
    else if (a == "--package-name" && i + 1 < args.size())
      package_name_opt = args[++i];
    else {
      std::cerr << "project: unknown option: " << a << "\n"
                << "  --dry-run       print probe result and XML; do not write files\n"
                << "  --force         overwrite existing package.xml / target.xml\n"
                << "  --legacy-cmake-parse  for CMake projects: parse add_library/add_executable into target.xml\n"
                << "                         (fragile). Default writes <cmake/> plus imported_installed wrappers from install rules.\n"
                << "  --output-dir <path>  package root for generated files (default: cwd). target.xml is placed next\n"
                << "                         to each target's CMake/.pro tree when possible, else under .targets/<name>/.\n"
                << "  --package-name <name>  override <package name=\"...\">\n";
      return 2;
    }
  }

  const std::filesystem::path scan_root = cwd;
  std::filesystem::path write_root = output_dir.value_or(cwd);

  std::error_code ec;
  write_root = std::filesystem::absolute(write_root, ec);
  if (ec) {
    std::cerr << "project: invalid --output-dir\n";
    return 2;
  }

  if (path_has_non_ascii(scan_root) || path_has_non_ascii(write_root)) {
    std::cerr << lang::configure_path_non_ascii() << to_posix_path_string(write_root) << "\n";
    return 2;
  }

  const std::filesystem::path pkg_xml = write_root / "package.xml";
  if (std::filesystem::exists(pkg_xml) && !force) {
    if (!dry_run) {
      std::cerr << "project: " << to_posix_path_string(pkg_xml)
                << " already exists. Use --force to overwrite, or --dry-run to preview.\n";
      return 2;
    }
  }

  const ProbeResult probe = probe_build_system(scan_root);
  ImportedPackage imported;
  std::string err;
  const bool cmake_scaffold =
      (probe.kind == BuildProbeKind::CMake && !legacy_cmake_parse);
  std::string scaffold_rel_warn;
  std::string cmake_rel_dir;

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
    if (!import_cmake_installed_from_probe(probe.anchor, imported, err)) {
      std::cerr << "project: " << err << "\n";
      return 1;
    }
    // Keep CMake project() name (or --package-name override) as package.xml name.
    imported.package_name = package_name_opt.value_or(default_pkg_name);
    if (imported.targets.empty()) {
      std::cout << "project: warning: no install(TARGETS ...) libraries detected; generated package.xml only.\n";
      std::cout << "project: hint: keep <cmake/> and add imported_installed_* target.xml manually, or use --legacy-cmake-parse.\n";
    }
  } else if (!import_from_probe(scan_root, write_root, probe, imported, err)) {
    std::cerr << "project: " << err << "\n";
    return 1;
  }

  if (!cmake_scaffold && imported.targets.empty()) {
    std::cerr << "project: no targets generated.\n";
    return 1;
  }

  PackageDesc pkg;
  pkg.name = package_name_opt.value_or(imported.package_name);
  pkg.version = "0.1.0";
  if (cmake_scaffold) {
    PackageExternalCmake ec;
    ec.source_dir = cmake_rel_dir;
    pkg.external_cmake = std::move(ec);
  }

  std::cout << "project: probe=" << probe_label(probe.kind) << "\n";
  if (cmake_scaffold) {
    std::cout << "project: CMake scaffold — wrote <cmake/> and imported_installed wrapper target.xml when install rules are detectable.\n";
  } else {
    std::cout << "project: heuristic import — CMake/qmake/autotools parsing is incomplete; edit XML as needed.\n";
  }
  for (const auto& w : imported.warnings)
    std::cout << "project: warning: " << w << "\n";

  if (dry_run) {
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
    return 0;
  }

  std::filesystem::create_directories(write_root);
  for (const auto& pr : imported.targets)
    std::filesystem::create_directories(write_root / pr.first);

  std::string werr;
  if (!write_package_xml(pkg_xml, pkg, werr)) {
    std::cerr << "project: " << werr << "\n";
    return 1;
  }
  for (const auto& pr : imported.targets) {
    const auto tp = write_root / pr.first / "target.xml";
    if (!write_target_xml(tp, pr.second, werr)) {
      std::cerr << "project: " << werr << "\n";
      return 1;
    }
  }

  std::cout << "project: wrote " << to_posix_path_string(pkg_xml) << " and " << imported.targets.size()
            << " target.xml (under " << to_posix_path_string(write_root) << ").\n";
  return 0;
}

}  // namespace up
