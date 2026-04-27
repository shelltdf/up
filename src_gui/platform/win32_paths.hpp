#pragma once

#include <filesystem>
#include <string>

namespace up::gui::platform::win32 {

// 当前模块（up-gui.exe）所在目录；尾无分隔符，分隔符统一为正斜杠。
std::wstring DirOfModule();

// 与 exe 同目录的 up_gui_settings.txt。
std::filesystem::path GuiSettingsPath();

// 路径统一为 POSIX 风格「/」分隔（generic 形式），便于与 Linux 习惯一致、配置文件易读；Windows API 多数仍接受此种路径。
std::wstring PathToPortableSlashes(std::wstring w);

}  // namespace up::gui::platform::win32
