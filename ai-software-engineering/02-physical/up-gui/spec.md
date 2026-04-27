# up-gui 物理规格（摘要）

## 职责边界

- **不得**编译、链接或包含 `src/` 下任何实现文件；仅通过子进程调用 **`up`**。
- 读取/写入 **`up_gui_settings.txt`**（与 GUI 实现一致），在发起 `configure` 时将选项拼接为 **`--opt`**。

## 平台源文件

| 平台 | 主要文件 |
|------|-----------|
| Windows | `main/main_win32.cpp`、`platform/win32_gui.cpp`、`win32_gui.hpp`、`platform/win32_paths.*`、`platform/win32_window_placement.*`、`platform/win32_modal_file_dialog_center.*`、`resources/up_gui.rc`（路径前缀均为 `src_gui/`） |
| Linux | `main/main_unix.cpp`、`platform/gtk_gui.cpp`、`gtk_gui.hpp`、`core/gui_persist.cpp`、`core/gui_persist.hpp` |
| macOS | `main/main_cocoa.mm`、`platform/cocoa_gui.mm`、`cocoa_gui.hpp`、`core/gui_persist.cpp`、`core/gui_persist.hpp` |

## 与 `UP_ENABLE_REVERSE`

- 与 `up` 同步定义 **`UP_ENABLE_REVERSE`**；影响 GUI 是否暴露逆向（`reverse`）相关能力（见各平台 `*.cpp` / `*.mm` 中 `#if UP_ENABLE_REVERSE`）。
