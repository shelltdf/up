# up-cli（CMake 目标 `up`）

| 项 | 值 |
|----|-----|
| 产物文件名 | `up`（Windows：`up.exe`） |
| 类型 | 可执行文件 |
| 源码根 | 仓库根 `src/`（入口 `src/cli/main.cpp`） |
| CMake 目标名 | `up` |

## 编译定义

- **`UP_ENABLE_PROJECT`**：由 CMake `option(UP_ENABLE_PROJECT ...)` 注入；为 `0` 时编译裁减 `project` 相关翻译单元与 `project` 子命令分支。
