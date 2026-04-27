#pragma once

namespace gz::gui::platform::cocoa {

// Cocoa 应用入口（`NSApplication` 在 cocoa_gui.mm）。
// 主窗：cocoa_gui_window.*；GzGuiCtrl：cocoa_gui_handlers.mm + cocoa_gui_internal.h；调 gz / 面板：cocoa_gui_bridge.*；
// 日志：cocoa_gui_log.*；NSString 转换：cocoa_gui_string.*；全局状态：cocoa_gui_state.*。
int run(int argc, char** argv);

}  // namespace gz::gui::platform::cocoa
