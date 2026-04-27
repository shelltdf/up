#include "platform/cocoa/cocoa_gui_state.hpp"

namespace gz::gui::platform::cocoa {

static CocoaState g;

CocoaState& cs() {
  return g;
}

}  // namespace gz::gui::platform::cocoa
