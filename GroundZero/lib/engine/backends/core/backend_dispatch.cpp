#include "core/backend_dispatch.hpp"

#include "archive/archive_backend.hpp"
#include "cmake/cmake_backend.hpp"
#include "commands_common.hpp"
#include "paths.hpp"
#include "ninja/ninja_backend.hpp"
#include "test/ctest_backend.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <vector>

namespace gz {

namespace {

bool cpack_available() {
#if defined(_WIN32)
  const int code = std::system("cpack --version >nul 2>&1");
#else
  const int code = std::system("cpack --version >/dev/null 2>&1");
#endif
  return code == 0;
}

bool try_run_cpack(const PackBackendContext& ctx, int& out_code) {
  if (!std::filesystem::exists(ctx.build_out_dir / "CPackConfig.cmake"))
    return false;
  if (!cpack_available())
    return false;
  std::error_code ec;
  std::filesystem::create_directories(ctx.dst_dir, ec);
  const std::string command = build_cpack_command(ctx);
  std::cout << command << "\n";
  const int code = std::system(command.c_str());
  if (code == 0) {
    std::cout << "pack: cpack output -> " << to_posix_path_string(ctx.dst_dir) << "\n";
    out_code = 0;
    return true;
  }
  out_code = static_cast<unsigned>(code) > 255u ? 1 : code;
  return false;
}

int run_archive_pack(const PackBackendContext& ctx) {
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
  std::cout << "pack: " << to_posix_path_string(ctx.src_dir) << " -> " << to_posix_path_string(archive) << "\n";
  return 0;
}

}  // namespace

int run_build_backend(const BuildBackendContext& ctx, int failure_return_code) {
  const std::string command =
      equals_ci(ctx.build_system, "ninja") ? build_ninja_install_command(ctx) : build_cmake_build_command(ctx);
  std::cout << command << "\n" << std::flush;
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
    std::cerr << "test: no matching test executables in " << to_posix_path_string(ctx.install_bin_dir) << "\n";
    return not_found_return_code;
  }
  for (const auto& t : tests) {
    std::ostringstream cmd;
    cmd << "\"" << to_posix_path_string(t) << "\"";
    const int code = std::system(cmd.str().c_str());
    if (code != 0)
      return static_cast<unsigned>(code) > 255u ? 1 : code;
  }
  return 0;
}

int run_pack_backend(const PackBackendContext& ctx) {
  const std::string mode = lower_ascii(ctx.pack_backend.empty() ? "auto" : ctx.pack_backend);
  if (mode == "archive")
    return run_archive_pack(ctx);

  if (mode == "cpack") {
    int cpack_code = 1;
    if (try_run_cpack(ctx, cpack_code))
      return 0;
    std::cerr << "pack: cpack failed or unavailable";
    if (!ctx.allow_fallback) {
      std::cerr << " (fallback disabled)\n";
      return cpack_code;
    }
    std::cerr << ", falling back to archive backend\n";
    return run_archive_pack(ctx);
  }

  if (mode == "auto") {
    if (equals_ci(ctx.build_system, "cmake")) {
      int cpack_code = 1;
      if (try_run_cpack(ctx, cpack_code))
        return 0;
      if (!ctx.allow_fallback) {
        std::cerr << "pack: cpack failed or unavailable and fallback disabled\n";
        return cpack_code;
      }
      std::cerr << "pack: cpack not available/failed, falling back to archive backend\n";
    }
    return run_archive_pack(ctx);
  }

  std::cerr << "pack: unsupported GZ_PACK_BACKEND=" << ctx.pack_backend << " (expected auto/archive/cpack)\n";
  return 3;
}

int run_generate_backend(const ConfigureGraphModel& model) {
  if (equals_ci(model.build_system, "ninja")) {
    return write_ninja_file(model);
  }
  return write_cmake_lists(model);
}

int run_configure_backend(const ConfigureBackendContext& ctx) {
  std::cout << std::flush;
  const std::string command = build_cmake_configure_command(ctx);
  const int code = std::system(command.c_str());
  if (code != 0) {
    std::cerr << "configure: cmake configure failed with code " << code << "\n";
    return static_cast<unsigned>(code) > 255u ? 1 : code;
  }
  return 0;
}

}  // namespace gz
