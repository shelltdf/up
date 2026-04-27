#pragma once

#include <windows.h>

namespace up::gui::platform::win32 {

// 包含 ref_hwnd 的显示器工作区；ref 为空时用光标所在显示器（与主窗首次居中一致）。
RECT WorkAreaForMonitorContainingWindow(HWND ref_hwnd);

// 在 reference 所在显示器工作区内，将 outer_w×outer_h 的「外接矩形」居中，输出左上角屏幕坐标。
void CenterOuterWindowOnScreen(HWND reference, int outer_w, int outer_h, int* out_x, int* out_y);

}  // namespace up::gui::platform::win32
