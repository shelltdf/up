#include "test/ctest_backend.hpp"

#include "paths.hpp"

#include <sstream>

namespace gz {

std::string build_ctest_command(const TestBackendContext& ctx) {
  std::ostringstream cmd;
#if defined(_WIN32)
  cmd << "ctest --test-dir \"" << to_posix_path_string(ctx.build_bin_dir) << "\" -C " << ctx.config_name << " --output-on-failure";
#else
  cmd << "ctest --test-dir \"" << to_posix_path_string(ctx.build_bin_dir) << "\" --output-on-failure";
#endif
  if (!ctx.test_name.empty()) {
    cmd << " -R \"^" << ctx.test_name << "$\"";
  }
  return cmd.str();
}

}  // namespace gz
