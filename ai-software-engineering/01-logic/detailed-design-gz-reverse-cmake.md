# 详细设计：独立工具 `gz_reverse_cmake`

## 目的

为无法或不便运行 `cmake` configure 的环境，对 **CMakeLists 脚本**做**命令级静态解释**，生成 `package.xml` 与各 `target.xml` 初稿；与主 `gz` 可执行解耦（不实现为子命令）。

## 数据流（高层）

1. 用户可指定 `--source` / `--out`；**省略** `--source` 时取**当前工作目录**为源根；**省略** `--out` 时取 **`<--source>/gz_reverse/`**，与源码同树、子目录名固定。要工作区级或其它根时**显式** `--out`。**显式**将 `--out` 与 `--source` 指到同一路径时告警。
2. 将 `CMakeLists.txt` 规范化为 **命令节点序列**（**语句级浅层 AST** / 命令流）：每条 `name( args )` 对应一节点，再对节点流做**静态重解释**（见 `02-physical/gz-reverse-cmake/spec.md`）——`project` / `set` / `include`（**内联**、环/重复检测）/ `add_subdirectory`（**环/重复** Listfile 检测）/ `add_executable` / `add_library` / `include_directories` / `target_sources` / `target_link_libraries` / `target_include_directories` / **`configure_file`（子集，映射为 GZ `<config_files>`）** 等。`set` 与按 `spec` **注入**的有限 `CMAKE_*` 目录名（`CMAKE_SOURCE_DIR` / `CMAKE_BINARY_DIR` / `CMAKE_CURRENT_BINARY_DIR` 等；其中 `CMAKE_BINARY_DIR` **恒**按与 **`gz configure`** 一致的 **`.intermediate/build/<叶>`** 自动估计（**无** CLI 覆盖，见 `spec.md`））共同参与 `${…}` 展开；**`if/elseif/else/endif`** 在**可判定**子集上**压平**后解释；**`macro` 多轮展开**子集；`function` 体与 `foreach`/`while` 仍用块深度**屏蔽**内部 `add_*` / `configure_file` 等；**`$<…>`** 为 **L6 尽力**子集。可选 **`--file-api`** 读入**用户**预置 JSON（L7，**不**在工具内跑 `cmake`）与静态结果**对照**。
3. 将已收集的绝对源路径、include 目录转为**相对**各 `target.xml` 目录的 portable 相对路径，写出 XML；`target_link_libraries` 中可判定的同包目标名写为 `<dependency>`。

## 与主系统（`gz`）的关系

- 输出格式以 `doc/zh/package-target-xml-spec.md` 与 `gz` 实现解析器为准。本工具对 **`<config_files>`** 提供 **`configure_file(…)` → `<file in= to=/>` 的受控子集**（见 `02-physical/gz-reverse-cmake/spec.md` 归属与 `to` 占位规则）；**不保证**与真实 CMake 输出路径/COPYONLY 等完全一致；**不**保证覆盖 `prebuilt_*`、细粒度 `when` 等，也不替代「纯手写工作流」。
- **实现**：C++ 标准库 + 自研脚本解析；**不**在工具内子进程执行 `cmake`；L7 下可用手写子集解析**用户**提供的 `codemodel` 类 JSON，**不**使用第三方 JSON 库（见 `02-physical/gz-reverse-cmake/mapping.md`、`spec.md` 之「L1–L7」与「与 File API 路线」）。

- **发布**：可执行由根 `CMakeLists.txt` 纳管，`install` 与 **`gz` / `gz-gui` 同分量 `gz_runtime`** 至 `bin/`（选项 B）。

## 与系统设计的边界

- 不进入 `gz configure` / `build` 内部管线；见 [system-design.md](system-design.md) 中主流程。本工具是**纯离线、无配置图**辅助，不在运行期参与。

## 覆盖度与扩展层次

- **逻辑层**不替代物理层字段级规格；L1–L7 能力表、非目标与 CMake 参考版本以 **`02-physical/gz-reverse-cmake/spec.md`** 为**单一事实来源**（与 `mapping.md` 交叉引用）。

