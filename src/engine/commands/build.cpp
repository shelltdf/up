#include "build.hpp"
#include "backend_dispatch.hpp"
#include "commands_common.hpp"
#include "paths.hpp"

#include <filesystem>
#include <iostream>

namespace up {

int cmd_build(const std::filesystem::path& cwd) {
  const std::string build_system_hint = resolve_build_system_from_cache(cwd);
  const auto build_root = default_build_root(cwd, build_system_hint);
  const std::string arch = resolve_arch_from_cache(cwd, build_root);
  const auto opts = load_up_options(build_root, arch);
  const auto src_dir = build_root / arch;
  const auto inst = default_install_root(cwd) / arch;
  const std::string build_system =
      option_or_compat(opts, "UP_TARGET_BUILD_SYSTEM", "UP_BUILD_SYSTEM", "cmake");
  if (!equals_ci(build_system, "cmake") && !equals_ci(build_system, "ninja")) {
    std::cerr << "build: unsupported UP_TARGET_BUILD_SYSTEM=" << build_system << " (expected cmake/ninja)\n";
    return 3;
  }
  std::filesystem::create_directories(inst);
  const auto bin_dir = src_dir / "out";
  std::filesystem::create_directories(bin_dir);
  if (equals_ci(build_system, "ninja")) {
    if (!std::filesystem::exists(src_dir / "out" / "build.ninja")) {
      std::cerr << "build: run `up configure` first (missing " << (src_dir / "out" / "build.ninja") << ")\n";
      return 2;
    }
  } else if (!std::filesystem::exists(src_dir / "CMakeLists.txt")) {
    std::cerr << "build: run `up configure` first (missing " << (src_dir / "CMakeLists.txt") << ")\n";
    return 2;
  }
  const std::string cmake_generator = option_or(opts, "UP_CMAKE_GENERATOR", "");
  const bool use_debug = equals_ci(option_or_compat(opts, "UP_TARGET_DEBUG", "UP_DEBUG", "OFF"), "ON");
  const std::string config_name = use_debug ? "Debug" : "Release";

  bool multi_config = contains_ci(cmake_generator, "visual studio") || contains_ci(cmake_generator, "multi-config");
#if defined(_WIN32)
  if (equals_ci(build_system, "cmake") && cmake_generator.empty())
    multi_config = true;  // default VS generator on Windows
#endif
  BuildBackendContext backend_ctx{
      src_dir, bin_dir, inst, opts, build_system, cmake_generator, config_name, multi_config};
  return run_build_backend(backend_ctx, 1);
}

}  // namespace up
