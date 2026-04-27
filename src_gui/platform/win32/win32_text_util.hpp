#pragma once

#include <filesystem>
#include <string>

namespace up::gui::platform::win32 {

// 本地时间 `YYYY-MM-DD HH:MM:SS`，用于日志行前缀。
std::wstring NowTimeStamp();

// 整文件按字节读入（UTF-8 由调用方解释；失败返回空串）。
std::string ReadTextFileUtf8BestEffort(const std::filesystem::path& p);

// 极简 `attr="..."` 扫描（供 tests.xml 等小块标记用）。
std::string ExtractXmlAttr(const std::string& xml, const char* attr);

std::string ToLowerAscii(std::string s);

}  // namespace up::gui::platform::win32
