#pragma once

#include <filesystem>
#include <vector>

namespace up {

int cmd_pack(const std::filesystem::path& cwd, const std::vector<std::filesystem::path>& install_dirs);

}  // namespace up
