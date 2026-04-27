// Linux GTK3：与 Windows 版 up-gui（platform/win32_gui.cpp）同一套 up_gui_settings.txt 与 configure 传参逻辑（CWD、扫描目录、环境 --opt、build-dir-name）。
// 未实现：Win32 上的「编译环境」三 Tab 弹窗、选项表 ListView、从 up_cache 自动填运行目标等（可后续补）。

#include "platform/gtk_gui.hpp"

#include "gui_persist.hpp"

#include <gtk/gtk.h>

#include <atomic>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace up::gui::platform::gtk {
namespace {

namespace persist = up::gui::persist;

std::atomic<bool> g_busy{false};
GtkApplication* g_app{};

GtkWidget* g_win{};
GtkWidget* g_entry_cwd{};
GtkWidget* g_entry_build{};
GtkWidget* g_entry_install{};
GtkWidget* g_entry_extra{};
GtkWidget* g_entry_run{};
GtkWidget* g_entry_test{};
GtkListStore* g_store_scan{};
GtkWidget* g_scan_view{};
GtkWidget* g_log_view{};
GtkWidget* g_status{};

persist::PersistedEnv g_persist{};
std::filesystem::path g_settings_file;
std::filesystem::path g_up_exe;

static void trim_ascii(std::string& s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
    s.erase(0, 1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
    s.pop_back();
}

std::string get_entry(GtkWidget* e) {
  return e ? gtk_entry_get_text(GTK_ENTRY(e)) : std::string{};
}

void set_status(const char* s) {
  if (g_status)
    gtk_label_set_text(GTK_LABEL(g_status), s);
}

struct LogMsg {
  std::string text;
};
static gboolean on_log_idle(gpointer data) {
  auto* m = static_cast<LogMsg*>(data);
  if (g_log_view && m) {
    GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(g_log_view));
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buf, &end);
    gtk_text_buffer_insert(buf, &end, m->text.c_str(), static_cast<int>(m->text.size()));
  }
  delete m;
  return FALSE;
}

void log_line(const std::string& s) {
  g_idle_add(on_log_idle, new LogMsg{s + "\n"});
}

std::vector<std::string> collect_scans() {
  std::vector<std::string> out;
  if (!g_store_scan)
    return out;
  GtkTreeIter it{};
  if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(g_store_scan), &it))
    return out;
  do {
    gchar* path = nullptr;
    gtk_tree_model_get(GTK_TREE_MODEL(g_store_scan), &it, 0, &path, -1);
    if (path && path[0])
      out.emplace_back(path);
    g_free(path);
  } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(g_store_scan), &it));
  return out;
}

void save_ui_to_persist() {
  g_persist.browse_cwd = persist::path_to_portable_utf8(get_entry(g_entry_cwd));
  persist::save_settings(g_settings_file, g_persist);
}

void on_browse_cwd(GtkWidget*, gpointer) {
  GtkWidget* dlg = gtk_file_chooser_dialog_new("Working directory (CWD)", GTK_WINDOW(g_win), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                               "_Cancel", GTK_RESPONSE_CANCEL, "_Select", GTK_RESPONSE_ACCEPT, nullptr);
  gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER_ALWAYS);
  std::string init = get_entry(g_entry_cwd);
  if (init.empty())
    init = g_persist.browse_cwd;
  if (!init.empty())
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), init.c_str());
  if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
    char* p = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
    if (p) {
      const std::string portable = persist::path_to_portable_utf8(p);
      gtk_entry_set_text(GTK_ENTRY(g_entry_cwd), portable.c_str());
      g_persist.browse_cwd = portable;
      g_free(p);
    }
  }
  gtk_widget_destroy(dlg);
}

void on_scan_add(GtkWidget*, gpointer) {
  GtkWidget* dlg = gtk_file_chooser_dialog_new("Add scan root", GTK_WINDOW(g_win), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                               "_Cancel", GTK_RESPONSE_CANCEL, "_Select", GTK_RESPONSE_ACCEPT, nullptr);
  gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER_ALWAYS);
  if (!g_persist.browse_scan.empty())
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), g_persist.browse_scan.c_str());
  if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
    char* p = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
    if (p) {
      g_persist.browse_scan = persist::path_to_portable_utf8(p);
      GtkTreeIter it{};
      gtk_list_store_append(g_store_scan, &it);
      gtk_list_store_set(g_store_scan, &it, 0, g_persist.browse_scan.c_str(), -1);
      g_free(p);
    }
  }
  gtk_widget_destroy(dlg);
}

void on_scan_remove(GtkWidget*, gpointer) {
  GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(g_scan_view));
  GtkTreeIter it{};
  if (gtk_tree_selection_get_selected(sel, nullptr, &it))
    gtk_list_store_remove(g_store_scan, &it);
}

static void append_install_dir_flag(std::string& args, const std::string& inst_edit, std::string& err) {
  std::string inst = persist::path_to_portable_utf8(inst_edit);
  trim_ascii(inst);
  if (inst.empty()) {
    err = "Install Dir empty (--install-dir-name required)";
    return;
  }
  const std::string leaf = persist::intermediate_leaf_from_build_dir_field(inst);
  if (leaf.empty()) {
    err = "Invalid install dir / leaf for --install-dir-name";
    return;
  }
  args += " --install-dir-name ";
  args += persist::shell_single_quote(leaf);
}

void run_up_line_async(const std::string& args_no_exe) {
  if (g_busy.exchange(true)) {
    log_line("[busy] previous run still in progress");
    g_busy = false;
    return;
  }
  const std::string cwd = persist::path_to_portable_utf8(get_entry(g_entry_cwd));
  if (cwd.empty()) {
    log_line("[error] Set CWD first.");
    g_busy = false;
    return;
  }
  if (!std::filesystem::exists(g_up_exe)) {
    log_line("[error] `up` not found next to this executable.");
    g_busy = false;
    return;
  }

  std::string extra = get_entry(g_entry_extra);
  trim_ascii(extra);

  std::thread([args_no_exe, cwd, extra]() {
    const std::filesystem::path cwd_p(cwd);
    std::string cmd = persist::shell_single_quote(g_up_exe.generic_string()) + " " + args_no_exe;
    if (!extra.empty())
      cmd += " " + extra;
    std::string out;
    int code = -1;
    const bool ok = persist::run_shell_in_dir(cwd_p, cmd, out, code);
    if (!ok)
      log_line("[error] failed to spawn shell");
    else {
      if (!out.empty())
        log_line(out);
      log_line("[exit " + std::to_string(code) + "]");
    }
    g_busy = false;
    g_idle_add(
        +[](gpointer) -> gboolean {
          set_status("Ready");
          return FALSE;
        },
        nullptr);
  }).detach();
  set_status("Running up…");
}

// 返回非空则为完整 configure 参数行（不含可执行文件路径）；失败时写日志并返回空。
std::string build_configure_args_line() {
  const std::string cwd = persist::path_to_portable_utf8(get_entry(g_entry_cwd));
  if (cwd.empty()) {
    log_line("[error] Set CWD first.");
    return {};
  }
  std::string leaf;
  std::string qerr;
  if (!persist::query_print_build_dir_name(g_up_exe, std::filesystem::path(cwd), get_entry(g_entry_build), g_persist,
                                              leaf, qerr)) {
    log_line("[error] print-build-dir-name: " + qerr);
    return {};
  }
  std::string args = "configure";
  persist::append_scan_args_utf8(args, collect_scans(), cwd);
  persist::append_configure_env_opts(g_persist, args);
  args += " --build-dir-name ";
  args += persist::shell_single_quote(leaf);
  return args;
}

#if UP_ENABLE_REVERSE
void on_reverse(GtkWidget*, gpointer) {
  run_up_line_async("reverse");
}
#endif

void on_configure(GtkWidget*, gpointer) {
  const std::string cfg = build_configure_args_line();
  if (cfg.empty())
    return;
  run_up_line_async(cfg);
}

void on_build(GtkWidget*, gpointer) {
  std::string args = "build";
  const std::string leaf = persist::intermediate_leaf_from_build_dir_field(get_entry(g_entry_build));
  if (leaf.empty()) {
    log_line("[error] Set Build Dir (or leaf name) first.");
    return;
  }
  args += " --build-dir-name ";
  args += persist::shell_single_quote(leaf);
  run_up_line_async(args);
}

void on_run(GtkWidget*, gpointer) {
  std::string tgt = get_entry(g_entry_run);
  trim_ascii(tgt);
  if (tgt.empty()) {
    log_line("[error] Run target empty.");
    return;
  }
  std::string err;
  std::string args = "run";
  append_install_dir_flag(args, get_entry(g_entry_install), err);
  if (!err.empty()) {
    log_line("[error] " + err);
    return;
  }
  args += " ";
  args += persist::shell_single_quote(tgt);
  run_up_line_async(args);
}

void on_test(GtkWidget*, gpointer) {
  std::string tgt = get_entry(g_entry_test);
  trim_ascii(tgt);
  std::string err;
  std::string args = "test";
  append_install_dir_flag(args, get_entry(g_entry_install), err);
  if (!err.empty()) {
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
  append_install_dir_flag(args, get_entry(g_entry_install), err);
  if (!err.empty()) {
    log_line("[error] " + err);
    return;
  }
  run_up_line_async(args);
}

void on_window_destroy(GtkWidget*, gpointer) {
  save_ui_to_persist();
  if (g_app)
    g_application_quit(G_APPLICATION(g_app));
}

static void activate(GtkApplication* app, gpointer) {
  g_app = app;
  g_settings_file = persist::settings_path_near_executable();
  g_up_exe = persist::executable_parent_dir() / "up";
  (void)persist::load_settings(g_settings_file, g_persist);

  GtkWindow* w = GTK_WINDOW(gtk_application_window_new(app));
  g_win = GTK_WIDGET(w);
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
  g_entry_cwd = gtk_entry_new();
  gtk_grid_attach(GTK_GRID(grid), g_entry_cwd, 1, r, 1, 1);
  GtkWidget* btn_cwd = gtk_button_new_with_label("Browse…");
  g_signal_connect(btn_cwd, "clicked", G_CALLBACK(on_browse_cwd), nullptr);
  gtk_grid_attach(GTK_GRID(grid), btn_cwd, 2, r, 1, 1);
  ++r;

  g_store_scan = gtk_list_store_new(1, G_TYPE_STRING);
  g_scan_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(g_store_scan));
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(g_scan_view), -1, "Scan roots", gtk_cell_renderer_text_new(),
                                              "text", 0, nullptr);
  GtkWidget* sw_scan = gtk_scrolled_window_new(nullptr, nullptr);
  gtk_widget_set_size_request(sw_scan, -1, 120);
  gtk_container_add(GTK_CONTAINER(sw_scan), g_scan_view);
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
  row("Build dir", &g_entry_build);
  row("Install dir", &g_entry_install);
  row("Extra args", &g_entry_extra);
  row("Run target", &g_entry_run);
  row("Test name", &g_entry_test);

  gtk_box_pack_start(GTK_BOX(vbox), grid, FALSE, FALSE, 0);

  g_log_view = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(g_log_view), FALSE);
  gtk_text_view_set_monospace(GTK_TEXT_VIEW(g_log_view), TRUE);
  GtkWidget* sw_log = gtk_scrolled_window_new(nullptr, nullptr);
  gtk_widget_set_vexpand(sw_log, TRUE);
  gtk_container_add(GTK_CONTAINER(sw_log), g_log_view);
  gtk_box_pack_start(GTK_BOX(vbox), sw_log, TRUE, TRUE, 0);

  g_status = gtk_label_new("Ready");
  gtk_label_set_xalign(GTK_LABEL(g_status), 0);
  gtk_box_pack_start(GTK_BOX(vbox), g_status, FALSE, FALSE, 0);

  if (!g_persist.browse_cwd.empty())
    gtk_entry_set_text(GTK_ENTRY(g_entry_cwd), g_persist.browse_cwd.c_str());

  gtk_widget_show_all(GTK_WIDGET(w));
  log_line("up-gui (GTK) — settings: " + g_settings_file.generic_string());
}

}  // namespace

int run(int argc, char** argv) {
  GtkApplication* app = gtk_application_new("dev.up.up_gui", G_APPLICATION_FLAGS_NONE);
  g_signal_connect(app, "activate", G_CALLBACK(activate), nullptr);
  const int st = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return st;
}

}  // namespace up::gui::platform::gtk
