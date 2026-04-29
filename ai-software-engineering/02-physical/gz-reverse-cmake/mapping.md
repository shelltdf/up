# 模型/行为 → 源码位置（`gz_reverse_cmake`）

| 元素 / 行为 | 源码位置 |
|-------------|----------|
| 可执行入口、CLI、`package.xml`/`target.xml` 写出 | `gz_reverse_cmake/src/main.cpp` 中 `main` |
| CMake 脚本拆成 `command( args )` 序列（L1：路径/行号、`parse_notes`） | `gz_reverse_cmake/src/cmake_parse.cpp`、`cmake_parse.hpp` 中 `parse_cmake_listfile_text` 等 |
| `project`/`set`/`include` 内联/`add_subdirectory`/`add_*`/`target_*`/`configure_file` 子集解释 | `gz_reverse_cmake/src/cmake_interpret.cpp`、`cmake_interpret.hpp` 中 `process_listfile`、`include_inline_for_listfile`、`filter_if_flat`、`interpret_cmake_tree`；`configure_file` → `TargetModel::config_files` 或 `InterpretResult::package_config_files` → `target.xml` / `package.xml` 的 `<config_files>`（`main.cpp`） |
| `target_compile_options` / `target_link_options` / `set_target_properties(…, COMPILE_FLAGS, LINK_FLAGS)` | `cmake_interpret.cpp` 中 `collect_target_options_list` 等 → `TargetModel::compile_flags` / `link_flags` → `main.cpp` 写出小写 `<compile_flags>` / `<link_flags><arg>…` |
| `include` 环/重复、`add_subdirectory` 环/重复 | `include_inline_for_listfile` 内 `include_seen`；`process_listfile` 入口与递归共用 `subdir_visited`（规范化 Listfile 路径） |
| `$<…>` 生成器子集 | `gz_reverse_cmake/src/cmake_genex.cpp` / `cmake_genex.hpp`，由 `expc` 路径**尽力**调用 |
| L7 File API / codemodel JSON 子集摄入 | `gz_reverse_cmake/src/file_api_ingest.cpp` / `file_api_ingest.hpp`；CLI `--file-api` 见 `main.cpp`；结果写入 `InterpretResult::file_api_target_names` / `file_api_merge_notes` |
| GZ 约定下 `CMAKE_BINARY_DIR`（**仅**此来源、无 CLI 覆盖） | `cmake_interpret.cpp` 中 `infer_gz_default_cmake_binary_root`（对齐 `GroundZero/lib/infra/platform/paths.cpp` 的 `default_cmake_build_root` + 叶名 `default` 或 `…/build` 下唯一子目录） |
| 包名默认值 | `interpret_cmake_tree` 中首次 `project(…)` 首参 |
| 安装到 `bin/`、随 `gz_runtime` | 根 `CMakeLists.txt` 中 `add_subdirectory(gz_reverse_cmake)` + `gz_reverse_cmake/CMakeLists.txt` 中 `install(TARGETS gz_reverse_cmake … COMPONENT ${GZ_RUNTIME_COMPONENT})` |

## 与 CMake 官方文档的关系

- **不**在工具内作为**自动化** [cmake-file-api(7)](https://cmake.org/cmake/help/latest/manual/cmake-file-api.7.html) 客户端发查询；L7 仅**读入用户**已生成的**回复** JSON 子集作对照。行为以 [cmake-language(7)](https://cmake.org/cmake/help/latest/manual/cmake-language.7.html) 为**参考**，实现为**故意缩小**的可执行子集；以本仓库 `spec.md` 与源码为准。
