#include "test.hpp"

#include "cli_verbose.hpp"
#include "core/backend_dispatch.hpp"
#include "paths.hpp"

#include <filesystem>
#include <iostream>

namespace gz {

int cmd_test(const std::filesystem::path& install_dir, const std::string& test_name) {
  cli_verbose_phase("test", "start");
  const auto inst = std::filesystem::absolute(install_dir);
  const auto bin = inst / "bin";
  if (!std::filesystem::exists(bin)) {
    std::cerr << "test: missing install bin directory: " << to_posix_path_string(bin)
              << " (library-only package may have no executable tests)\n";
    return 2;
  }
  const TestBackendContext backend_ctx{
      bin,
      bin,
      "Release",
      test_name};
  cli_verbose_phase("test", "test_backend");
  return run_test_backend_ninja(backend_ctx, 2);
}

}  // namespace gz
