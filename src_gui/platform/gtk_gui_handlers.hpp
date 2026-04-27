#pragma once

#include <gtk/gtk.h>

#include <string>

namespace up::gui::platform::gtk {

void on_browse_cwd(GtkWidget*, gpointer);
void on_scan_add(GtkWidget*, gpointer);
void on_scan_remove(GtkWidget*, gpointer);
void run_up_line_async(const std::string& args_no_exe);
#if UP_ENABLE_REVERSE
void on_reverse(GtkWidget*, gpointer);
#endif
void on_configure(GtkWidget*, gpointer);
void on_build(GtkWidget*, gpointer);
void on_run(GtkWidget*, gpointer);
void on_test(GtkWidget*, gpointer);
void on_pack(GtkWidget*, gpointer);
void on_window_destroy(GtkWidget*, gpointer);

}  // namespace up::gui::platform::gtk
