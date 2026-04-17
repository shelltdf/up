#pragma once

#include <filesystem>
#include <sstream>

namespace up::cmake_third_party {

// Append CMake fragments for known upstream source layouts (zlib, etc.).
// `inferred_package_source_root` is the inferred single-tree root (directory containing package sources).
void emit_detected_upstream_snippets(std::ostringstream& cm, const std::filesystem::path& inferred_package_source_root);

}  // namespace up::cmake_third_party
