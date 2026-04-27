#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace up {

int cmd_reverse(const std::filesystem::path& cwd, const std::vector<std::string>& args);

}  // namespace up
