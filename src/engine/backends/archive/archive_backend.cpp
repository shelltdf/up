#include "archive/archive_backend.hpp"

#include "paths.hpp"

#include <sstream>

namespace up {

std::filesystem::path archive_output_path(const PackBackendContext& ctx) {
#if defined(_WIN32)
  return ctx.dst_dir / ("up-" + ctx.arch + ".zip");
#else
  return ctx.dst_dir / ("up-" + ctx.arch + ".tar.gz");
#endif
}

std::string build_archive_command(const PackBackendContext& ctx) {
  const auto archive = archive_output_path(ctx);
  std::ostringstream cmd;
#if defined(_WIN32)
  cmd << "powershell -NoProfile -Command \"Compress-Archive -Path '"
      << to_posix_path_string(ctx.src_dir / "*") << "' -DestinationPath '" << to_posix_path_string(archive) << "' -Force\"";
#else
  cmd << "tar -czf \"" << to_posix_path_string(archive) << "\" -C \"" << to_posix_path_string(ctx.src_dir) << "\" .";
#endif
  return cmd.str();
}

std::string build_cpack_command(const PackBackendContext& ctx) {
  std::ostringstream cmd;
  cmd << "cpack --config \"" << to_posix_path_string(ctx.build_out_dir / "CPackConfig.cmake") << "\""
      << " -B \"" << to_posix_path_string(ctx.dst_dir) << "\"";
  if (!ctx.config_name.empty()) {
    cmd << " -C " << ctx.config_name;
  }
  return cmd.str();
}

}  // namespace up
