#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string>

namespace up::gui::platform::win32 {

// 将 UTF-16 文本放入剪贴板（CF_UNICODETEXT）；失败返回 false。
bool CopyTextToClipboard(HWND owner, const std::wstring& text);

// CreateProcess 风格命令行参数转义（含空格/制表/引号时加引号并转义内部引号）。
std::wstring QuoteWinArg(const std::wstring& s);

}  // namespace up::gui::platform::win32
