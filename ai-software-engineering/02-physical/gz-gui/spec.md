# gz-gui 物理规格（摘要）

## 职责边界

- **不得**编译、链接或包含 `GroundZero/` 下任何实现文件；仅通过子进程调用 **`gz`**。
- 读取/写入 **`gz_gui_settings.txt`**（与 GUI 实现一致），在发起 `configure` 时将选项拼接为 **`--opt`**。
- **core 层（三平台均链接）**：`core/gui_shell_actions.*` 承载与原生控件无关的「调 `gz`」逻辑（configure 参数行、`--install-dir-name` 片段、占用 busy、后台子进程）；Unix 用 `popen`，Windows 由 `gui_persist::run_shell_in_dir` 经 PowerShell `-EncodedCommand` 执行等价 POSIX 单引号命令行。`core/gui_os_queue.hpp` 约定日志/收尾任务可由工作线程触发、由平台层投递到 UI 线程。各 `platform/win32/`、`platform/gtk/`、`platform/cocoa/` 下文件负责主窗、消息循环与系统对话框；Win32 主流程仍以 `win32/win32_gui.cpp` 宽字符实现为主。
- **命名（与实现对齐）**：`gz::gui::shell` 中后台调 CLI 为 **`run_gz_command_in_detached_thread(gz_exe, ...)`**；`build_configure_args_line` / `try_acquire_run_context` 的首参为 **`gz_exe`**；`persist::query_print_build_dir_name` 首参为 **`gz_exe`**。GTK / Cocoa 单例状态里 CLI 路径字段为 **`gz_exe`**；macOS 侧 **`GzGuiCtrl`**（`cocoa_gui_internal.h` / `cocoa_gui_handlers.mm`）承载表格与窗口委托。

## 平台源文件

| 平台 | 主要文件 |
|------|-----------|
| Windows | `main/main_win32.cpp`、`platform/win32/win32_gui.cpp`、`platform/win32/win32_gui.hpp`、`platform/win32/win32_gui_ids.hpp`、`platform/win32/win32_run_process.*`、`platform/win32/win32_paths.*`、`platform/win32/win32_encoding.*`、`platform/win32/win32_fsutil.*`、`platform/win32/win32_text_util.*`、`platform/win32/win32_window_placement.*`、`platform/win32/win32_modal_file_dialog_center.*`、`resources/gz_gui.rc`、`core/gui_persist.cpp`、`core/gui_persist.hpp`、`core/gui_shell_actions.cpp`、`core/gui_shell_actions.hpp`、`core/gui_os_queue.hpp`（路径前缀均为 `GroundZeroGUI/`） |
| Linux | `main/main_unix.cpp`、`platform/gtk/gtk_gui.cpp`、`platform/gtk/gtk_gui.hpp`、`platform/gtk/gtk_gui_window.*`、`platform/gtk/gtk_gui_handlers.*`、`platform/gtk/gtk_gui_log.*`、`platform/gtk/gtk_gui_state.*`、`core/gui_persist.*`、`core/gui_shell_actions.*`、`core/gui_os_queue.hpp` |
| macOS | `main/main_cocoa.mm`、`platform/cocoa/cocoa_gui.mm`、`platform/cocoa/cocoa_gui.hpp`、`platform/cocoa/cocoa_gui_internal.h`、`platform/cocoa/cocoa_gui_string.*`、`platform/cocoa/cocoa_gui_log.*`、`platform/cocoa/cocoa_gui_bridge.*`、`platform/cocoa/cocoa_gui_handlers.mm`、`platform/cocoa/cocoa_gui_window.*`、`platform/cocoa/cocoa_gui_state.*`、`core/gui_persist.*`、`core/gui_shell_actions.*`、`core/gui_os_queue.hpp` |
