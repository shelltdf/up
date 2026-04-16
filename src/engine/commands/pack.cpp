#include "pack.hpp"

#include "commands_common.hpp"
#include "paths.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>

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

#if defined(_WIN32)
  const auto archive = dst_dir / ("up-" + arch + ".zip");
  if (std::filesystem::exists(archive))
    std::filesystem::remove(archive, ec);
  std::ostringstream cmd;
  cmd << "powershell -NoProfile -Command \"Compress-Archive -Path '"
      << (src / "*").string() << "' -DestinationPath '" << archive.string() << "' -Force\"";
#else
  const auto archive = dst_dir / ("up-" + arch + ".tar.gz");
  if (std::filesystem::exists(archive))
    std::filesystem::remove(archive, ec);
  std::ostringstream cmd;
  cmd << "tar -czf \"" << archive.string() << "\" -C \"" << src.string() << "\" .";
#endif

  const int code = std::system(cmd.str().c_str());
  if (code != 0) {
    std::cerr << "pack: archive command failed with code " << code << "\n";
    return static_cast<unsigned>(code) > 255u ? 1 : code;
  }
  std::cout << "pack: " << src << " -> " << archive << "\n";
  return 0;
}

}  // namespace up
