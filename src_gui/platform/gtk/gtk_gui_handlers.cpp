#include "platform/gtk/gtk_gui_handlers.hpp"

#include "platform/gtk/gtk_gui_log.hpp"
#include "platform/gtk/gtk_gui_state.hpp"

#include "gui_persist.hpp"
#include "gui_shell_actions.hpp"

#include <gtk/gtk.h>

namespace up::gui::platform::gtk {

namespace persist = up::gui::persist;
namespace shell = up::gui::shell;

void on_browse_cwd(GtkWidget*, gpointer) {
  GtkWidget* dlg = gtk_file_chooser_dialog_new("Working directory (CWD)", GTK_WINDOW(gs().win), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                               "_Cancel", GTK_RESPONSE_CANCEL, "_Select", GTK_RESPONSE_ACCEPT, nullptr);
  gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER_ALWAYS);
  std::string init = get_entry(gs().entry_cwd);
  if (init.empty())
    init = gs().persist.browse_cwd;
  if (!init.empty())
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), init.c_str());
  if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
    char* p = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
    if (p) {
      const std::string portable = persist::path_to_portable_utf8(p);
      gtk_entry_set_text(GTK_ENTRY(gs().entry_cwd), portable.c_str());
      gs().persist.browse_cwd = portable;
      g_free(p);
    }
  }
  gtk_widget_destroy(dlg);
}

void on_scan_add(GtkWidget*, gpointer) {
  GtkWidget* dlg = gtk_file_chooser_dialog_new("Add scan root", GTK_WINDOW(gs().win), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                               "_Cancel", GTK_RESPONSE_CANCEL, "_Select", GTK_RESPONSE_ACCEPT, nullptr);
  gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER_ALWAYS);
  if (!gs().persist.browse_scan.empty())
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), gs().persist.browse_scan.c_str());
  if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
    char* p = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
    if (p) {
      gs().persist.browse_scan = persist::path_to_portable_utf8(p);
      GtkTreeIter it{};
      gtk_list_store_append(gs().store_scan, &it);
      gtk_list_store_set(gs().store_scan, &it, 0, gs().persist.browse_scan.c_str(), -1);
      g_free(p);
    }
  }
  gtk_widget_destroy(dlg);
}

void on_scan_remove(GtkWidget*, gpointer) {
  GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(gs().scan_view));
  GtkTreeIter it{};
  if (gtk_tree_selection_get_selected(sel, nullptr, &it))
    gtk_list_store_remove(gs().store_scan, &it);
}

void run_up_line_async(const std::string& args_no_exe) {
  const std::string cwd = persist::path_to_portable_utf8(get_entry(gs().entry_cwd));
  const auto maybe_cwd = shell::try_acquire_run_context(gs().up_exe, cwd, gs().busy, [](const std::string& s) { log_line(s); });
  if (!maybe_cwd)
    return;
  const std::string extra = get_entry(gs().entry_extra);
  shell::run_up_command_in_detached_thread(gs().up_exe, *maybe_cwd, args_no_exe, extra, gs().busy,
                                           [](const std::string& s) { log_line(s); },
                                           []() {
                                             g_idle_add(
                                                 +[](gpointer) -> gboolean {
                                                   set_status("Ready");
                                                   return FALSE;
                                                 },
                                                 nullptr);
                                           });
  set_status("Running up…");
}

void on_configure(GtkWidget*, gpointer) {
  const std::string cwd = persist::path_to_portable_utf8(get_entry(gs().entry_cwd));
  const std::string cfg = shell::build_configure_args_line(gs().up_exe, cwd, get_entry(gs().entry_build), collect_scans(),
                                                            gs().persist, [](const std::string& s) { log_line(s); });
  if (cfg.empty())
    return;
  run_up_line_async(cfg);
}

void on_build(GtkWidget*, gpointer) {
  std::string args = "build";
  const std::string leaf = persist::intermediate_leaf_from_build_dir_field(get_entry(gs().entry_build));
  if (leaf.empty()) {
    log_line("[error] Set Build Dir (or leaf name) first.");
    return;
  }
  args += " --build-dir-name ";
  args += persist::shell_single_quote(leaf);
  run_up_line_async(args);
}

void on_run(GtkWidget*, gpointer) {
  std::string tgt = get_entry(gs().entry_run);
  shell::trim_ascii_inplace(tgt);
  if (tgt.empty()) {
    log_line("[error] Run target empty.");
    return;
  }
  std::string err;
  std::string args = "run";
  if (!shell::append_install_dir_flag(args, get_entry(gs().entry_install), err)) {
    log_line("[error] " + err);
    return;
  }
  args += " ";
  args += persist::shell_single_quote(tgt);
  run_up_line_async(args);
}

void on_test(GtkWidget*, gpointer) {
  std::string tgt = get_entry(gs().entry_test);
  shell::trim_ascii_inplace(tgt);
  std::string err;
  std::string args = "test";
  if (!shell::append_install_dir_flag(args, get_entry(gs().entry_install), err)) {
    log_line("[error] " + err);
    return;
  }
  if (!tgt.empty()) {
    args += " ";
    args += persist::shell_single_quote(tgt);
  }
  run_up_line_async(args);
}

void on_pack(GtkWidget*, gpointer) {
  std::string err;
  std::string args = "pack";
  if (!shell::append_install_dir_flag(args, get_entry(gs().entry_install), err)) {
    log_line("[error] " + err);
    return;
  }
  run_up_line_async(args);
}

void on_window_destroy(GtkWidget*, gpointer) {
  save_ui_to_persist();
  if (gs().app)
    g_application_quit(G_APPLICATION(gs().app));
}

}  // namespace up::gui::platform::gtk
