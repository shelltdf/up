#pragma once

#include <filesystem>
#include <string>

namespace up {

int cmd_build(const std::filesystem::path& cwd);
int cmd_run(const std::filesystem::path& cwd, const std::string& target_name);
int cmd_test(const std::filesystem::path& cwd, const std::string& test_name = "");
int cmd_pack(const std::filesystem::path& cwd);
int cmd_project(const std::filesystem::path& cwd);

}  // namespace up
