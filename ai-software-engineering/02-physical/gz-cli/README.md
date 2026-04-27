# gz-cli（CMake 目标 `gz`）

| 项 | 值 |
|----|-----|
| 产物文件名 | `gz`（Windows：`gz.exe`） |
| 类型 | 可执行文件 |
| 源码根 | 仓库根 `GroundZero/`（入口 `GroundZero/exe/main.cpp`，库实现位于 `GroundZero/lib/`） |
| CMake 目标名 | `gz` |

- **argv / 子命令逐项说明与范例**（实现侧、中文）：仓库 **`doc/zh/cli-reference.md`**（与 **`gz --help`** 对齐；**单一事实来源**）。
- **物理层行为**（目录、硬约束、`gz_dom`、退出码摘要）：本目录 **`spec.md`**。
- **子命令 → 源码**：本目录 **`mapping.md`**。

## 目录分层约束

- `GroundZero/exe/`：仅放 CLI 入口与参数分发（当前为 `GroundZero/exe/main.cpp`）。
- `GroundZero/lib/`：仅放 `gz.lib` 的实现代码（`engine/` 与 `infra/`）。
- 业务能力（命令、后端、XML/DOM、平台能力）默认放在 `GroundZero/lib/`，避免回流到入口层。
