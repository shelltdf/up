#pragma once

#include "gui_persist.hpp"

#include <atomic>
#include <filesystem>
#include <gtk/gtk.h>

namespace up::gui::platform::gtk {

// 主窗与各控件指针（多编译单元共享，单例在 gtk_gui_state.cpp）。
struct GuiState {
  std::atomic<bool> busy{false};
  GtkApplication* app{};
  GtkWidget* win{};
  GtkWidget* entry_cwd{};
  GtkWidget* entry_build{};
  GtkWidget* entry_install{};
  GtkWidget* entry_extra{};
  GtkWidget* entry_run{};
  GtkWidget* entry_test{};
  GtkListStore* store_scan{};
  GtkWidget* scan_view{};
  GtkWidget* log_view{};
  GtkWidget* status{};
  persist::PersistedEnv persist{};
  std::filesystem::path settings_file;
  std::filesystem::path up_exe;
};

GuiState& gs();

}  // namespace up::gui::platform::gtk
