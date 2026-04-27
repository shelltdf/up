// Linux GTK3 入口：主窗与回调拆至 gtk_gui_window.cpp、gtk_gui_handlers.cpp、gtk_gui_log.cpp、gtk_gui_state.cpp。

#include "platform/gtk_gui.hpp"
#include "platform/gtk_gui_window.hpp"

#include <gtk/gtk.h>

namespace up::gui::platform::gtk {

int run(int argc, char** argv) {
  GtkApplication* app = gtk_application_new("dev.up.up_gui", G_APPLICATION_FLAGS_NONE);
  g_signal_connect(app, "activate", G_CALLBACK(gtk_application_activate), nullptr);
  const int st = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return st;
}

}  // namespace up::gui::platform::gtk
