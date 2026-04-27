#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "platform/win32/win32_text_util.hpp"

namespace up::gui::platform::win32 {

std::wstring NowTimeStamp() {
  SYSTEMTIME st{};
  GetLocalTime(&st);
  wchar_t buf[64]{};
  swprintf_s(buf, L"%04u-%02u-%02u %02u:%02u:%02u", static_cast<unsigned>(st.wYear), static_cast<unsigned>(st.wMonth),
             static_cast<unsigned>(st.wDay), static_cast<unsigned>(st.wHour), static_cast<unsigned>(st.wMinute),
             static_cast<unsigned>(st.wSecond));
  return buf;
}

std::string ReadTextFileUtf8BestEffort(const std::filesystem::path& p) {
  std::ifstream in(p, std::ios::binary);
  if (!in)
    return {};
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::string ExtractXmlAttr(const std::string& xml, const char* attr) {
  const std::string key = std::string(attr) + "=\"";
  const auto k = xml.find(key);
  if (k == std::string::npos)
    return {};
  const auto b = k + key.size();
  const auto e = xml.find('"', b);
  if (e == std::string::npos || e <= b)
    return {};
  return xml.substr(b, e - b);
}

std::string ToLowerAscii(std::string s) {
  for (char& c : s) {
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c - 'A' + 'a');
  }
  return s;
}

}  // namespace up::gui::platform::win32
