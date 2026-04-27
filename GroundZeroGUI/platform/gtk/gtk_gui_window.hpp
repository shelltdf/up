#pragma once

#include <gtk/gtk.h>

namespace gz::gui::platform::gtk {

void gtk_application_activate(GtkApplication* app, gpointer user_data);

}  // namespace gz::gui::platform::gtk
