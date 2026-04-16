#pragma once

#include <filesystem>
#include <string>

namespace up {

int cmd_test(const std::filesystem::path& cwd, const std::string& test_name = "");

}  // namespace up
