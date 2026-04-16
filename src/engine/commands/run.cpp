#include "run.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>

namespace up {

int cmd_run(const std::filesystem::path& install_dir, const std::string& target_name) {
  const auto inst = std::filesystem::absolute(install_dir);
  const auto exe_dir = inst / "bin";
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
