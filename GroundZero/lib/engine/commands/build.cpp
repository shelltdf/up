#include "build.hpp"
#include "cli_verbose.hpp"
#include "core/backend_dispatch.hpp"
#include "commands_common.hpp"
#include "paths.hpp"
#include "redist_emit.hpp"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace gz {

namespace {

bool env_explicitly_disables_redist_emit(const char* name) {
  const char* e = std::getenv(name);
  if (!e || e[0] == '\0')
    return false;
  std::string s;
  for (const char* p = e; *p; ++p)
    s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(*p))));
  return s == "0" || s == "false" || s == "off" || s == "no";
}

}  // namespace

int cmd_build(const std::filesystem::path& cwd, const std::filesystem::path& build_dir, bool no_emit_redistribution_xml) {
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
  const int code = run_build_backend(backend_ctx, 1);
  if (code != 0)
    return code;
  const bool do_emit = !no_emit_redistribution_xml && !env_explicitly_disables_redist_emit("GZ_EMIT_REDIST_XML");
  if (!do_emit)
    return 0;
  const auto manifest_path = bd / "gz_redist_manifest.json";
  if (!std::filesystem::exists(manifest_path)) {
    std::cout
        << "build: skipping redistribution xml: no " << to_posix_path_string(manifest_path) << ".\n"
        << "  (Usually: main package has no library-like targets, only executable/asset; or run `gz configure` for this "
           "--build-dir-name. Output would be under " << to_posix_path_string(inst / "gz-redist")
        << "/package.xml.)\n";
    return 0;
  }
  GzRedistManifest man;
  std::string err;
  if (!read_gz_redist_manifest_json(manifest_path, man, err)) {
    std::cerr << "build: gz_redist_manifest.json: " << err << "\n";
    return 8;
  }
  if (man.targets.empty()) {
    std::cout << "build: emit redistribution xml: manifest has no library targets; skipping\n";
    return 0;
  }
  const int er = emit_gz_redistribution_xml(inst, man, err);
  if (er != 0) {
    std::cerr << "build: emit redistribution xml: " << err << "\n";
    return 9;
  }
  std::cout << "build: wrote redistribution package under " << to_posix_path_string(inst / "gz-redist") << std::endl;
  return 0;
}

}  // namespace gz
