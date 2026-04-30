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

## CMake 后端（`configure` 写出的 `CMakeLists.txt`）

- **实现结构（防再循环）**：`write_cmake_lists` 仅负责组装；**生成**按阶段拆成 **`append_preamble`**、**`append_prebuilt_imported`**、**`append_native_libraries`**、**`append_executables`**、**`append_test_block`**、**`append_install`**，预置库链接索引 **`PrebuiltLinkIndex`** 每文件**构建一次**后传入链入阶段。策略见下条。代码见 `GroundZero/lib/engine/backends/cmake/cmake_backend.cpp`。
- **路径策略（写表时 CWD = 包根）**：凡可相对 **`build_root`**（= 生成 `CMakeLists` 的目录 = `cmake -S` = **`CMAKE_SOURCE_DIR`**）的路径，在 **`add_library` / `add_executable` 源列表**、**`target_include_*`**、**`IMPORTED_*` 属性**、**`install(…)` 与自定义命令的源/依赖** 等处一律写 **`"${CMAKE_SOURCE_DIR}/…"`**；仅当**无法**相对（如**跨盘**）时回退**绝对**路径。`cmake_escape_string_value` 仍用于转义。`install(… DESTINATION <段>)` 仍是**安装树内**布局名，非主机绝对。
- **构建树 vs 安装树（与 LNK1181 的「一次性」结论）**：**不要**在生成列表里对**构建**设全局 **`CMAKE_ARCHIVE_OUTPUT_DIRECTORY` → `…/lib`**（及配套的 **bin/ + 全局** `CMAKE_*_OUTPUT_DIRECTORY`）。该组合在 **MSVC/MSBuild** 下易使同包 **`target_link_libraries(…, 目标名)`** 变成**错误**的相对 **`lib\Release\…\.lib`**（**LNK1181**），与绝对路径、**TFL**、**`target_link_directories` + 文件名** 等修补**反复横跳**仍不稳。**策略**：**构建**沿用 CMake 默认的各目标输出；**`install(…)`** 仍写 **`ARCHIVE` / `LIBRARY` → `lib/`**、**`RUNTIME` → `bin/`**（**安装**树仍整齐）。同包**原生**库在 **`target_link_libraries`** 中**只写裸目标名**；**IMPORTED** `prebuilt_*` 规则不变。**`install(TARGETS …)` 将 `static_library` 与 `shared_library` 分条写出**；**`WIN32` 上**共享库只写 **`RUNTIME → bin/`** 与 **`ARCHIVE → lib/`**（**不要**对 Windows DLL 再写 **`LIBRARY`——CMake 中 DLL 属 `RUNTIME`、导入 `.lib` 属 `ARCHIVE`，混写易**只装到 .dll 而漏装导入 .lib**）。非 Windows 的 **`.so` 等** 用 **`LIBRARY → lib/`**。**不要**把静/动混在**同一条** `install(TARGETS` 里；**不要**用 **`install(FILES` + `TARGET_LINKER_FILE`**。问题**不是**「没在 **zlib** / **zlibstatic** 之间自动二选一」。`emit` 对 **`lib/Release/…` 等** 见 `redist_emit.cpp`。
- **库**目标会经 `target_include_directories(... PUBLIC ...)` 带上 **包内公共源根**；可执行在 **`PRIVATE` 链**上应拿到库的 `PUBLIC` include。生成逻辑**仍**为可执行目标**追加**同一包源根 **`PRIVATE` include`（**兜底** `test/` 下源与边缘组合）。实现同文件。
- **静态链（`GZ_TARGET_DYNAMIC_LIBRARY=OFF`）与显式 `dependency` 名**：若可执行在 **target.xml** 中依赖了 **`shared_library` 名**（如 `zlib`）而同包内存在**同名+`static` 的 `static_library`**（如 `zlibstatic`），**`configure` 在生成 `target_link_libraries` 时把链接重映射为后者**，避免在 MSVC 下仍对 **导入库** 求 **相对 `Release\zlib.lib`** 引发 **LNK1181**；动态链（`ON`）不改名。见 `configure.cpp`。
- **`add_test`**：`COMMAND` 写**可执行目标名**（如 `COMMAND example`），**不**用 **`$<TARGET_FILE:…>`**；CMake 对**同项目**内 `add_test` 的 `COMMAND` 首项为**目标**时，会解析为**可执行文件**路径。实现同文件。

---

## 硬约束（与实现对齐）

- **`build`**：必须提供 **`--build-dir-name`**；构建目录下必须存在 **`gz_cache.txt`**。**`configure` 写 `gz_redist_manifest.json` 时**：若 **`GZ_TARGET_DYNAMIC_LIBRARY=OFF`（`link=static`）** 且同包内存在 **`static_library` / `library`**，则**不**把并存的 **`shared_library`** 写入 manifest（避免仍按 DLL+导入库做 `emit` 校验）。仅含 **shared** 的包仍写入。默认在 **install 成功之后** 读取 **`gz_redist_manifest.json`**（**`schema` 3** 为**分字段**布局；**`schema` 1** 可有旧式 **`arch` 长串**并在读入时**反拆**为分字段；**`schema` 2/3** 自 **JSON** 读分字段）并在**安装根下** **`gz-redist/`** 中生成 **`package.xml` / `*/target.xml`**。生成的 **`target.xml`** 中 **`<prebuilt …/>`** 只带 **§3.1.1** 的 **`os` / `cpu` / `build_system` / `toolchain` / `link` / `config` / `crt`**（安装根目录名单段由 **`arch=`** / **`compose_arch_tag`** 与缓存表达，不重复在 void 属性中）。**`<prebuilt/>`** 上**仅**弃用 **`arch="…"` 单长串**可反拆。可用 **`--no-emit-redistribution-xml`** 或 **`GZ_EMIT_REDIST_XML`** 为假值关闭（详见 **`doc/zh/package-target-xml-spec.md` §8**）。
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

---

## 内嵌 Lua 与 `gz.file`（`configure` 阶段）

**单一事实来源（与实现对齐）**：**行为与字段级约定**以仓库内嵌 **`gz spec`**（`GroundZero/lib/engine/commands/spec.cpp` 中 **`GZ_XML_SPEC_REVISION`**，含 **§4 Scripts** 表）与 **`doc/en|zh/package-target-xml-spec.md`** 为准；本小节只作物理层**索引**。

- **运行时**：`gz` 链入 **`GroundZero/lib`** → **`lz_embed`** 静态库，源码为 **[`3rdparty/lua-5.5.0`](../../../3rdparty/lua-5.5.0)** 官方 `src/*.c`（**不含** `lua.c` / `luac.c` 独立可执行），**不**使用系统 `find_package(Lua)` 或外网拉取。
- **触发**：`package.xml` / `target.xml` 中 **`<var name="…" type="script" script_type="lua" trigger="configure" value="…"/>`**；**`value`** 为 **Lua 源码**（与标量 `var` 的 `value` 一样经 XML 进入 DOM 的 **`ScriptValue::source` 串**）。在 **`run_configure`** 内、**写出 CMake/Ninja 后端之前**，对 DOM 中**每个包（含子包）与目标**上满足条件的 `var` 调用 **`run_gz_embedded_configure_lua`**（见 `configure.cpp`）；失败则 **`configure` 非零退出**。
- **API**：全局表 **`gz.file`**：`read` / `write` / `append`；路径须在 **工作区、build 根、包目录、`.intermediate/generated/<arch>`** 的允许并集内（实现：`GroundZero/lib/engine/lua/gz_embedded_lua.*`）。全局 **`GZ`** 为字符串表（如 **`GZ_WORKSPACE`**、**`GZ_BUILD_ROOT`**、**`GZ_PACKAGE`**、**`GZ_ARCH`**）。
- **与 shell 回退的边界**：**`trigger=configure`** **以外**的已支持 trigger，在与 **空** `preprocess`/`postprocess` **`command`** 配对时，**`value` 仍按整行 shell 命令**使用（**`resolve_script_command`** 与既有 Ninja/CMake **自定义命令** 生成链不变）。见内嵌 `gz spec` §4。

### 与 `gz_reverse_cmake`（逆向）的分工

- **正向（本目标 `gz` + `gz configure`）**：类 CMake **`file(READ|WRITE|…)`** 的**可维护替代**是 **`trigger=configure` + Lua + `gz.file`**；权威见上。
- **逆向（[gz-reverse-cmake](gz_reverse_cmake)）**：**不**在工具内跑 CMake、**不**保证执行 `string(REGEX …)` / `foreach` 等全链条；**仅**静态登记 **`file(WRITE …)` 输出路径** 与 `source_path_for_gz_remap` 等，并可选 **stderr 注记** 与 **`scripts/gz_cmake_file_stub.lua`** 占位。细节见 **[`gz-reverse-cmake/spec.md`](../gz-reverse-cmake/spec.md)**。
