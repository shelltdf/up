# 详细设计：独立工具 `gz_reverse_cmake`

## 目的

为无法或不便运行 `cmake` configure 的环境，对 **CMakeLists 脚本**做**命令级静态解释**，生成 `package.xml` 与各 `target.xml` 初稿；与主 `gz` 可执行解耦（不实现为子命令）。

## 数据流（高层）

1. 用户可指定 `--source` / `--out`；**省略** `--source` 时取**当前工作目录**为源根；**省略** `--out` 时取 **`<--source>/gz_reverse/`**，与源码同树、子目录名固定。要工作区级或其它根时**显式** `--out`。**显式**将 `--out` 与 `--source` 指到同一路径时告警。
2. 将 `CMakeLists.txt` 规范化为 **命令节点序列**（**语句级浅层 AST** / 命令流）：每条 `name( args )` 对应一节点，再对节点流做**静态重解释**（见 `02-physical/gz-reverse-cmake/spec.md`）——`project` / `set` / `add_subdirectory` / `add_executable` / `add_library` / `include_directories` / `target_sources` / `target_link_libraries` / `target_include_directories` 等。`function`/`macro`/`foreach`/`while` 用块深度**屏蔽**内部 `add_*` 等，**不**做宏/循环展开；**不**求值 `if/else/endif` 条件，两分支中若都出现 `add_*` 可能产生**重复/冲突**目标名，由包作者事后修正。
3. 将已收集的绝对源路径、include 目录转为**相对**各 `target.xml` 目录的 portable 相对路径，写出 XML；`target_link_libraries` 中可判定的同包目标名写为 `<dependency>`。

## 与主系统（`gz`）的关系

- 输出格式以 `doc/zh/package-target-xml-spec.md` 与 `gz` 实现解析器为准；本工具不保证覆盖 `prebuilt_*`、`config_files`、细粒度 `when` 等，也不替代「纯手写工作流」。
- **实现**：C++ 标准库 + 自研脚本解析，**不**使用 CMake File API、**不**使用第三方 JSON 库（见 `02-physical/gz-reverse-cmake/mapping.md`）。
- **发布**：可执行由根 `CMakeLists.txt` 纳管，`install` 与 **`gz` / `gz-gui` 同分量 `gz_runtime`** 至 `bin/`（选项 B）。

## 与系统设计的边界

- 不进入 `gz configure` / `build` 内部管线；见 [system-design.md](system-design.md) 中主流程。本工具是**纯离线、无配置图**辅助，不在运行期参与。
