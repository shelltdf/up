#include "ctest_backend.hpp"

#include <sstream>

namespace up {

std::string build_ctest_command(const TestBackendContext& ctx) {
  std::ostringstream cmd;
#if defined(_WIN32)
  cmd << "ctest --test-dir \"" << ctx.build_bin_dir.string() << "\" -C " << ctx.config_name << " --output-on-failure";
#else
  cmd << "ctest --test-dir \"" << ctx.build_bin_dir.string() << "\" --output-on-failure";
#endif
  if (!ctx.test_name.empty()) {
    cmd << " -R \"^" << ctx.test_name << "$\"";
  }
  return cmd.str();
}

}  // namespace up
