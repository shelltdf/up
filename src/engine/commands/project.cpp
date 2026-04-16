#include "project.hpp"

#include "project_import.hpp"
#include "lang.hpp"
#include "path_check.hpp"
#include "paths.hpp"
#include "simple_xml.hpp"

#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace up {
namespace {

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

}  // namespace

int cmd_project(const std::filesystem::path& cwd, const std::vector<std::string>& args) {
  bool dry_run = false;
  bool force = false;
  std::optional<std::filesystem::path> output_dir;
  std::optional<std::string> package_name_opt;

  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& a = args[i];
    if (a == "--dry-run")
      dry_run = true;
    else if (a == "--force")
      force = true;
    else if (a == "--output-dir" && i + 1 < args.size())
      output_dir = args[++i];
    else if (a == "--package-name" && i + 1 < args.size())
      package_name_opt = args[++i];
    else {
      std::cerr << "project: unknown option: " << a << "\n"
                << "  --dry-run       print probe result and XML; do not write files\n"
                << "  --force         overwrite existing package.xml / target.xml\n"
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
  if (!import_from_probe(scan_root, write_root, probe, imported, err)) {
    std::cerr << "project: " << err << "\n";
    return 1;
  }

  if (imported.targets.empty()) {
    std::cerr << "project: no targets generated.\n";
    return 1;
  }

  PackageDesc pkg;
  pkg.name = package_name_opt.value_or(imported.package_name);
  pkg.version = "0.1.0";

  std::cout << "project: probe=" << probe_label(probe.kind) << "\n";
  std::cout << "project: heuristic import — CMake/qmake/autotools parsing is incomplete; edit XML as needed.\n";
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
