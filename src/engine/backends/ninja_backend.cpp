#include "ninja_backend.hpp"

#include <sstream>

namespace up {

std::string build_ninja_install_command(const BuildBackendContext& ctx) {
  std::ostringstream cmd;
  cmd << "ninja -C \"" << ctx.bin_dir.string() << "\" install";
  return cmd.str();
}

}  // namespace up
