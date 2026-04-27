#include "platform/win32/win32_gui.hpp"

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, PWSTR, int show) {
  return gz::gui::platform::win32::run(hi, show);
}
