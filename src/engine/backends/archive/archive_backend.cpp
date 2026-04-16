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

}  // namespace up
