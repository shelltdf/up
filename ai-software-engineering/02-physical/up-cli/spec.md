# up-cli 物理规格（行为级）

## 退出码（摘要）

- **`0`**：成功、或仅打印用法/help。
- **`1`**：未知子命令等一般错误。
- **`2`**：参数缺失/非法、缺少 `up_cache.txt`、`project` 被禁用等可恢复错误。

## 子命令与实现源文件（主要）

| 子命令 | 实现 |
|--------|------|
| `configure` | `src/engine/commands/configure.cpp` |
| `build` | `src/engine/commands/build.cpp` |
| `run` | `src/engine/commands/run.cpp` |
| `test` | `src/engine/commands/test.cpp` |
| `pack` | `src/engine/commands/pack.cpp` |
| `spec` | `src/engine/commands/spec.cpp` |
| `project` | `src/engine/commands/project.cpp` 等（`#if UP_ENABLE_PROJECT`） |
| `print-build-dir-name` | `src/cli/main.cpp` |

## 硬约束

- **`build`**：必须提供 **`--build-dir-name`**；构建目录下必须存在 **`up_cache.txt`**。
- **`run` / `test`**：必须提供 **`--install-dir-name`**；其值为 **安装树根在 `.intermediate/install/` 下的单段目录名**（通常等于缓存中的 **`arch`**）。
- **`pack`**：至少一次 **`--install-dir-name`**；可重复以打多架构包。

## 安装路径计算（build）

`install_prefix = default_install_root(cwd) / arch`，其中 `arch` 优先取 `up_cache.txt` 中 `arch=`，否则由选项推导（见 `build.cpp`）。
