#include "test.hpp"

#include "core/backend_dispatch.hpp"
#include "paths.hpp"

#include <filesystem>
#include <iostream>

namespace up {

int cmd_test(const std::filesystem::path& install_dir, const std::string& test_name) {
  const auto inst = std::filesystem::absolute(install_dir);
  const auto bin = inst / "bin";
  if (!std::filesystem::exists(bin)) {
    std::cerr << "test: missing install bin directory: " << to_posix_path_string(bin) << "\n";
    return 2;
  }
  const TestBackendContext backend_ctx{
      bin,
      bin,
      "Release",
      test_name};
  return run_test_backend_ninja(backend_ctx, 2);
}

}  // namespace up
