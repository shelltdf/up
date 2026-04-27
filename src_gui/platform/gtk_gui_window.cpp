// GTK3：主窗布局与 activate（回调实现见 gtk_gui_handlers.cpp）。

#include "platform/gtk_gui_window.hpp"

#include "platform/gtk_gui_handlers.hpp"
#include "platform/gtk_gui_log.hpp"
#include "platform/gtk_gui_state.hpp"

#include "gui_persist.hpp"

#include <filesystem>
#include <gtk/gtk.h>

namespace up::gui::platform::gtk {

namespace persist = up::gui::persist;

void gtk_application_activate(GtkApplication* app, gpointer) {
  gs().app = app;
  gs().settings_file = persist::settings_path_near_executable();
  gs().up_exe = persist::executable_parent_dir() / "up";
  (void)persist::load_settings(gs().settings_file, gs().persist);

  GtkWindow* w = GTK_WINDOW(gtk_application_window_new(app));
  gs().win = GTK_WIDGET(w);
  gtk_window_set_title(w, "up-gui");
  gtk_window_set_default_size(w, 920, 680);
  gtk_window_set_position(w, GTK_WIN_POS_CENTER_ALWAYS);
  {
    const std::filesystem::path icon_path = persist::executable_parent_dir() / "up_gui.png";
    if (std::filesystem::exists(icon_path))
      gtk_window_set_icon_from_file(w, icon_path.string().c_str(), nullptr);
  }
  g_signal_connect(w, "destroy", G_CALLBACK(on_window_destroy), nullptr);

  GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gtk_container_add(GTK_CONTAINER(w), vbox);

  GtkWidget* mb = gtk_menu_bar_new();
  GtkWidget* mi_actions = gtk_menu_item_new_with_mnemonic("_Actions");
  GtkWidget* menu_actions = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi_actions), menu_actions);
  auto add_menu = [&](const char* label, void (*cb)(GtkWidget*, gpointer)) {
    GtkWidget* it = gtk_menu_item_new_with_mnemonic(label);
    g_signal_connect(it, "activate", G_CALLBACK(cb), nullptr);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_actions), it);
  };
#if UP_ENABLE_REVERSE
  add_menu("_Reverse", on_reverse);
#endif
  add_menu("_Configure", on_configure);
  add_menu("_Build", on_build);
  add_menu("_Run", on_run);
  add_menu("_Test", on_test);
  add_menu("_Pack", on_pack);
  gtk_menu_shell_append(GTK_MENU_SHELL(mb), mi_actions);
  gtk_box_pack_start(GTK_BOX(vbox), mb, FALSE, FALSE, 0);

  GtkWidget* bar = gtk_toolbar_new();
  auto add_tb = [&](const char* label, void (*cb)(GtkWidget*, gpointer)) {
    GtkToolItem* b = gtk_tool_button_new(nullptr, label);
    g_signal_connect(b, "clicked", G_CALLBACK(cb), nullptr);
    gtk_toolbar_insert(GTK_TOOLBAR(bar), b, -1);
  };
#if UP_ENABLE_REVERSE
  add_tb("Reverse", on_reverse);
#endif
  add_tb("Configure", on_configure);
  add_tb("Build", on_build);
  add_tb("Run", on_run);
  add_tb("Test", on_test);
  add_tb("Pack", on_pack);
  gtk_box_pack_start(GTK_BOX(vbox), bar, FALSE, FALSE, 0);

  GtkWidget* grid = gtk_grid_new();
  gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
  gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
  int r = 0;
  gtk_grid_attach(GTK_GRID(grid), gtk_label_new("CWD"), 0, r, 1, 1);
  gs().entry_cwd = gtk_entry_new();
  gtk_grid_attach(GTK_GRID(grid), gs().entry_cwd, 1, r, 1, 1);
  GtkWidget* btn_cwd = gtk_button_new_with_label("Browse…");
  g_signal_connect(btn_cwd, "clicked", G_CALLBACK(on_browse_cwd), nullptr);
  gtk_grid_attach(GTK_GRID(grid), btn_cwd, 2, r, 1, 1);
  ++r;

  gs().store_scan = gtk_list_store_new(1, G_TYPE_STRING);
  gs().scan_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(gs().store_scan));
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(gs().scan_view), -1, "Scan roots", gtk_cell_renderer_text_new(),
                                              "text", 0, nullptr);
  GtkWidget* sw_scan = gtk_scrolled_window_new(nullptr, nullptr);
  gtk_widget_set_size_request(sw_scan, -1, 120);
  gtk_container_add(GTK_CONTAINER(sw_scan), gs().scan_view);
  gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Scan"), 0, r, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), sw_scan, 1, r, 2, 1);
  ++r;
  GtkWidget* hscan = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
  GtkWidget* badd = gtk_button_new_with_label("+Add");
  GtkWidget* bdel = gtk_button_new_with_label("-Remove");
  g_signal_connect(badd, "clicked", G_CALLBACK(on_scan_add), nullptr);
  g_signal_connect(bdel, "clicked", G_CALLBACK(on_scan_remove), nullptr);
  gtk_box_pack_start(GTK_BOX(hscan), badd, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hscan), bdel, FALSE, FALSE, 0);
  gtk_grid_attach(GTK_GRID(grid), hscan, 1, r, 2, 1);
  ++r;

  auto row = [&](const char* lbl, GtkWidget** out) {
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new(lbl), 0, r, 1, 1);
    *out = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), *out, 1, r, 2, 1);
    ++r;
  };
  row("Build dir", &gs().entry_build);
  row("Install dir", &gs().entry_install);
  row("Extra args", &gs().entry_extra);
  row("Run target", &gs().entry_run);
  row("Test name", &gs().entry_test);

  gtk_box_pack_start(GTK_BOX(vbox), grid, FALSE, FALSE, 0);

  gs().log_view = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(gs().log_view), FALSE);
  gtk_text_view_set_monospace(GTK_TEXT_VIEW(gs().log_view), TRUE);
  GtkWidget* sw_log = gtk_scrolled_window_new(nullptr, nullptr);
  gtk_widget_set_vexpand(sw_log, TRUE);
  gtk_container_add(GTK_CONTAINER(sw_log), gs().log_view);
  gtk_box_pack_start(GTK_BOX(vbox), sw_log, TRUE, TRUE, 0);

  gs().status = gtk_label_new("Ready");
  gtk_label_set_xalign(GTK_LABEL(gs().status), 0);
  gtk_box_pack_start(GTK_BOX(vbox), gs().status, FALSE, FALSE, 0);

  if (!gs().persist.browse_cwd.empty())
    gtk_entry_set_text(GTK_ENTRY(gs().entry_cwd), gs().persist.browse_cwd.c_str());

  gtk_widget_show_all(GTK_WIDGET(w));
  log_line("up-gui (GTK) — settings: " + gs().settings_file.generic_string());
}

}  // namespace up::gui::platform::gtk
