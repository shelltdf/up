# cmake_static_scan

用于验证 `up project` 在 **不运行 CMake File API**（`--cmake-no-file-api`）时，对 `CMakeLists.txt` 的启发式扫描能否：

- 发现 `add_library` / `add_executable` 与源文件
- 从 `target_link_libraries` 推断同包内目标依赖（`cg_bar` → `cg_foo`）
- 从 `target_include_directories` 与 `${CMAKE_CURRENT_SOURCE_DIR}` 推断 `<headers>`（`cg_foo`）

在包根目录执行（需已构建仓库内的 `up.exe`）：

```powershell
..\..\..\_build\Release\up.exe project --cmake-no-file-api --dry-run
```

若本机 CMake 与依赖齐全，可与 File API 结果对照：

```powershell
..\..\..\_build\Release\up.exe project --cmake-query --dry-run
```
