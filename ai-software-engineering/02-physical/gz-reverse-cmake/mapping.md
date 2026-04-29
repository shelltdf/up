# 模型/行为 → 源码位置（`gz_reverse_cmake`）

| 元素 / 行为 | 源码位置 |
|-------------|----------|
| 可执行入口、CLI、`package.xml`/`target.xml` 写出 | `gz_reverse_cmake/src/main.cpp` 中 `main` |
| CMake 脚本拆成 `command( args )` 序列 | `gz_reverse_cmake/src/cmake_parse.cpp`、`cmake_parse.hpp` |
| `project`/`set`/`add_subdirectory`/`add_*`/`target_*` 子集解释 | `gz_reverse_cmake/src/cmake_interpret.cpp`、`cmake_interpret.hpp` 中 `process_listfile`、`interpret_cmake_tree` |
| 包名默认值 | `interpret_cmake_tree` 中首次 `project(…)` 首参 |
| 安装到 `bin/`、随 `gz_runtime` | 根 `CMakeLists.txt` 中 `add_subdirectory(gz_reverse_cmake)` + `gz_reverse_cmake/CMakeLists.txt` 中 `install(TARGETS gz_reverse_cmake … COMPONENT ${GZ_RUNTIME_COMPONENT})` |

## 与 CMake 官方文档的关系

- **不**使用 [cmake-file-api(7)](https://cmake.org/cmake/help/latest/manual/cmake-file-api.7.html)。行为以 [cmake-language(7)](https://cmake.org/cmake/help/latest/manual/cmake-language.7.html) 为**参考**，实现为**故意缩小**的可执行子集；以本仓库 `spec.md` 与源码为准。
