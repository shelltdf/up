#include "cmake/cmake_third_party_heuristics.hpp"

#include "paths.hpp"

#include <filesystem>

namespace up::cmake_third_party {
namespace {

// zlib: zlib.h includes zconf.h; upstream CMake generates zconf.h from zconf.h.cmakein into the binary dir.
void try_emit_zlib_zconf(std::ostringstream& cm, const std::filesystem::path& source_tree) {
  std::error_code ec;
  const auto cmakein = source_tree / "zconf.h.cmakein";
  if (!std::filesystem::is_regular_file(cmakein, ec))
    return;
  const auto zlib_h = source_tree / "zlib.h";
  if (!std::filesystem::is_regular_file(zlib_h, ec))
    return;

  const std::string in_path = to_posix_path_string(std::filesystem::absolute(cmakein));
  cm << "configure_file(\"" << in_path << "\" \"${CMAKE_CURRENT_BINARY_DIR}/zconf.h\" @ONLY)\n";
  cm << "include_directories(\"${CMAKE_CURRENT_BINARY_DIR}\")\n";
}

}  // namespace

void emit_detected_upstream_snippets(std::ostringstream& cm, const std::filesystem::path& inferred_package_source_root) {
  try_emit_zlib_zconf(cm, inferred_package_source_root);
}

}  // namespace up::cmake_third_party
