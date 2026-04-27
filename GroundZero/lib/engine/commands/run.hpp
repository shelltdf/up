#pragma once

#include <filesystem>
#include <string>

namespace gz {

int cmd_run(const std::filesystem::path& install_dir, const std::string& target_name);

}  // namespace gz
