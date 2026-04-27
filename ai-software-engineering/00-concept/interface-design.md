# 接口设计（程序间）

> 不含 GUI 人机交互细节（见 `software-design.md` 与逻辑/物理阶段 GUI 文档）。

## CLI 调用形态

```
up [--verbose|-v] <subcommand> ...
```

- **`UP_VERBOSE`**：环境变量等价为开启 verbose（与 `--verbose` 一致）。
- **帮助**：`up --help` | `-h` | `help`。

## 子命令（对外行为摘要）

| 子命令 | 关键参数 | 说明 |
|--------|-----------|------|
| `configure` | `[--build-dir-name <leaf>]`、`[--scan <dir>]...`、`[--opt KEY=VALUE]...` | 生成构建树；默认 `<leaf>=default`。 |
| `build` | **`--build-dir-name <leaf>`**（必填） | 读取对应目录下 `up_cache.txt` 并执行后端构建与 install。 |
| `run` | **`--install-dir-name <name>`**、可执行目标名 | `<name>` 为 `.intermediate/install/` 下**直接子目录名**，与缓存中 **`arch`** 一致。 |
| `test` | **`--install-dir-name <name>`**、`[测试名]` | 基于安装树运行 CTest 等。 |
| `pack` | **`--install-dir-name <name>`**（可重复） | 将对应安装树打入归档。 |
| `spec` | 无 | 向 stdout 输出内嵌的英文 XML 规则说明（供工具/AI）。 |
| `print-build-dir-name` | `[--build-dir-name <leaf>]`、`[--opt ...]` | 打印当前配置对应的 **`arch`** 字符串（供脚本承接 `run`/`test`/`pack`）。 |
| `reverse` | 多选项（CMake 探测相关，逆向生成描述） | **仅当**构建时启用 **`UP_ENABLE_REVERSE=ON`**；否则报错并提示重编译。 |

## 路径与编码约束（接口契约）

- 含非 ASCII 的路径在 configure 阶段应报错并提示使用 ASCII 路径（与实现 `path_check` 一致）。
- 控制台在 Windows 上尽力使用 UTF-8 代码页输出文案。
