#include "archive/archive_backend.hpp"

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
      << (ctx.src_dir / "*").string() << "' -DestinationPath '" << archive.string() << "' -Force\"";
#else
  cmd << "tar -czf \"" << archive.string() << "\" -C \"" << ctx.src_dir.string() << "\" .";
#endif
  return cmd.str();
}

std::string build_cpack_command(const PackBackendContext& ctx) {
  std::ostringstream cmd;
  cmd << "cpack --config \"" << (ctx.build_out_dir / "CPackConfig.cmake").string() << "\""
      << " -B \"" << ctx.dst_dir.string() << "\"";
  if (!ctx.config_name.empty()) {
    cmd << " -C " << ctx.config_name;
  }
  return cmd.str();
}

}  // namespace up
