# `src_gui/platform/` 目录约定

原生 GUI 外壳按操作系统分在三个子目录中（**不要**在 `platform/` 根下新增可编译的 `.cpp/.mm`，除非 CMake 与文档显式允许）：

| 子目录 | 用途 |
|--------|------|
| `win32/` | Windows Win32、`up_gui.rc` 由 CMake 列在目标中，路径仍写 `resources/up_gui.rc` |
| `gtk/` | Linux GTK3 |
| `cocoa/` | macOS Cocoa（含 `cocoa_gui_internal.h`） |

**包含路径**：以 `src_gui/` 为 include 根，使用 `#include "platform/win32/..."` 等形式（与 `CMakeLists.txt` 中 `target_include_directories(..., src_gui)` 一致）。

**跨平台共享逻辑**放在 `src_gui/core/`（如 `gui_persist`、`gui_shell_actions`），不要从 `platform/<os>/` 反向包含「另一平台」子目录下的头文件。
