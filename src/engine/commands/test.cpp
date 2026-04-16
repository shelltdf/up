#include "test.hpp"

#include "commands_common.hpp"
#include "paths.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <vector>

namespace up {

int cmd_test(const std::filesystem::path& cwd, const std::string& test_name) {
  const std::string build_system_hint = resolve_build_system_from_cache(cwd);
  const auto build_root = default_build_root(cwd, build_system_hint);
  const std::string arch = resolve_arch_from_cache(cwd, build_root);
  const auto opts = load_up_options(build_root, arch);
  const std::string build_system = option_or_compat(opts, "UP_TARGET_BUILD_SYSTEM", "UP_BUILD_SYSTEM", "cmake");
  if (equals_ci(build_system, "ninja")) {
    const auto install_bin = default_install_root(cwd) / arch / "bin";
    if (!std::filesystem::exists(install_bin)) {
      std::cerr << "test: install tree missing; run `up build` first.\n";
      return 2;
    }
    std::vector<std::filesystem::path> tests;
    for (const auto& e : std::filesystem::directory_iterator(install_bin)) {
      if (!e.is_regular_file())
        continue;
      auto p = e.path();
#if defined(_WIN32)
      if (!equals_ci(p.extension().string(), ".exe"))
        continue;
#endif
      const std::string stem = p.stem().string();
      if (!test_name.empty()) {
        if (!equals_ci(stem, test_name))
          continue;
      } else if (!contains_ci(stem, "test")) {
        continue;
      }
      tests.push_back(p);
    }
    if (tests.empty()) {
      std::cerr << "test: no matching test executables in " << install_bin << "\n";
      return 2;
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
  const bool use_debug = equals_ci(option_or_compat(opts, "UP_TARGET_DEBUG", "UP_DEBUG", "OFF"), "ON");
  const std::string config_name = use_debug ? "Debug" : "Release";
  const auto bin_dir = build_root / arch / "out";
  if (!std::filesystem::exists(bin_dir)) {
    std::cerr << "test: build tree missing; run `up build` first.\n";
    return 2;
  }
  std::ostringstream cmd;
#if defined(_WIN32)
  cmd << "ctest --test-dir \"" << bin_dir.string() << "\" -C " << config_name << " --output-on-failure";
#else
  cmd << "ctest --test-dir \"" << bin_dir.string() << "\" --output-on-failure";
#endif
  if (!test_name.empty())
    cmd << " -R \"^" << test_name << "$\"";
  return std::system(cmd.str().c_str());
}

}  // namespace up
