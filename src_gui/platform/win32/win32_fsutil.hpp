#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace up::gui::platform::win32 {

// 读取进程环境变量（空表示未设置或过长）。
std::wstring GetEnvVarW(const wchar_t* name);

// 用于启动子进程的 cmd.exe 路径（COMSPEC / SystemRoot 回退）。
std::wstring CmdExePath();

// 按 `;` 拆分 PATH 风格列表，去掉首尾空白并跳过空段。
std::vector<std::wstring> SplitPathList(const std::wstring& s);

// 绝对化后 generic 规范化路径字符串（失败则退回原 path 的规范化形式）。
std::wstring NormalizePath(const std::filesystem::path& p);

}  // namespace up::gui::platform::win32
