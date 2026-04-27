#pragma once

#include <string>
#include <vector>

namespace gz::gui::core {

struct GzLaunchPlan {
  std::wstring process_path;
  std::wstring command_line;
  std::wstring display_command;
  bool use_vcvars = false;
};

std::wstring quote_arg(const std::wstring& s);
void append_scan_args(std::wstring& args_no_exe, const std::vector<std::wstring>& scan_dirs, const std::wstring& cwd);

GzLaunchPlan build_launch_plan(const std::wstring& gz_exe,
                               const std::wstring& args_no_exe,
                               const std::wstring& extra_args,
                               const std::wstring& vcvars_path,
                               const std::wstring& cmd_exe_path);

}  // namespace gz::gui::core
