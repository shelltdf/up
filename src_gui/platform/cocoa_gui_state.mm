#include "platform/cocoa_gui_state.hpp"

namespace up::gui::platform::cocoa {

static CocoaState g;

CocoaState& cs() {
  return g;
}

}  // namespace up::gui::platform::cocoa
