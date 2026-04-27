#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "platform/win32/win32_modal_file_dialog_center.hpp"
#include "platform/win32/win32_window_placement.hpp"

namespace {

void CenterHWNDOnMonitorOf(HWND wnd, HWND ref_for_work_area) {
  if (!wnd)
    return;
  RECT wr{};
  if (!GetWindowRect(wnd, &wr))
    return;
  const int ww = wr.right - wr.left;
  const int hh = wr.bottom - wr.top;
  const RECT work =
      gz::gui::platform::win32::WorkAreaForMonitorContainingWindow(ref_for_work_area ? ref_for_work_area : wnd);
  int x = work.left + ((work.right - work.left) - ww) / 2;
  int y = work.top + ((work.bottom - work.top) - hh) / 2;
  if (x + ww > work.right)
    x = work.right - ww;
  if (y + hh > work.bottom)
    y = work.bottom - hh;
  if (x < work.left)
    x = work.left;
  if (y < work.top)
    y = work.top;
  SetWindowPos(wnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

HHOOK g_modal_file_dlg_cbt = nullptr;
HWND g_modal_file_dlg_owner = nullptr;
bool g_modal_file_dlg_centered = false;

LRESULT CALLBACK ModalFileDialogCenterCbtProc(int nCode, WPARAM wParam, LPARAM lParam) {
  (void)lParam;
  if (nCode != HCBT_ACTIVATE || !g_modal_file_dlg_owner || g_modal_file_dlg_centered)
    return CallNextHookEx(g_modal_file_dlg_cbt, nCode, wParam, lParam);
  const HWND target = reinterpret_cast<HWND>(wParam);
  if (!target)
    return CallNextHookEx(g_modal_file_dlg_cbt, nCode, wParam, lParam);
  const HWND root = GetAncestor(target, GA_ROOT);
  if (!root || root == g_modal_file_dlg_owner)
    return CallNextHookEx(g_modal_file_dlg_cbt, nCode, wParam, lParam);
  RECT wr{};
  if (!GetWindowRect(root, &wr))
    return CallNextHookEx(g_modal_file_dlg_cbt, nCode, wParam, lParam);
  const int ww = wr.right - wr.left;
  const int hh = wr.bottom - wr.top;
  if (ww < 220 || hh < 160)
    return CallNextHookEx(g_modal_file_dlg_cbt, nCode, wParam, lParam);

  const HWND dlg_owner = GetWindow(root, GW_OWNER);
  const HWND dlg_parent = GetParent(root);
  wchar_t cls[300]{};
  (void)GetClassNameW(root, cls, static_cast<int>(sizeof(cls) / sizeof(cls[0])));
  const bool owner_ok =
      g_modal_file_dlg_owner && (dlg_owner == g_modal_file_dlg_owner || dlg_parent == g_modal_file_dlg_owner);
  const bool class_ok =
      wcscmp(cls, L"#32770") == 0 || _wcsicmp(cls, L"CabinetWClass") == 0 || _wcsicmp(cls, L"ExploreWClass") == 0 ||
      _wcsicmp(cls, L"Windows.UI.Core.CoreWindow") == 0 || wcsstr(cls, L"Xaml_WindowedPopupClass") != nullptr;
  const DWORD wstyle = static_cast<DWORD>(GetWindowLongPtrW(root, GWL_STYLE));
  const DWORD wtp_root = GetWindowThreadProcessId(root, nullptr);
  const DWORD wtp_owner = g_modal_file_dlg_owner ? GetWindowThreadProcessId(g_modal_file_dlg_owner, nullptr) : 0;
  const bool same_thread_as_owner = wtp_root != 0 && wtp_root == wtp_owner;
  const RECT work_ref = gz::gui::platform::win32::WorkAreaForMonitorContainingWindow(g_modal_file_dlg_owner);
  const int work_w = work_ref.right - work_ref.left;
  const int work_h = work_ref.bottom - work_ref.top;
  const bool dialog_like =
      same_thread_as_owner && (wstyle & WS_CHILD) == 0 && (wstyle & WS_POPUP) != 0 && ww >= 360 && hh >= 240 &&
      ww <= (work_w > 0 ? work_w + 80 : 4096) && hh <= (work_h > 0 ? work_h + 80 : 4096);
  if (owner_ok || class_ok || dialog_like) {
    CenterHWNDOnMonitorOf(root, g_modal_file_dlg_owner);
    g_modal_file_dlg_centered = true;
  }
  return CallNextHookEx(g_modal_file_dlg_cbt, nCode, wParam, lParam);
}

}  // namespace

namespace gz::gui::platform::win32 {

void ModalFileDialogCenterBegin(HWND owner) {
  g_modal_file_dlg_owner = owner;
  g_modal_file_dlg_centered = false;
  g_modal_file_dlg_cbt = SetWindowsHookExW(WH_CBT, ModalFileDialogCenterCbtProc, nullptr, GetCurrentThreadId());
}

void ModalFileDialogCenterEnd() {
  if (g_modal_file_dlg_cbt) {
    UnhookWindowsHookEx(g_modal_file_dlg_cbt);
    g_modal_file_dlg_cbt = nullptr;
  }
  g_modal_file_dlg_owner = nullptr;
  g_modal_file_dlg_centered = false;
}

}  // namespace gz::gui::platform::win32
