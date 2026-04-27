#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <filesystem>
#include <string>

#include "platform/win32/win32_paths.hpp"

namespace {

void trim_w(std::wstring& s) {
  while (!s.empty() && (s.front() == L' ' || s.front() == L'\t'))
    s.erase(0, 1);
  while (!s.empty() && (s.back() == L' ' || s.back() == L'\t'))
    s.pop_back();
}

}  // namespace

namespace up::gui::platform::win32 {

std::wstring PathToPortableSlashes(std::wstring w) {
  trim_w(w);
  if (w.empty())
    return w;
  std::filesystem::path p(w);
  p = p.lexically_normal();
  return p.generic_wstring();
}

std::wstring DirOfModule() {
  wchar_t buf[MAX_PATH]{};
  if (!GetModuleFileNameW(nullptr, buf, MAX_PATH))
    return {};
  std::wstring p(buf);
  const auto pos = p.find_last_of(L"\\/");
  if (pos == std::wstring::npos)
    return {};
  return PathToPortableSlashes(p.substr(0, pos));
}

std::filesystem::path GuiSettingsPath() {
  return std::filesystem::path(DirOfModule()) / "up_gui_settings.txt";
}

}  // namespace up::gui::platform::win32
