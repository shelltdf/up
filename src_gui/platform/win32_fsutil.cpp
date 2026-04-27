#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <filesystem>
#include <string>
#include <vector>

#include "platform/win32_fsutil.hpp"
#include "platform/win32_paths.hpp"

namespace {

void trim_w(std::wstring& s) {
  while (!s.empty() && (s.front() == L' ' || s.front() == L'\t'))
    s.erase(0, 1);
  while (!s.empty() && (s.back() == L' ' || s.back() == L'\t'))
    s.pop_back();
}

}  // namespace

namespace up::gui::platform::win32 {

std::wstring GetEnvVarW(const wchar_t* name) {
  wchar_t buf[32767]{};
  const DWORD n = GetEnvironmentVariableW(name, buf, static_cast<DWORD>(std::size(buf)));
  if (n == 0 || n >= std::size(buf))
    return {};
  return std::wstring(buf, buf + n);
}

std::wstring CmdExePath() {
  std::wstring comspec = GetEnvVarW(L"COMSPEC");
  if (!comspec.empty()) {
    std::error_code ec;
    if (std::filesystem::exists(std::filesystem::path(comspec), ec))
      return PathToPortableSlashes(std::move(comspec));
  }
  std::wstring sysroot = GetEnvVarW(L"SystemRoot");
  if (!sysroot.empty()) {
    std::filesystem::path p = std::filesystem::path(sysroot) / "System32" / "cmd.exe";
    std::error_code ec;
    if (std::filesystem::exists(p, ec))
      return p.lexically_normal().generic_wstring();
  }
  return L"cmd.exe";
}

std::vector<std::wstring> SplitPathList(const std::wstring& s) {
  std::vector<std::wstring> out;
  size_t off = 0;
  while (off <= s.size()) {
    const size_t p = s.find(L';', off);
    std::wstring part = s.substr(off, p == std::wstring::npos ? std::wstring::npos : (p - off));
    trim_w(part);
    if (!part.empty())
      out.push_back(part);
    if (p == std::wstring::npos)
      break;
    off = p + 1;
  }
  return out;
}

std::wstring NormalizePath(const std::filesystem::path& p) {
  std::error_code ec;
  const auto abs = std::filesystem::absolute(p, ec);
  return (ec ? p : abs).lexically_normal().generic_wstring();
}

}  // namespace up::gui::platform::win32
