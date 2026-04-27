#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace up::gui::platform::win32 {

// Win32 主消息循环与主窗（实现见 platform/win32_gui.cpp）。
// 同目标下已拆出的编译单元（仍属 Win32 外壳，由 CMake 一并链接）：
//   platform/win32_paths.* — 模块目录、settings 路径、路径正斜杠规范化；
//   platform/win32_window_placement.* — 显示器工作区与「逻辑尺寸」窗口居中；
//   platform/win32_modal_file_dialog_center.* — IFileOpenDialog / GetSaveFileName 的 WH_CBT 居中。
int run(HINSTANCE instance, int show_cmd);

}  // namespace up::gui::platform::win32
