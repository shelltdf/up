#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstring>
#include <string>

#include "platform/win32/win32_clipboard_quote.hpp"

namespace gz::gui::platform::win32 {

bool CopyTextToClipboard(HWND owner, const std::wstring& text) {
  if (!OpenClipboard(owner))
    return false;
  EmptyClipboard();
  const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
  HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
  if (!h) {
    CloseClipboard();
    return false;
  }
  void* p = GlobalLock(h);
  if (!p) {
    GlobalFree(h);
    CloseClipboard();
    return false;
  }
  memcpy(p, text.c_str(), bytes);
  GlobalUnlock(h);
  if (!SetClipboardData(CF_UNICODETEXT, h)) {
    GlobalFree(h);
    CloseClipboard();
    return false;
  }
  CloseClipboard();
  return true;
}

std::wstring QuoteWinArg(const std::wstring& s) {
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

}  // namespace gz::gui::platform::win32
