#include "reverse_import.hpp"

#include "reverse_import_internal.hpp"
#include "reverse_import_common.hpp"

namespace up {

bool import_from_probe(const std::filesystem::path& scan_root, const std::filesystem::path& write_root, const ProbeResult& probe,
                       ImportedPackage& out, std::string& error) {
  out.targets.clear();
  out.warnings.clear();
  std::string pn = scan_root.filename().string();
  if (pn.empty() || pn == "." || pn == "..")
    pn = "package";
  out.package_name = reverse_import::sanitize_id(pn);

  std::vector<std::string> warnings;

  switch (probe.kind) {
    case BuildProbeKind::CMake: {
      import_cmake_file(probe.anchor, write_root, out, warnings, error);
      if (!out.targets.empty()) {
        out.warnings = std::move(warnings);
        return true;
      }
      import_source_fallback(scan_root, write_root, out, warnings, error);
      out.warnings = std::move(warnings);
      return error.empty();
    }
    case BuildProbeKind::Autotools: {
      import_autotools(scan_root, write_root, out, warnings, error);
      if (!out.targets.empty()) {
        out.warnings = std::move(warnings);
        return true;
      }
      import_source_fallback(scan_root, write_root, out, warnings, error);
      out.warnings = std::move(warnings);
      return error.empty();
    }
    case BuildProbeKind::QMake: {
      import_qmake(probe.anchor, scan_root, write_root, out, warnings, error);
      if (!out.targets.empty()) {
        out.warnings = std::move(warnings);
        return true;
      }
      import_source_fallback(scan_root, write_root, out, warnings, error);
      out.warnings = std::move(warnings);
      return error.empty();
    }
    case BuildProbeKind::Meson: {
      import_meson_basic(scan_root, write_root, out, warnings, error);
      import_source_fallback(scan_root, write_root, out, warnings, error);
      out.warnings.insert(out.warnings.end(), warnings.begin(), warnings.end());
      return error.empty();
    }
    case BuildProbeKind::SourceTreeFallback:
      import_source_fallback(scan_root, write_root, out, warnings, error);
      out.warnings = std::move(warnings);
      return error.empty();
  }
  error = "internal: unknown probe kind";
  return false;
}

bool import_cmake_installed_from_probe(const std::filesystem::path& cmake_file, ImportedPackage& out, std::string& error) {
  out.targets.clear();
  out.warnings.clear();
  std::string pn = cmake_file.parent_path().filename().string();
  if (pn.empty() || pn == "." || pn == "..")
    pn = "package";
  out.package_name = reverse_import::sanitize_id(pn);
  import_cmake_installed_wrappers(cmake_file, out, out.warnings, error);
  return error.empty();
}

bool import_cmake_dependencies_from_probe(const std::filesystem::path& cmake_file,
                                          std::vector<std::pair<std::string, bool>>& deps, std::string& error) {
  deps.clear();
  import_cmake_find_package_deps(cmake_file, deps, error);
  return error.empty();
}

}  // namespace up
