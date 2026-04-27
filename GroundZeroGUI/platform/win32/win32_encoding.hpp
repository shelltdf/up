#pragma once

#include <string>
#include <string_view>

namespace gz::gui::platform::win32 {

std::string WideToUtf8(std::wstring_view ws);
std::wstring Utf8ToWide(std::string_view utf8);

}  // namespace gz::gui::platform::win32
