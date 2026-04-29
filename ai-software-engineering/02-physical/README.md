# 物理阶段（02-physical）

按**构建目标**组织，与根 `CMakeLists.txt` 中 `add_executable` 名称对应。

| target-id | 产物 | 说明 |
|-----------|------|------|
| [gz-cli](gz-cli/) | `gz` / `gz.exe` | 命令行主程序 |
| [gz-gui](gz-gui/) | `gz-gui` / `gz-gui.exe` | 图形外壳 |
| [gz-reverse-cmake](gz-reverse-cmake/) | `gz_reverse_cmake` / `gz_reverse_cmake.exe` | 静态读 `CMakeLists.txt` 命令子集，生成 `package.xml` / `target.xml`（不跑 `cmake`、不用 File API）；与 `gz` / `gz-gui` 同属 **`gz_runtime`**，默认 `install.py` 装到 **`bin/`**（源码根 `gz_reverse_cmake/`） |

各子目录内 **`spec.md`** 为行为与路径语义的单一事实来源之一；与 `01-logic` 冲突时以代码与 `spec.md` 为准回写逻辑层。

用户侧 XML 工作流以仓库 **`doc/zh/package-target-xml-spec.md`** 文首「产品方向」为准（索引导航见 **`doc/README.md`**）：**纯手写** `package.xml` / `target.xml`；**`gz reverse` 已移除** — 若需从已有 CMake 工程**辅助**得到初稿，请使用与主工程同仓、**随默认安装分发的**可执行程序 **`gz_reverse_cmake`**（独立子命令，**不**并入 `gz` 的 argv）。
