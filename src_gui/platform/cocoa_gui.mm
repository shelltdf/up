#include "../core/gui_core_actions.hpp"

#include <iostream>

namespace up::gui::platform::cocoa {

int run() {
  // Stage-1 shell: platform windowing is intentionally thin and delegates behavior to core.
  std::cout << "up-gui cocoa shell is enabled. Core bridge ready.\n";
  return 0;
}

}  // namespace up::gui::platform::cocoa
