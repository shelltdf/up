#pragma once

namespace up::gui::platform::gtk {

// GTK3 应用入口（`gtk_application_run` 在 gtk_gui.cpp）。
// 主窗与布局：gtk_gui_window.*；回调：gtk_gui_handlers.*；日志：gtk_gui_log.*；控件指针与 busy：gtk_gui_state.*。
int run(int argc, char** argv);

}  // namespace up::gui::platform::gtk
