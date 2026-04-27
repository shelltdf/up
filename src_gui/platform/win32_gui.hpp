#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace up::gui::platform::win32 {

// Win32 主消息循环与主窗（实现见 platform/win32_gui.cpp）。
// GTK/macOS 与 Win32 均编译 core/gui_persist.cpp、core/gui_shell_actions.cpp（UTF-8 调 up 与设置文件）；主窗与消息循环仍在本文件及各平台 platform/*。
// 同目标下已拆出的编译单元（仍属 Win32 外壳，由 CMake 一并链接）：
//   platform/win32_paths.* — 模块目录、settings 路径、路径正斜杠规范化；
//   platform/win32_encoding.* — UTF-8 / UTF-16 / 本地代码页与宽字符互转（日志与子进程输出）；
//   platform/win32_fsutil.* — 环境变量、cmd 路径、PATH 拆分、路径绝对化规范化；
//   platform/win32_text_util.* — 本地时间戳、按字节读文件、极简 XML 属性、ASCII 小写；
//   platform/win32_window_placement.* — 显示器工作区与「逻辑尺寸」窗口居中；
//   platform/win32_modal_file_dialog_center.* — IFileOpenDialog / GetSaveFileName 的 WH_CBT 居中。
int run(HINSTANCE instance, int show_cmd);

}  // namespace up::gui::platform::win32
