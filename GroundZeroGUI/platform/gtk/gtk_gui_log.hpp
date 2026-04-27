#pragma once

#include <gtk/gtk.h>

#include <string>
#include <vector>

namespace gz::gui::platform::gtk {

void log_line(const std::string& s);
void set_status(const char* s);
std::string get_entry(GtkWidget* e);
std::vector<std::string> collect_scans();
void save_ui_to_persist();

}  // namespace gz::gui::platform::gtk
