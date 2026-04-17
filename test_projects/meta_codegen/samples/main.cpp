#include "widget.h"

#include <iostream>

void WidgetWindow::show() { std::cout << "[widget] WidgetWindow::show()\n"; }

int main() {
  WidgetWindow window;
  window.show();
  return 0;
}
