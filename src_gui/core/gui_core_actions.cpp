#include "gui_core_actions.hpp"

#include <algorithm>

namespace up::gui::core {

namespace {

bool path_equal_ci(const std::wstring& a, const std::wstring& b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i) {
    wchar_t ca = a[i];
    wchar_t cb = b[i];
    if (ca >= L'A' && ca <= L'Z')
      ca = static_cast<wchar_t>(ca - L'A' + L'a');
    if (cb >= L'A' && cb <= L'Z')
      cb = static_cast<wchar_t>(cb - L'A' + L'a');
    if (ca != cb)
      return false;
  }
  return true;
}

}  // namespace

std::wstring quote_arg(const std::wstring& s) {
  if (s.empty())
    return L"\"\"";
  if (s.find_first_of(L" \t\"") == std::wstring::npos)
    return s;
  std::wstring out = L"\"";
  for (wchar_t c : s) {
    if (c == L'"')
      out += L'\\';
    out += c;
  }
  out += L"\"";
  return out;
}

std::vector<std::wstring> merge_scan_dirs_with_cwd(const std::vector<std::wstring>& scan_dirs, const std::wstring& cwd) {
  std::vector<std::wstring> out = scan_dirs;
  if (cwd.empty())
    return out;
  bool has_cwd = false;
  for (const auto& d : out) {
    if (path_equal_ci(d, cwd)) {
      has_cwd = true;
      break;
    }
  }
  if (!has_cwd)
    out.push_back(cwd);
  return out;
}

void append_scan_args(std::wstring& args_no_exe, const std::vector<std::wstring>& scan_dirs, const std::wstring& cwd) {
  const auto merged = merge_scan_dirs_with_cwd(scan_dirs, cwd);
  for (const auto& d : merged) {
    if (d.empty())
      continue;
    args_no_exe += L" --scan ";
    if (d.find(L' ') != std::wstring::npos)
      args_no_exe += L"\"" + d + L"\"";
    else
      args_no_exe += d;
  }
}

UpLaunchPlan build_launch_plan(const std::wstring& up_exe,
                               const std::wstring& args_no_exe,
                               const std::wstring& extra_args,
                               const std::wstring& vcvars_path,
                               const std::wstring& cmd_exe_path) {
  UpLaunchPlan p{};
  const std::wstring base = quote_arg(up_exe) + L" " + args_no_exe + (extra_args.empty() ? L"" : (L" " + extra_args));
  p.display_command = base;
  p.process_path = up_exe;
  p.command_line = base;

  if (!vcvars_path.empty() && !cmd_exe_path.empty()) {
    p.use_vcvars = true;
    p.process_path = cmd_exe_path;
    p.command_line = L"/d /c \"\"" + vcvars_path + L"\" >nul && " + base + L"\"";
    p.display_command = p.command_line;
  }
  return p;
}

}  // namespace up::gui::core
