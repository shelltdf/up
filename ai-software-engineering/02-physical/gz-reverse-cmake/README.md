# 构建目标 `gz_reverse_cmake`

- **产物文件名**：`gz_reverse_cmake`（Windows 为 `gz_reverse_cmake.exe`）
- **类型**：可执行文件
- **源码根**（相对仓库根）：`gz_reverse_cmake/`
- **CMake 目标名**：`gz_reverse_cmake`（见 `gz_reverse_cmake/CMakeLists.txt` 中 `add_executable`）

## 与根工程的关系

- 根 **`CMakeLists.txt`** 已 **`add_subdirectory(gz_reverse_cmake)`**，与 **`gz`** / **`gz-gui`** 同库一次配置、一次构建（**`build.py`** 显式 `--target` 三者）。
- **安装**：`install(TARGETS … RUNTIME DESTINATION bin COMPONENT ${GZ_RUNTIME_COMPONENT})`，与 **`install.py` 默认的 `gz_runtime`** 一致，**随 `gz` / `gz-gui` 一同安装**到前缀 **`bin/`**（**选项 B：默认运行时分量**）。

仍可在仅检出该子目录时**单独**配置（用于最小复现；**仅** C++17 标准库，**无**第三方依赖、**无**外网拉取）：

```text
cmake -S gz_reverse_cmake -B gz_reverse_cmake/build
cmake --build gz_reverse_cmake/build
```

- **行为与约束**：见同目录 [spec.md](spec.md)
- **组件级结构**：见 [uml-component.md](uml-component.md)
- **实现映射**：见 [mapping.md](mapping.md)
