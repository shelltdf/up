#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace up::gui::platform::win32 {
int run(HINSTANCE instance, int show_cmd);
}  // namespace up::gui::platform::win32

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, PWSTR, int show) {
  return up::gui::platform::win32::run(hi, show);
}
