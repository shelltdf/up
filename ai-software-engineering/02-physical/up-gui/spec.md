# up-gui 物理规格（摘要）

## 职责边界

- **不得**编译、链接或包含 `src/` 下任何实现文件；仅通过子进程调用 **`up`**。
- 读取/写入 **`up_gui_settings.txt`**（与 GUI 实现一致），在发起 `configure` 时将选项拼接为 **`--opt`**。

## 平台源文件

| 平台 | 主要文件 |
|------|-----------|
| Windows | `src_gui/up_gui_win32.cpp` |
| Linux | `src_gui/main/main_unix.cpp`、`src_gui/platform/gtk_gui.cpp`、`src_gui/core/gui_unix_shared.cpp` |
| macOS | `src_gui/main/main_cocoa.mm`、`src_gui/platform/cocoa_gui.mm`、`src_gui/core/gui_unix_shared.cpp` |

## 与 `UP_ENABLE_REVERSE`

- 与 `up` 同步定义 **`UP_ENABLE_REVERSE`**；影响 GUI 是否暴露逆向（`reverse`）相关能力（见各平台 `*.cpp` / `*.mm` 中 `#if UP_ENABLE_REVERSE`）。
