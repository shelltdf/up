#include "platform/gtk/gtk_gui_log.hpp"
#include "platform/gtk/gtk_gui_state.hpp"

#include "gui_persist.hpp"

#include <gtk/gtk.h>

namespace up::gui::platform::gtk {

namespace {

struct LogMsg {
  std::string text;
};

gboolean on_log_idle(gpointer data) {
  auto* m = static_cast<LogMsg*>(data);
  if (gs().log_view && m) {
    GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gs().log_view));
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buf, &end);
    gtk_text_buffer_insert(buf, &end, m->text.c_str(), static_cast<int>(m->text.size()));
  }
  delete m;
  return FALSE;
}

}  // namespace

std::string get_entry(GtkWidget* e) {
  return e ? gtk_entry_get_text(GTK_ENTRY(e)) : std::string{};
}

void set_status(const char* s) {
  if (gs().status)
    gtk_label_set_text(GTK_LABEL(gs().status), s);
}

void log_line(const std::string& s) {
  g_idle_add(on_log_idle, new LogMsg{s + "\n"});
}

std::vector<std::string> collect_scans() {
  std::vector<std::string> out;
  if (!gs().store_scan)
    return out;
  GtkTreeIter it{};
  if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(gs().store_scan), &it))
    return out;
  do {
    gchar* path = nullptr;
    gtk_tree_model_get(GTK_TREE_MODEL(gs().store_scan), &it, 0, &path, -1);
    if (path && path[0])
      out.emplace_back(path);
    g_free(path);
  } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(gs().store_scan), &it));
  return out;
}

void save_ui_to_persist() {
  gs().persist.browse_cwd = up::gui::persist::path_to_portable_utf8(get_entry(gs().entry_cwd));
  up::gui::persist::save_settings(gs().settings_file, gs().persist);
}

}  // namespace up::gui::platform::gtk
