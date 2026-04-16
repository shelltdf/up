#pragma once

#include <string>
#include <vector>

namespace up::gui::core {

struct UpLaunchPlan {
  std::wstring process_path;
  std::wstring command_line;
  std::wstring display_command;
  bool use_vcvars = false;
};

std::wstring quote_arg(const std::wstring& s);
void append_scan_args(std::wstring& args_no_exe, const std::vector<std::wstring>& scan_dirs, const std::wstring& cwd);

UpLaunchPlan build_launch_plan(const std::wstring& up_exe,
                               const std::wstring& args_no_exe,
                               const std::wstring& extra_args,
                               const std::wstring& vcvars_path,
                               const std::wstring& cmd_exe_path);

}  // namespace up::gui::core
