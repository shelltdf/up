# up-cli 物理规格（行为级）

## 退出码（摘要）

- **`0`**：成功、或仅打印用法/help。
- **`1`**：未知子命令等一般错误。
- **`2`**：参数缺失/非法、缺少 `up_cache.txt` 等可恢复错误。

## 子命令与实现源文件（主要）

| 子命令 | 实现 |
|--------|------|
| `configure` | `src/lib/engine/commands/configure.cpp` |
| `build` | `src/lib/engine/commands/build.cpp` |
| `run` | `src/lib/engine/commands/run.cpp` |
| `test` | `src/lib/engine/commands/test.cpp` |
| `pack` | `src/lib/engine/commands/pack.cpp` |
| `spec` | `src/lib/engine/commands/spec.cpp` |
| `list` | `src/lib/engine/commands/list.cpp` |
| `print-build-dir-name` | `src/exe/main.cpp` |

## 物理目录分层约束

- `src/exe/`：仅放 `up.exe` 入口与参数分发代码（当前入口：`src/exe/main.cpp`）。
- `src/lib/`：仅放 `up.lib` 实现代码（`engine/` 与 `infra/`）。
- `up` CMake 目标通过 `target_link_libraries(up PRIVATE up-lib)` 组合入口与库实现。
- 新增命令/后端/基础能力时，默认落在 `src/lib/`；`src/exe/` 不承载业务实现。

## 硬约束

- **`build`**：必须提供 **`--build-dir-name`**；构建目录下必须存在 **`up_cache.txt`**。
- **`run` / `test`**：必须提供 **`--install-dir-name`**；其值为 **安装树根在 `.intermediate/install/` 下的单段目录名**（通常等于缓存中的 **`arch`**）。
- **`pack`**：至少一次 **`--install-dir-name`**；可重复以打多架构包。

## `list` 参数与输出行为

- 标准入口：`up list [--format tree|json|xml] [--xml <path>] [--json <path>] [--quiet]`
- stdout 载荷由 `--format` 决定（默认 `tree`）。
- `--xml <path>` 与 `--json <path>` 为文件导出，可与任意 stdout 模式并存。
- `--quiet` 会抑制树形文本与“导出成功提示”，但不会抑制 `--format json|xml` 的主载荷。
- 以下组合仅告警，不中断执行：
  - `--format xml` + `--json <path>`
  - `--format json` + `--xml <path>`

## 安装路径计算（build）

`install_prefix = default_install_root(cwd) / arch`，其中 `arch` 优先取 `up_cache.txt` 中 `arch=`，否则由选项推导（见 `build.cpp`）。
