#pragma once

#include <filesystem>
#include <string>

namespace up {

int cmd_build(const std::filesystem::path& cwd, const std::filesystem::path& build_dir);

}  // namespace up
