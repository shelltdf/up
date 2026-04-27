#pragma once

#include <filesystem>
#include <string>

namespace gz {

int cmd_build(const std::filesystem::path& cwd, const std::filesystem::path& build_dir);

}  // namespace gz
