# up-gui：行为 → 源码映射（摘录）

| 能力 | 路径 |
|------|------|
| 共享动作 / 调起 `up` | `src_gui/core/gui_core_actions.cpp`、`gui_core_actions.hpp` |
| 与 UI 线程投递相关的类型别名 | `src_gui/core/gui_os_queue.hpp` |
| configure/run 等命令拼装与后台调 `up`（无原生控件；三平台 core 均编译） | `src_gui/core/gui_shell_actions.cpp`、`gui_shell_actions.hpp` |
| UTF-8 设置与 `up_gui_settings.txt`、POSIX 风格子进程封装 | `src_gui/core/gui_persist.cpp`、`gui_persist.hpp` |
| Win32 UI | `src_gui/main/main_win32.cpp`、`src_gui/platform/win32_gui.cpp`、`win32_gui.hpp` |
| Win32 控件/菜单/自定义消息 ID | `src_gui/platform/win32_gui_ids.hpp` |
| Win32 子进程输出分块（UTF-8→宽字符行） | `src_gui/platform/win32_run_process.cpp`、`win32_run_process.hpp` |
| Win32 路径与设置文件位置 | `src_gui/platform/win32_paths.cpp`、`win32_paths.hpp` |
| Win32 编码 UTF-8/宽字符 | `src_gui/platform/win32_encoding.cpp`、`win32_encoding.hpp` |
| Win32 环境 / 文件系统小工具 | `src_gui/platform/win32_fsutil.cpp`、`win32_fsutil.hpp` |
| Win32 文本与时间小工具 | `src_gui/platform/win32_text_util.cpp`、`win32_text_util.hpp` |
| Win32 窗口/显示器居中 | `src_gui/platform/win32_window_placement.cpp`、`win32_window_placement.hpp` |
| Win32 系统文件对话框居中 | `src_gui/platform/win32_modal_file_dialog_center.cpp`、`win32_modal_file_dialog_center.hpp` |
| GTK3 入口 | `src_gui/platform/gtk_gui.cpp`、`gtk_gui.hpp` |
| GTK3 主窗布局 / `activate` | `src_gui/platform/gtk_gui_window.cpp`、`gtk_gui_window.hpp` |
| GTK3 菜单与按钮回调 | `src_gui/platform/gtk_gui_handlers.cpp`、`gtk_gui_handlers.hpp` |
| GTK3 日志投递 | `src_gui/platform/gtk_gui_log.cpp`、`gtk_gui_log.hpp` |
| GTK3 全局控件状态 | `src_gui/platform/gtk_gui_state.cpp`、`gtk_gui_state.hpp` |
| Cocoa 入口 | `src_gui/platform/cocoa_gui.mm`、`cocoa_gui.hpp` |
| Cocoa `UpGuiCtrl` 声明（ObjC） | `src_gui/platform/cocoa_gui_internal.h` |
| Cocoa 字符串 / NSString | `src_gui/platform/cocoa_gui_string.mm`、`cocoa_gui_string.hpp` |
| Cocoa 日志与状态栏 | `src_gui/platform/cocoa_gui_log.mm`、`cocoa_gui_log.hpp` |
| Cocoa 调 `up` 与文件面板辅助 | `src_gui/platform/cocoa_gui_bridge.mm`、`cocoa_gui_bridge.hpp` |
| Cocoa 控制器实现 | `src_gui/platform/cocoa_gui_handlers.mm` |
| Cocoa 主窗搭建 | `src_gui/platform/cocoa_gui_window.mm`、`cocoa_gui_window.hpp` |
| Cocoa 全局状态 | `src_gui/platform/cocoa_gui_state.mm`、`cocoa_gui_state.hpp` |
| Unix 入口 | `src_gui/main/main_unix.cpp` |
| macOS 入口 | `src_gui/main/main_cocoa.mm` |
