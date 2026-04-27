# up-gui：行为 → 源码映射（摘录）

| 能力 | 路径 |
|------|------|
| 共享动作 / 调起 `up` | `src_gui/core/gui_core_actions.cpp`、`gui_core_actions.hpp` |
| GTK/macOS 设置与 `up_gui_settings.txt` | `src_gui/core/gui_persist.cpp`、`gui_persist.hpp` |
| Win32 UI | `src_gui/main/main_win32.cpp`、`src_gui/platform/win32_gui.cpp`、`win32_gui.hpp` |
| Win32 路径与设置文件位置 | `src_gui/platform/win32_paths.cpp`、`win32_paths.hpp` |
| Win32 编码 UTF-8/宽字符 | `src_gui/platform/win32_encoding.cpp`、`win32_encoding.hpp` |
| Win32 环境 / 文件系统小工具 | `src_gui/platform/win32_fsutil.cpp`、`win32_fsutil.hpp` |
| Win32 文本与时间小工具 | `src_gui/platform/win32_text_util.cpp`、`win32_text_util.hpp` |
| Win32 窗口/显示器居中 | `src_gui/platform/win32_window_placement.cpp`、`win32_window_placement.hpp` |
| Win32 系统文件对话框居中 | `src_gui/platform/win32_modal_file_dialog_center.cpp`、`win32_modal_file_dialog_center.hpp` |
| GTK3 | `src_gui/platform/gtk_gui.cpp`、`gtk_gui.hpp` |
| Cocoa | `src_gui/platform/cocoa_gui.mm`、`cocoa_gui.hpp` |
| Unix 入口 | `src_gui/main/main_unix.cpp` |
| macOS 入口 | `src_gui/main/main_cocoa.mm` |
