#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <string>
#include <string_view>

#include "platform/win32_encoding.hpp"

namespace up::gui::platform::win32 {

std::string WideToUtf8(std::wstring_view ws) {
  if (ws.empty())
    return {};
  const int n = WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
  if (n <= 0) {
    std::string fallback;
    fallback.reserve(ws.size());
    for (wchar_t c : ws)
      fallback.push_back((c >= 0 && c <= 127) ? static_cast<char>(c) : '?');
    return fallback;
  }
  std::string out(static_cast<size_t>(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), out.data(), n, nullptr, nullptr);
  return out;
}

std::wstring Utf8ToWide(std::string_view utf8) {
  if (utf8.empty())
    return {};
  auto decode_utf16_bytes = [](std::string_view bytes, bool little_endian) -> std::wstring {
    if (bytes.size() < 2)
      return {};
    std::wstring out;
    out.reserve(bytes.size() / 2);
    for (size_t i = 0; i + 1 < bytes.size(); i += 2) {
      const unsigned char b0 = static_cast<unsigned char>(bytes[i]);
      const unsigned char b1 = static_cast<unsigned char>(bytes[i + 1]);
      const unsigned short code = little_endian ? static_cast<unsigned short>(b0 | (b1 << 8))
                                                 : static_cast<unsigned short>((b0 << 8) | b1);
      out.push_back(static_cast<wchar_t>(code));
    }
    return out;
  };
  auto looks_utf16_stream = [](std::string_view bytes, bool little_endian) -> bool {
    if (bytes.size() < 6)
      return false;
    size_t zeros = 0;
    size_t checked = 0;
    const size_t limit = (std::min)(bytes.size(), static_cast<size_t>(128));
    for (size_t i = little_endian ? 1 : 0; i < limit; i += 2) {
      ++checked;
      if (bytes[i] == '\0')
        ++zeros;
    }
    return checked >= 8 && zeros * 100 / checked >= 60;
  };
  const auto* raw = reinterpret_cast<const unsigned char*>(utf8.data());
  if (utf8.size() >= 2) {
    if (raw[0] == 0xFF && raw[1] == 0xFE)
      return decode_utf16_bytes(utf8.substr(2), true);
    if (raw[0] == 0xFE && raw[1] == 0xFF)
      return decode_utf16_bytes(utf8.substr(2), false);
  }
  if (looks_utf16_stream(utf8, true))
    return decode_utf16_bytes(utf8, true);
  if (looks_utf16_stream(utf8, false))
    return decode_utf16_bytes(utf8, false);
  auto looks_mojibake_zh = [](const std::wstring& s) -> bool {
    static const wchar_t* kBadMarks[] = {
        L"閫傜敤浜", L"鐗堟湰", L"姝ｅ湪", L"鍒涘缓", L"缂栬瘧", L"鎿嶄綔",
    };
    for (const auto* m : kBadMarks) {
      if (s.find(m) != std::wstring::npos)
        return true;
    }
    return false;
  };
  auto convert = [&](unsigned cp, DWORD flags = 0) -> std::wstring {
    const int n = MultiByteToWideChar(cp, flags, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (n <= 0)
      return {};
    std::wstring w(static_cast<size_t>(n), L'\0');
    if (MultiByteToWideChar(cp, flags, utf8.data(), static_cast<int>(utf8.size()), w.data(), n) <= 0)
      return {};
    return w;
  };
  std::wstring w = convert(CP_UTF8, MB_ERR_INVALID_CHARS);
  if (!w.empty())
    return w;
  const std::wstring utf8_lenient = convert(CP_UTF8, 0);
  w = convert(CP_ACP);
  if (!w.empty() && !utf8_lenient.empty() && looks_mojibake_zh(w))
    return utf8_lenient;
  if (!w.empty())
    return w;
  w = convert(CP_OEMCP);
  if (!w.empty() && !utf8_lenient.empty() && looks_mojibake_zh(w))
    return utf8_lenient;
  if (!w.empty())
    return w;
  return utf8_lenient;
}

}  // namespace up::gui::platform::win32
