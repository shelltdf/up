# gz-cli 物理规格（行为级）

## 本文与 `doc/zh/cli-reference.md` 的分工

| 载体 | 职责 |
|------|------|
| **本 `spec.md`（物理层）** | 与**磁盘布局、缓存、安装树、可交付物形态**强绑定的行为与硬约束；**退出码仅保留摘要**（便于与逻辑/运维对齐）。**不**逐条维护 argv、子命令开关、范例命令行。 |
| **[`doc/zh/cli-reference.md`](../../../doc/zh/cli-reference.md)** | **`gz` argv 的单一事实来源**：子命令一览、**每个参数**、工作模式、`--verbose`/`GZ_VERBOSE`、`list` 的 stdout/文件导出与告警组合、**完整退出码表**、PowerShell 范例等。 |
| **[`mapping.md`](mapping.md)**（同目录） | **子命令 → 主实现源文件** 及零散符号映射（摘录）。 |

---

## 退出码（摘要）

- **`0`**：成功、或仅打印用法/help。
- **`1`**：未知子命令等一般错误。
- **`2`**：参数缺失/非法、缺少 `gz_cache.txt` 等可恢复错误。
- **`5`（摘要）**：**`configure`** 业务失败；其中 **`target.xml` `type`** 不在白名单时 stderr 含 **`unknown target type …`**（详表与**其它 5 的成因**以 [`doc/zh/cli-reference.md`](../../../doc/zh/cli-reference.md) §3 与实现为准）。

（**`list` 的 3/4/6** 等细码与其它子命令边界码：**以 [`doc/zh/cli-reference.md`](../../../doc/zh/cli-reference.md) §3 为准**。）

---

## 物理目录分层约束

- `GroundZero/exe/`：仅放 `gz.exe` 入口与参数分发代码（当前入口：`GroundZero/exe/main.cpp`，分派见 `cli_dispatch.cpp`）。
- `GroundZero/lib/`：仅放 `gz.lib` 实现代码（`engine/` 与 `infra/`）。
- `gz` CMake 目标通过 `target_link_libraries(gz PRIVATE gz-lib)` 组合入口与库实现。
- 新增命令/后端/基础能力时，默认落在 `GroundZero/lib/`；`GroundZero/exe/` 不承载业务实现。

---

## 硬约束（与实现对齐）

- **`build`**：必须提供 **`--build-dir-name`**；构建目录下必须存在 **`gz_cache.txt`**。默认在 **install 成功之后** 读取 **`gz_redist_manifest.json`**（**`schema` 3** 为**分字段**布局；**`schema` 1** 可有旧式 **`arch` 长串**并在读入时**反拆**为分字段；**`schema` 2/3** 自 **JSON** 读分字段）并在**安装根下** **`gz-redist/`** 中生成 **`package.xml` / `*/target.xml`**。生成的 **`target.xml`** 中 **`<prebuilt …/>`** 只带 **§3.1.1** 的 **`os` / `cpu` / `build_system` / `toolchain` / `link` / `config` / `crt`**（安装根目录名单段由 **`arch=`** / **`compose_arch_tag`** 与缓存表达，不重复在 void 属性中）。**`<prebuilt/>`** 上**仅**弃用 **`arch="…"` 单长串**可反拆。可用 **`--no-emit-redistribution-xml`** 或 **`GZ_EMIT_REDIST_XML`** 为假值关闭（详见 **`doc/zh/package-target-xml-spec.md` §8**）。
- **`run` / `test`**：必须提供 **`--install-dir-name`**；其值为 **安装树根在 `.intermediate/install/` 下的单段目录名**（通常等于缓存中的 **`arch`**）。
- **`pack`**：至少一次 **`--install-dir-name`**；可重复以打多架构包。
- **`pack`（当前行为补充）**：仅对给定 **安装树**做 **zip/tar.gz**（或 **CPack**）归档；**不**生成或改写 **`package.xml` / `target.xml`**。若 **`gz-redist/`** 已存在于安装树下（由 **`gz build` 默认 emit** 等路径生成），**`pack`** 会将其一并打进归档。

---

## DOM 快照文件（`list --xml` 可交付物形态）

- **`gz list --xml <path>`** 写出的 DOM 快照为 **UTF-8** XML，含 `<?xml ...?>` 头；**根元素为 `<gz_dom>`**。
- **`list` 的 CLI 开关组合、stdout 载荷、`--quiet` 与告警行为**：见 **[`doc/zh/cli-reference.md`](../../../doc/zh/cli-reference.md)** §10（不在此重复）。

---

## 安装路径计算（build）

`install_prefix = default_install_root(cwd) / arch`，其中 `arch` 优先取 `gz_cache.txt` 中的 **`arch=`** 行取值，否则由选项推导（见 `build.cpp`）。
