#include "reverse_import.hpp"

#include <filesystem>

namespace up {

ProbeResult probe_build_system(const std::filesystem::path& scan_root) {
  ProbeResult r;
  std::error_code ec;
  if (std::filesystem::exists(scan_root / "CMakeLists.txt", ec)) {
    r.kind = BuildProbeKind::CMake;
    r.anchor = scan_root / "CMakeLists.txt";
    return r;
  }
  if (std::filesystem::exists(scan_root / "Makefile.am", ec)) {
    r.kind = BuildProbeKind::Autotools;
    r.anchor = scan_root / "Makefile.am";
    return r;
  }
  for (const auto& ent : std::filesystem::directory_iterator(scan_root, ec)) {
    if (!ent.is_regular_file())
      continue;
    if (ent.path().extension() == ".pro") {
      r.kind = BuildProbeKind::QMake;
      r.anchor = ent.path();
      return r;
    }
  }
  if (std::filesystem::exists(scan_root / "meson.build", ec)) {
    r.kind = BuildProbeKind::Meson;
    r.anchor = scan_root / "meson.build";
    return r;
  }
  r.kind = BuildProbeKind::SourceTreeFallback;
  r.anchor = scan_root;
  return r;
}

}  // namespace up
