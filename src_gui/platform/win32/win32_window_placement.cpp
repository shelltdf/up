#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "platform/win32/win32_window_placement.hpp"

namespace up::gui::platform::win32 {

RECT WorkAreaForMonitorContainingWindow(HWND ref) {
  MONITORINFO mi{sizeof(mi)};
  RECT work{};
  HMONITOR mon = nullptr;
  if (ref)
    mon = MonitorFromWindow(ref, MONITOR_DEFAULTTONEAREST);
  else {
    POINT pt{};
    if (GetCursorPos(&pt))
      mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    if (!mon)
      mon = MonitorFromWindow(GetDesktopWindow(), MONITOR_DEFAULTTOPRIMARY);
  }
  if (!mon) {
    POINT pt{};
    GetCursorPos(&pt);
    mon = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
  }
  if (mon && GetMonitorInfoW(mon, &mi))
    return mi.rcWork;
  SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
  return work;
}

void CenterOuterWindowOnScreen(HWND reference, int outer_w, int outer_h, int* out_x, int* out_y) {
  const RECT work = WorkAreaForMonitorContainingWindow(reference);
  const int wa_w = work.right - work.left;
  const int wa_h = work.bottom - work.top;
  int x = work.left + (wa_w - outer_w) / 2;
  int y = work.top + (wa_h - outer_h) / 2;
  if (x + outer_w > work.right)
    x = work.right - outer_w;
  if (y + outer_h > work.bottom)
    y = work.bottom - outer_h;
  if (x < work.left)
    x = work.left;
  if (y < work.top)
    y = work.top;
  *out_x = x;
  *out_y = y;
}

}  // namespace up::gui::platform::win32
