#pragma once

#include <filesystem>
#include <string>

namespace up {

int cmd_run(const std::filesystem::path& cwd, const std::string& target_name);

}  // namespace up
