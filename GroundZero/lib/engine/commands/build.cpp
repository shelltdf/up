#include "build.hpp"
#include "cli_verbose.hpp"
#include "core/backend_dispatch.hpp"
#include "commands_common.hpp"
#include "paths.hpp"

#include <filesystem>
#include <iostream>

namespace gz {

int cmd_build(const std::filesystem::path& cwd, const std::filesystem::path& build_dir) {
  cli_verbose_phase("build", "start");
  (void)cwd;
  const auto bd = std::filesystem::absolute(build_dir);
  const auto cache = bd / "gz_cache.txt";
  if (!std::filesystem::exists(cache)) {
    std::cerr << "build: missing " << to_posix_path_string(cache) << " (run `gz configure` for this --build-dir-name first)\n";
    return 2;
  }
  const auto opts = load_gz_options_from_build_dir(bd);
  std::string arch = read_plain_cache_value(cache, "arch");
  if (arch.empty())
    arch = arch_from_options(opts);
  const auto src_dir = bd;
  const auto inst = default_install_root(cwd) / arch;
  const std::string build_system =
      option_or_compat(opts, "GZ_TARGET_BUILD_SYSTEM", "GZ_BUILD_SYSTEM", "cmake");
  if (!equals_ci(build_system, "cmake") && !equals_ci(build_system, "ninja")) {
    std::cerr << "build: unsupported GZ_TARGET_BUILD_SYSTEM=" << build_system << " (expected cmake/ninja)\n";
    return 3;
  }
  std::filesystem::create_directories(inst);
  const auto bin_dir = src_dir / "out";
  std::filesystem::create_directories(bin_dir);
  if (equals_ci(build_system, "ninja")) {
    if (!std::filesystem::exists(src_dir / "out" / "build.ninja")) {
      std::cerr << "build: run `gz configure` first (missing " << to_posix_path_string(src_dir / "out" / "build.ninja")
                << ")\n";
      return 2;
    }
  } else if (!std::filesystem::exists(src_dir / "CMakeLists.txt")) {
    std::cerr << "build: run `gz configure` first (missing " << to_posix_path_string(src_dir / "CMakeLists.txt")
              << ")\n";
    return 2;
  }
  const std::string cmake_generator = option_or(opts, "GZ_CMAKE_GENERATOR", "");
  const bool use_debug = equals_ci(option_or_compat(opts, "GZ_TARGET_DEBUG", "GZ_DEBUG", "OFF"), "ON");
  const std::string config_name = use_debug ? "Debug" : "Release";

  bool multi_config = contains_ci(cmake_generator, "visual studio") || contains_ci(cmake_generator, "multi-config");
#if defined(_WIN32)
  if (equals_ci(build_system, "cmake") && cmake_generator.empty())
    multi_config = true;  // default VS generator on Windows
#endif
  const std::string cmake_prefix_path = option_or(opts, "GZ_CMAKE_PREFIX_PATH", "");
  BuildBackendContext backend_ctx{src_dir,
                                    bin_dir,
                                    inst,
                                    opts,
                                    build_system,
                                    cmake_generator,
                                    config_name,
                                    multi_config,
                                    cmake_prefix_path};
  cli_verbose_phase("build", "build_backend");
  return run_build_backend(backend_ctx, 1);
}

}  // namespace gz
