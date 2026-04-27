# up-gui：行为 → 源码映射（摘录）

| 能力 | 路径 |
|------|------|
| 共享动作 / 调起 `up` | `src_gui/core/gui_core_actions.cpp`、`gui_core_actions.hpp` |
| Win32 UI | `src_gui/main/main_win32.cpp`, `src_gui/platform/win32_gui.cpp` |
| GTK3 | `src_gui/platform/gtk_gui.cpp` |
| Cocoa | `src_gui/platform/cocoa_gui.mm` |
| Unix 入口 | `src_gui/main/main_unix.cpp` |
