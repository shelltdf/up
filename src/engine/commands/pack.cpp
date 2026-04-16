#include "pack.hpp"

#include "core/backend_dispatch.hpp"
#include "commands_common.hpp"
#include "paths.hpp"

#include <filesystem>
#include <iostream>

namespace up {

int cmd_pack(const std::filesystem::path& cwd) {
  const std::string build_system_hint = resolve_build_system_from_cache(cwd);
  const auto build_root = default_build_root(cwd, build_system_hint);
  const std::string arch = resolve_arch_from_cache(cwd, build_root);
  const auto src = default_install_root(cwd) / arch;
  const auto dst_dir = default_pack_root(cwd) / arch;
  if (!std::filesystem::exists(src)) {
    std::cerr << "pack: install tree missing; run `up build` first (expected " << src << ")\n";
    return 2;
  }
  std::error_code ec;
  std::filesystem::create_directories(dst_dir, ec);
  if (ec) {
    std::cerr << "pack: cannot create " << dst_dir << ": " << ec.message() << "\n";
    return 3;
  }

  const PackBackendContext backend_ctx{src, dst_dir, arch};
  return run_pack_backend(backend_ctx);
}

}  // namespace up
