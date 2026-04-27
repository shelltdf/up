# up-cli（CMake 目标 `up`）

| 项 | 值 |
|----|-----|
| 产物文件名 | `up`（Windows：`up.exe`） |
| 类型 | 可执行文件 |
| 源码根 | 仓库根 `src/`（入口 `src/exe/main.cpp`，库实现位于 `src/lib/`） |
| CMake 目标名 | `up` |

## 编译定义

- **`UP_ENABLE_REVERSE`**：由 CMake `option(UP_ENABLE_REVERSE ...)` 注入；为 `0` 时编译裁减逆向（`reverse`）相关翻译单元与子命令分支。

## 目录分层约束

- `src/exe/`：仅放 CLI 入口与参数分发（当前为 `src/exe/main.cpp`）。
- `src/lib/`：仅放 `up.lib` 的实现代码（`engine/` 与 `infra/`）。
- 业务能力（命令、后端、XML/DOM、平台能力）默认放在 `src/lib/`，避免回流到入口层。
