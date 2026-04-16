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

void trim_trailing_dir_seps(std::wstring& s) {
  while (!s.empty() && (s.back() == L'\\' || s.back() == L'/'))
    s.pop_back();
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

void append_scan_args(std::wstring& args_no_exe, const std::vector<std::wstring>& scan_dirs, const std::wstring& cwd) {
  std::wstring cwd_norm = cwd;
  trim_trailing_dir_seps(cwd_norm);
  for (const auto& d : scan_dirs) {
    if (d.empty())
      continue;
    std::wstring dn = d;
    trim_trailing_dir_seps(dn);
    // up 已在 cwd 下扫描，不必再传与 cwd 相同的 --scan
    if (!cwd_norm.empty() && path_equal_ci(dn, cwd_norm))
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
