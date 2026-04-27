#include "platform/gtk/gtk_gui_state.hpp"

namespace up::gui::platform::gtk {

static GuiState g;

GuiState& gs() {
  return g;
}

}  // namespace up::gui::platform::gtk
