# gz-gui：行为 → 源码映射（摘录）

**目录**：原生实现仅位于 `GroundZeroGUI/platform/win32/`、`platform/gtk/`、`platform/cocoa/`（见 `GroundZeroGUI/platform/README.md`）。

| 能力 | 路径 |
|------|------|
| 共享动作 / 调起 `gz` | `GroundZeroGUI/core/gui_core_actions.cpp`、`gui_core_actions.hpp` |
| 与 UI 线程投递相关的类型别名 | `GroundZeroGUI/core/gui_os_queue.hpp` |
| configure/run 等命令拼装与后台调 `gz`（无原生控件；三平台 core 均编译） | `GroundZeroGUI/core/gui_shell_actions.cpp`、`gui_shell_actions.hpp` |
| UTF-8 设置与 `gz_gui_settings.txt`、POSIX 风格子进程封装 | `GroundZeroGUI/core/gui_persist.cpp`、`gui_persist.hpp` |
| Win32 UI | `GroundZeroGUI/main/main_win32.cpp`、`GroundZeroGUI/platform/win32/win32_gui.cpp`、`GroundZeroGUI/platform/win32/win32_gui.hpp` |
| Win32 控件/菜单/自定义消息 ID | `GroundZeroGUI/platform/win32/win32_gui_ids.hpp` |
| Win32 子进程输出分块（UTF-8→宽字符行） | `GroundZeroGUI/platform/win32/win32_run_process.cpp`、`GroundZeroGUI/platform/win32/win32_run_process.hpp` |
| Win32 路径与设置文件位置 | `GroundZeroGUI/platform/win32/win32_paths.cpp`、`GroundZeroGUI/platform/win32/win32_paths.hpp` |
| Win32 编码 UTF-8/宽字符 | `GroundZeroGUI/platform/win32/win32_encoding.cpp`、`GroundZeroGUI/platform/win32/win32_encoding.hpp` |
| Win32 环境 / 文件系统小工具 | `GroundZeroGUI/platform/win32/win32_fsutil.cpp`、`GroundZeroGUI/platform/win32/win32_fsutil.hpp` |
| Win32 文本与时间小工具 | `GroundZeroGUI/platform/win32/win32_text_util.cpp`、`GroundZeroGUI/platform/win32/win32_text_util.hpp` |
| Win32 窗口/显示器居中 | `GroundZeroGUI/platform/win32/win32_window_placement.cpp`、`GroundZeroGUI/platform/win32/win32_window_placement.hpp` |
| Win32 系统文件对话框居中 | `GroundZeroGUI/platform/win32/win32_modal_file_dialog_center.cpp`、`GroundZeroGUI/platform/win32/win32_modal_file_dialog_center.hpp` |
| GTK3 入口 | `GroundZeroGUI/platform/gtk/gtk_gui.cpp`、`GroundZeroGUI/platform/gtk/gtk_gui.hpp` |
| GTK3 主窗布局 / `activate` | `GroundZeroGUI/platform/gtk/gtk_gui_window.cpp`、`GroundZeroGUI/platform/gtk/gtk_gui_window.hpp` |
| GTK3 菜单与按钮回调 | `GroundZeroGUI/platform/gtk/gtk_gui_handlers.cpp`、`GroundZeroGUI/platform/gtk/gtk_gui_handlers.hpp` |
| GTK3 日志投递 | `GroundZeroGUI/platform/gtk/gtk_gui_log.cpp`、`GroundZeroGUI/platform/gtk/gtk_gui_log.hpp` |
| GTK3 全局控件状态 | `GroundZeroGUI/platform/gtk/gtk_gui_state.cpp`、`GroundZeroGUI/platform/gtk/gtk_gui_state.hpp` |
| Cocoa 入口 | `GroundZeroGUI/platform/cocoa/cocoa_gui.mm`、`GroundZeroGUI/platform/cocoa/cocoa_gui.hpp` |
| Cocoa `GzGuiCtrl` 声明（ObjC） | `GroundZeroGUI/platform/cocoa/cocoa_gui_internal.h` |
| Cocoa 字符串 / NSString | `GroundZeroGUI/platform/cocoa/cocoa_gui_string.mm`、`GroundZeroGUI/platform/cocoa/cocoa_gui_string.hpp` |
| Cocoa 日志与状态栏 | `GroundZeroGUI/platform/cocoa/cocoa_gui_log.mm`、`GroundZeroGUI/platform/cocoa/cocoa_gui_log.hpp` |
| Cocoa 调 `gz` 与文件面板辅助 | `GroundZeroGUI/platform/cocoa/cocoa_gui_bridge.mm`、`GroundZeroGUI/platform/cocoa/cocoa_gui_bridge.hpp` |
| Cocoa 控制器实现 | `GroundZeroGUI/platform/cocoa/cocoa_gui_handlers.mm` |
| Cocoa 主窗搭建 | `GroundZeroGUI/platform/cocoa/cocoa_gui_window.mm`、`GroundZeroGUI/platform/cocoa/cocoa_gui_window.hpp` |
| Cocoa 全局状态 | `GroundZeroGUI/platform/cocoa/cocoa_gui_state.mm`、`GroundZeroGUI/platform/cocoa/cocoa_gui_state.hpp` |
| Unix 入口 | `GroundZeroGUI/main/main_unix.cpp` |
| macOS 入口 | `GroundZeroGUI/main/main_cocoa.mm` |
