#pragma once

#include <filesystem>
#include <string>

namespace up {

// Runs test-like executables from <install_dir>/bin (same discovery as the former ninja path).
int cmd_test(const std::filesystem::path& install_dir, const std::string& test_name = "");

}  // namespace up
