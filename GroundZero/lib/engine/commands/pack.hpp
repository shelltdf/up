#pragma once

#include <filesystem>
#include <vector>

namespace gz {

int cmd_pack(const std::filesystem::path& cwd, const std::vector<std::filesystem::path>& install_dirs);

}  // namespace gz
