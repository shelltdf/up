#include "run.hpp"

#include "commands_common.hpp"
#include "paths.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>

namespace up {

int cmd_run(const std::filesystem::path& cwd, const std::string& target_name) {
  const std::string build_system_hint = resolve_build_system_from_cache(cwd);
  const auto build_root = default_build_root(cwd, build_system_hint);
  const std::string arch = resolve_arch_from_cache(cwd, build_root);
  const auto exe_dir = default_install_root(cwd) / arch / "bin";
  std::filesystem::path exe = exe_dir / target_name;
#if defined(_WIN32)
  if (exe.extension().empty())
    exe.replace_filename(exe.filename().string() + ".exe");
#endif
  if (!std::filesystem::exists(exe)) {
    std::cerr << "run: executable not found: " << exe << "\n";
    return 2;
  }
  std::ostringstream cmd;
  cmd << "\"" << exe.string() << "\"";
  return std::system(cmd.str().c_str());
}

}  // namespace up
