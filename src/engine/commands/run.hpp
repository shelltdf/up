#pragma once

#include <filesystem>
#include <string>

namespace up {

int cmd_run(const std::filesystem::path& install_dir, const std::string& target_name);

}  // namespace up
