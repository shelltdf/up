#pragma once

namespace up::gui::platform::cocoa {

// Cocoa 应用入口（`NSApplication` 在 cocoa_gui.mm）。
// 主窗：cocoa_gui_window.*；UpGuiCtrl：cocoa_gui_handlers.mm + cocoa_gui_internal.h；调 up / 面板：cocoa_gui_bridge.*；
// 日志：cocoa_gui_log.*；NSString 转换：cocoa_gui_string.*；全局状态：cocoa_gui_state.*。
int run(int argc, char** argv);

}  // namespace up::gui::platform::cocoa
