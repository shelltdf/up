# up-gui（CMake 目标 `up-gui`）

| 项 | 值 |
|----|-----|
| 产物文件名 | `up-gui`（Windows：`up-gui.exe`） |
| 类型 | 可执行文件（Win32 为 `WIN32` 子系统） |
| 源码根 | 仓库根 `src_gui/` **仅** |
| CMake 目标名 | `up-gui` |

CMake 在配置期强制：**所有 `up-gui` 源文件路径必须以 `src_gui/` 为前缀**。

## 源码布局（摘要）

| 区域 | 说明 |
|------|------|
| `src_gui/core/` | `gui_persist`、`gui_shell_actions`、`gui_core_actions`、`gui_os_queue`（与原生 UI 解耦的 UTF-8 逻辑） |
| `src_gui/platform/` | 仅 **`platform/win32/`**、**`platform/gtk/`**、**`platform/cocoa/`** 三棵子树承载各 OS 外壳；约定见 `src_gui/platform/README.md` |
| `src_gui/main/` | 各平台最小 `main_*` |

完整路径映射见 **`mapping.md`**；组件关系见 **`uml-component.md`**。
