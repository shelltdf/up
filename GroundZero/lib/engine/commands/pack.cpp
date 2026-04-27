#include "pack.hpp"

#include "cli_verbose.hpp"
#include "core/backend_dispatch.hpp"
#include "paths.hpp"

#include <filesystem>
#include <iostream>
#include <map>
#include <string>

namespace gz {

int cmd_pack(const std::filesystem::path& cwd, const std::vector<std::filesystem::path>& install_dirs) {
  cli_verbose_phase("pack", "start");
  if (install_dirs.empty()) {
    std::cerr << "pack: missing --install-dir-name <name> (repeatable; under .intermediate/install)\n";
    return 2;
  }

  int last = 0;
  for (const auto& install_dir : install_dirs) {
    const auto src = std::filesystem::absolute(install_dir);
    if (!std::filesystem::exists(src)) {
      std::cerr << "pack: install tree missing: " << to_posix_path_string(src) << "\n";
      return 2;
    }

    const std::string arch = src.filename().string();
    if (arch.empty()) {
      std::cerr << "pack: cannot infer arch tag from install path: " << to_posix_path_string(src) << "\n";
      return 2;
    }

    const auto dst_dir = default_pack_root(cwd) / arch;
    std::error_code ec;
    std::filesystem::create_directories(dst_dir, ec);
    if (ec) {
      std::cerr << "pack: cannot create " << to_posix_path_string(dst_dir) << ": " << ec.message() << "\n";
      return 3;
    }

    const std::map<std::string, std::string> opts;
    const std::string build_system = "cmake";
    const std::string config_name = "Release";
    const std::string pack_backend = "auto";
    const bool allow_fallback = true;
    const PackBackendContext backend_ctx{
        src,
        dst_dir,
        arch,
        build_system,
        {},
        config_name,
        pack_backend,
        allow_fallback};
    cli_verbose_phase("pack", "pack_backend");
    last = run_pack_backend(backend_ctx);
    if (last != 0)
      return last;
  }
  return last;
}

}  // namespace gz
