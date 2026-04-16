#include "test.hpp"

#include "core/backend_dispatch.hpp"
#include "commands_common.hpp"
#include "paths.hpp"

#include <filesystem>
#include <iostream>

namespace up {

int cmd_test(const std::filesystem::path& cwd, const std::string& test_name) {
  const std::string build_system_hint = resolve_build_system_from_cache(cwd);
  const auto build_root = default_build_root(cwd, build_system_hint);
  const std::string arch = resolve_arch_from_cache(cwd, build_root);
  const auto opts = load_up_options(build_root, arch);
  const std::string build_system = option_or_compat(opts, "UP_TARGET_BUILD_SYSTEM", "UP_BUILD_SYSTEM", "cmake");
  const TestBackendContext backend_ctx{
      build_root / arch / "out",
      default_install_root(cwd) / arch / "bin",
      equals_ci(option_or_compat(opts, "UP_TARGET_DEBUG", "UP_DEBUG", "OFF"), "ON") ? "Debug" : "Release",
      test_name};
  if (equals_ci(build_system, "ninja")) {
    if (!std::filesystem::exists(backend_ctx.install_bin_dir)) {
      std::cerr << "test: install tree missing; run `up build` first.\n";
      return 2;
    }
    return run_test_backend_ninja(backend_ctx, 2);
  }
  if (!std::filesystem::exists(backend_ctx.build_bin_dir)) {
    std::cerr << "test: build tree missing; run `up build` first.\n";
    return 2;
  }
  return run_test_backend_ctest(backend_ctx);
}

}  // namespace up
