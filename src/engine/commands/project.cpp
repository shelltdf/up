#include "project.hpp"

#include <iostream>

namespace up {

int cmd_project(const std::filesystem::path& cwd) {
  std::cout << "project: placeholder — migrate existing sources in " << cwd
            << " to package.xml + per-target subdirs with target.xml.\n";
  return 0;
}

}  // namespace up
