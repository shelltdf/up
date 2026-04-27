#include "platform/gtk/gtk_gui_state.hpp"

namespace gz::gui::platform::gtk {

static GuiState g;

GuiState& gs() {
  return g;
}

}  // namespace gz::gui::platform::gtk
