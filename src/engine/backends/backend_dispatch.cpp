#include "backend_dispatch.hpp"

#include "archive_backend.hpp"
#include "cmake_backend.hpp"
#include "commands_common.hpp"
#include "ctest_backend.hpp"
#include "ninja_backend.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <vector>

namespace up {

int run_build_backend(const BuildBackendContext& ctx, int failure_return_code) {
  const std::string command =
      equals_ci(ctx.build_system, "ninja") ? build_ninja_install_command(ctx) : build_cmake_build_command(ctx);
  std::cout << command << "\n";
  const int code = std::system(command.c_str());
  if (code != 0) {
    std::cerr << "build: " << (equals_ci(ctx.build_system, "ninja") ? "ninja" : "cmake")
              << " failed with code " << code << "\n";
    return static_cast<unsigned>(code) > 255u ? failure_return_code : code;
  }
  return 0;
}

int run_test_backend_ctest(const TestBackendContext& ctx) {
  const std::string command = build_ctest_command(ctx);
  return std::system(command.c_str());
}

int run_test_backend_ninja(const TestBackendContext& ctx, int not_found_return_code) {
  std::vector<std::filesystem::path> tests;
  for (const auto& e : std::filesystem::directory_iterator(ctx.install_bin_dir)) {
    if (!e.is_regular_file())
      continue;
    auto p = e.path();
#if defined(_WIN32)
    if (!equals_ci(p.extension().string(), ".exe"))
      continue;
#endif
    const std::string stem = p.stem().string();
    if (!ctx.test_name.empty()) {
      if (!equals_ci(stem, ctx.test_name))
        continue;
    } else if (!contains_ci(stem, "test")) {
      continue;
    }
    tests.push_back(p);
  }
  if (tests.empty()) {
    std::cerr << "test: no matching test executables in " << ctx.install_bin_dir << "\n";
    return not_found_return_code;
  }
  for (const auto& t : tests) {
    std::ostringstream cmd;
    cmd << "\"" << t.string() << "\"";
    const int code = std::system(cmd.str().c_str());
    if (code != 0)
      return static_cast<unsigned>(code) > 255u ? 1 : code;
  }
  return 0;
}

int run_pack_backend(const PackBackendContext& ctx) {
  const auto archive = archive_output_path(ctx);
  std::error_code ec;
  if (std::filesystem::exists(archive))
    std::filesystem::remove(archive, ec);
  const std::string command = build_archive_command(ctx);
  const int code = std::system(command.c_str());
  if (code != 0) {
    std::cerr << "pack: archive command failed with code " << code << "\n";
    return static_cast<unsigned>(code) > 255u ? 1 : code;
  }
  std::cout << "pack: " << ctx.src_dir << " -> " << archive << "\n";
  return 0;
}

int run_configure_backend(const ConfigureBackendContext& ctx) {
  const std::string command = build_cmake_configure_command(ctx);
  const int code = std::system(command.c_str());
  if (code != 0) {
    std::cerr << "configure: cmake configure failed with code " << code << "\n";
    return static_cast<unsigned>(code) > 255u ? 1 : code;
  }
  return 0;
}

}  // namespace up
