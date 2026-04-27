#pragma once

#include <gtk/gtk.h>

namespace up::gui::platform::gtk {

void gtk_application_activate(GtkApplication* app, gpointer user_data);

}  // namespace up::gui::platform::gtk
