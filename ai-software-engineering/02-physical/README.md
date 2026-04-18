# 物理阶段（02-physical）

按**构建目标**组织，与根 `CMakeLists.txt` 中 `add_executable` 名称对应。

| target-id | 产物 | 说明 |
|-----------|------|------|
| [up-cli](up-cli/) | `up` / `up.exe` | 命令行主程序 |
| [up-gui](up-gui/) | `up-gui` / `up-gui.exe` | 图形外壳 |

各子目录内 **`spec.md`** 为行为与路径语义的单一事实来源之一；与 `01-logic` 冲突时以代码与 `spec.md` 为准回写逻辑层。
