# 物理规格：`gz_reverse_cmake`

## 角色

在**不修改**被扫描的 CMake 源码树的前提下，**不运行** `cmake` 配置，对顶层及 `add_subdirectory` 可达的 **`CMakeLists.txt`** 做**静态、可解释子集**的解析，在指定输出根目录下生成与 GroundZero 约定一致的 `package.xml` 与各 `target.xml` 初稿，供 `gz configure` 消费或人工精修。

**与正向（`gz` / 内嵌 Lua）的关系**：在 **`ai-software-engineering/02-physical/`** 内，**类 CMake `file` 的可执行替代**以 **[`gz-cli/spec.md`](../gz-cli/spec.md)** 中 **「内嵌 Lua 与 `gz.file`（`configure` 阶段）」** 为**正向**权威；**本目录 `spec.md`** 负责**逆向**静态子集与诚实边界（登记路径、**不**模拟全文生成逻辑）。

## 输入

- `--source`：含顶层 `CMakeLists.txt` 的源目录（**只读**扫描）。**省略**时默认为**当前工作目录**（先 `cd` 到该根，可无参执行）。
- `--out`：输出根，其下将创建 **`<包名>/package.xml`** 与 **`<包名>/<目标名>/target.xml`**。**省略**时默认为 **`<--source>/gz_reverse/`**（与 `CMakeLists.txt` 同根、子目录名固定，每个第三方库自管一份反解结果）。要写到**其它**根（如工作区级目录），在命令行**显式**传 `--out`。
- **路径重叠**：当**显式**的 `--out` 与 `--source` 为**同一路径**时**警告**；**默认**的 `<--source>/gz_reverse/` 不警告。
- **隐式需求**：无。不要求本机已安装或能成功运行 `cmake` 配置；**仅**读取与解释 `.txt` 脚本。
- **`--file-api`（L7，可选）**：**用户**在本机**外**事先运行 CMake 并生成的 File API / `codemodel-*.json` 等**单文件**路径。工具在**不**子进程执行 `cmake` 的前提下，用手写子集解析器抽取 **target 名**等，与静态反解结果**对照**；解析失败或缺键时**降级**为注记，不阻退出码。**不**与 configure 的同一时刻作比特级保证（用户负责产物与 `--source` 的对应关系）。
- **解析进度（stderr）**：默认在 **stderr** 用**回车覆写同一行**；**每进入一个新** `CMakeLists.txt` 有文件级行；**同一 Listfile 内**另有阶段/条命令进度：**读入**、**词法/解析(字节)**（`m/n` 为**已扫字节/文件字节长**；在**单条大命令**的**配平** `skip_balanced_parens` 中: 单下标、**Mode** ∈ {Normal, DQuote, 行内`#`, Bracket[=[) } 显式转移, 原 `skip_quoted`/`skip_bracket`/`#` 行 已内联; 各态内**每字**可 `tick`, 实际刷新见 `emit_with_span`；**括号体切分** `split_cmake_arguments` 的节流**另见下**; 且对括号体**仅下标推进**、**不在**循环里 `substr` 整段后缀, 避免大 `add_library` 等 **O(n²)** 拷贝致长时间假死;`emit_with_span`：Listfile 总长 **≤64KiB** 时约**每 48 已扫字节**发一帧(避免 7K 级仍用 2KiB 档导致**长时间**同一 `m/总`)；更大文件为 **2KiB/近尾 64** 且**剩余到 EOF 小于当前档**时缩档, 防「永远差一段」. **切分** `lastp`：`emit_during_split` 小 Listfile 约 **48** 步; 热区用 `maybe_report_split`（约 64/256 体字节) + 实参段末 `split_checkpoint`, **禁止**细步进+每字回调(会数千次全行 `stderr`/`flush`、Windows 上**极慢**、似卡死). 长双引号须 `one.reserve(…)` 保 **O(n)**；大 Listfile 热区仍**按位**降频后 **256** 步进(若每字进 `on_pos` 会过密). 段末 `end_abs` + `absorb_at_least` 与全局 `last` 对齐（**不**将切分再映射为「配平后 last…」的**合成**区间, 减少 stderr/进度在「切分 string」附近似停滞）；**同一行** 上词法/解析(字节) 的提示可带 **1-based 行号** 与**子状态** `subphase`（如 配平 `name` 的 `()` 体、方括号 `[[…]]`、切分实参、扫描起首、解析完成等，实现见 `CmakeParseProgress` 回调）。**内联/解析(字节)**（`include()` 拉入子文件时同理，亦带行号/子状态）、**include 内联**（多轮, 1/32 形式）、**宏展开**（多轮, 1/32）、**if 压平**（对**压平前**整段命令流逐条 `if/elseif/…`, `m/n` 为**命令序/条数**、大文件步进）、**预扫** / **主扫**（压平**后**两遍, 按**命令条数** `m/n`）。实现侧：对**非** CMake `[[`/`[=[` 的孤立 `[` 在扫描中**必前进 1 字节**, 避免与 `skip_bracket` 不推进组合成**死循环**（L1 子集、与真 CMake 对非法脚本的错误处理不必一致）。**`--no-progress`** 可关闭，便于脚本重定向。

## 过程（实现策略）

1. **结构表示（与「AST」的关系）**：不构建 CMake 的完整语法树（不解析表达式/生成器/条件体含义），而将每个 `CMakeLists.txt` 规约为 **有序命令节点序列**（`identifier(实参…)` 一条一节点，文档中称 **Listfile 命令流 / 语句级浅层 AST**），再在其上做离线重解释，等价于「在受限指令集上重做语义」而非运行 CMake。
2. **重解释**（非完整 CMake 语义）：
   - `project`：取包名（首参，经简单清洗）；
   - `set`：维护 `${VAR}` 展开表（`;` 列表合并为单串，与 `target.xml` 中多 `<file>` 展开在实现中处理）；
   - **CMake 目录内置变量**（**非** `cmake` 可执行、仅为静态估计）：**每个** `CMakeLists.txt` 进入时向同一展开表**注入** `CMAKE_SOURCE_DIR`、`CMAKE_BINARY_DIR`、`CMAKE_CURRENT_SOURCE_DIR`、`CMAKE_CURRENT_LIST_DIR`、`CMAKE_CURRENT_BINARY_DIR`（`add_subdirectory` 出口恢复），以展开常见 `configure_file`/`set(…${CMAKE_…}…)` 写法。`CMAKE_BINARY_DIR` **恒**按 GroundZero 与 `gz configure` 一致**自动**取 **`<顶层源根>/.intermediate/build/<叶>`**（实现为 `infer_gz_default_cmake_binary_root`）：叶名**优先** `default`（若该目录已存在）；否则若 `…/build/` 下**仅有一个**子目录则取该名；再否则仍按 **`default`** 路径作估计（与 **cli-reference** 中「构建叶子」约定一致，见 `GroundZero/lib/infra/platform/paths.cpp` 的 `default_cmake_build_root` + `default`）。**无**单独 CLI 覆盖。`CMAKE_CURRENT_BINARY_DIR` 为 `CMAKE_BINARY_DIR` 下**相对** `CMAKE_SOURCE_DIR` 与当前 Listfile 目录的镜像子路径。**不**提供完整 CMake 内置名表（如 `CMAKE_CURRENT_LIST_FILE` 等需另有需求再扩）。
   - `include_directories`：与自父链继承的 include 路径合并，供同目录下目标使用；
   - `add_subdirectory`：递归处理子目录 `CMakeLists.txt`；
   - `add_executable` / `add_library(STATIC/SHARED/MODULE…)`：登记目标与源；允许无源占位（与后续 `target_sources` 组合，贴近现代 CMake 写法）；
   - **`add_custom_target( name …)`**（`block==0`）：登记 **`kind=custom_target`**，**不**演算 `COMMAND`；**`SOURCES`** 后路径与 **`DEPENDS`** 中**带 `/` 的**项按读入/依赖记 **`FileReadEntry`（`out_var`=`depends`）**；**`DEPENDS`** 中**无** `/` 且非 `::` 的记 **`link_to`**。反解 `target.xml` 的 **`type=custom_target`** 可无 `<sources>`；**全空**（无源、无依赖、无 `file`/`string` 注记等）的 `add_custom_target` **不**导出以减噪。
   - `target_sources`：向已存在目标**追加**源；`IMPORTED`/`INTERFACE`/`ALIAS`/`OBJECT` 等仍按实现跳过或不出现在可编译目标集；
   - `target_link_libraries` / `add_dependencies` / `target_include_directories`：在已登记目标上**链接/构建序**依赖名与头路径；`add_dependencies` 与 `target_link_libraries` 的**目标名**均并入 **`link_to`**，在 `target.xml` 中**均**映为 **`<dependency name="…"/>`**（不区分链接与仅构建序，属反解诚实约定）；跳过多数关键字与 `$<…>` 生成式片段。
   - `target_compile_options` / `target_link_options` / `set_target_properties(… PROPERTIES COMPILE_FLAGS|LINK_FLAGS …)`：在已登记目标上追加**小写** `target.xml` 的 `<compile_flags>` / `<link_flags>`（子元素为 `<arg>` 一段一 token）；`$<…>` 不展开则跳过。
   - **`configure_file`**：在**同层** `function`/`macro` 外（`block==0`）解析**传统**两位置实参或 **`INPUT` / `OUTPUT` 关键字**对；`COPYONLY` / `ESCAPE_QUOTES` / `@ONLY` / `NEWLINE_STYLE` 等**仅**在识别时忽略，与真实 CMake 的**逐条语义**可能不同，结果仍按 GZ **`<config_files><file in=… to=…/></config_files>`** 写出。未展开 **`$<…>`** 的实参**跳过**并在 `InterpretResult::errors` 中记一条（含 Listfile 行号）。**归属启发式**（v1）：记录同 Listfile 中最近一次成功登记的 `add_executable` / `add_library` 名；`configure_file` 若出现于此之后则写入**该** `target.xml`；若此前尚无任何 `add_*` 则写入 **`package.xml`** 包级 `config_files`。**`to` 路径**（与 **GZ** `package-target-xml-spec` 一致）：**相对** **`.intermediate/generated/<arch 段>/<包名>/_package/`** 或 **`.intermediate/generated/<arch 段>/<包名>/<目标名>/`**（**`<arch 段>`** 与本次 `gz configure` 的 **`compose_arch_tag` / `gz_cache.txt` 的 `arch=`** 一致，用于区分多平台/多配置/多叶；`gz_reverse_cmake` 从 **`<--source>/.intermediate/build/<叶>/gz_cache.txt`** 读取 `arch=`，无则回退为 **build 叶目录名**；见 `infer_gz_generated_arch_segment`），**不**将 CMake 的 **`.intermediate/build/<叶>/...`** 整段写入 `to`；反解**默认**只写**输出文件名**（如 `zlib.pc`、`zconf.h`）。需要子目录或改名时**手改**；**不**在 XML 里用变量——占位符在**模板 `in` 文件**和 **`<vars>`** 的 `@NAME@` / `${NAME}` 中由 `gz configure` 处理，**不是**在 `to=` 属性里做变量展开。
   - **`file(WRITE|APPEND path …)`**、**`add_custom_command(OUTPUT a [b…] [BYPRODUCTS b1 [b2…]] [COMMAND …] [DEPENDS d1 [d2…]] …)`**（`block==0`，`TARGET` 表形不解析 OUTPUT、略过）：**只**对输出 / 副产品路径作 `${}` 可展开之登记；`OUTPUT` 后遇 **`byproducts` 子段**时登记至 **`file_write_outputs_abs`** 与主输出相同；**`depends` 段**内路径登记为 **`FileReadEntry`** 且 `out_var` 固定为 **`"depends"`**（与 `file(READ)` 区分）。段界仍依 **`command` / `byproducts` / `depends` 等**关键词**尽力**切分（路径与关键字**同名**未加引号时或误判，属诚实边界）。**不**演算 **`COMMAND`**。其余同下 **`file(READ)`** 与 GZ/ **`*_filegen.lua`**；**`file(WRITE …)`** 后内容实参**不**执行。
   - **`file(READ path out_var …)`**（`block==0`）：**只**对读入路径与输出变量名做 `${}` 可展开之登记，路径解算同 **`file(WRITE)`**（`weakly_canonical` 等），不读盘、不执行 `HEX`/`LIMIT` 等副作用；**归属**用与 `file(WRITE)` 相同之 **`last_tname`** 启发式。登记落在 **`TargetModel::file_read_entries`（`FileReadEntry`：path + out_var）** 或 **`package_file_read_entries`**。
   - **`string(REGEX|APPEND|REPLACE|CONCAT …)`**（解释器之 `block==0`）、**顶层 `foreach(…)` / `while(…)`**（对 `foreach`/`while` 恰在 `block` 由 0 变为 1 之**一次**记录，嵌套或体内部之 `block>=2` 不记）：**不**执行、不维护变量内容；将**命令名 + 实参**截断后写入 **`CmakeFilegenNote`**，按与 **`file(READ/WRITE)`** 相同之 **`last_tname`** 归入 **目标**或**包级**（`package_filegen_cmake_notes`），供 **`scripts/*_filegen.lua`** 作「弱注记」；**循环/函数体**内命令不记，与真 CMake 不同属预期。
3. **控制与展开（非完整 CMake）**：
   - **`include()`**（`block==0`）：将目标文件**内联**进当前命令流（**多轮**展开）；同一规范化路径**再次**被 `include` 时视为**环或重复**并**跳过**（`parse_diagnostics` 记一条，保留外层的 `include` 节点不展开）。
   - **`add_subdirectory`**：递归子目录 `CMakeLists.txt`；全树共享**已处理 Listfile 集**，同一规范化路径**再次**进入时视为**环或重复**并**整文件跳过**（`parse_diagnostics` 记一条）。**不**在工具内运行 `cmake`。
   - **`if`/`elseif`/`else`/`endif`（压平后）**：对**可判定**的常量/简单 `STREQUAL` / `DEFINED` / `EXISTS` / `AND` / `OR` / `NOT` 等**子集**在解释前**筛掉**未选分支；分支内**顶层**的 `set`/`unset` 在压平前可参与条件中的变量。复杂或与实现不符的 `if` 求值**未定义**，仍可能**两分支**均保留（与 CMake 可能不一致，见「重要边界」）。
   - **`macro`/`function` 定义**从主命令流**剥离**；**`macro` 名**的调用点做**多轮**参数代入展开（**尽力**与 CMake 接近，非全量）。**`function` 体**与 **`foreach`/`while`** 内部仍用**块深度**屏蔽本工具关心的 `add_*` 等（不模拟函数/循环体完整实例化与返回语义）。
   - **生成器表达式 `$<…>`**（L6 子集）：在若干 `expc` 后路径上调用 **尽力**展开（如 `$<BOOL:…>` 等），**不**与任意 CMake 版本**完全**同义。
4. 写出 `package.xml` 与各 `target.xml`；路径均为**相对**对应 `package.xml` / `target.xml` 所在目录的 portable 形式。**`<sources>`** 中：若某条路径与已写入 **`package.xml` / 本 `target.xml` 的 `config_files`** 的 `configure_file` 输出**为同一文件**，则**不再**在 `<sources>`/`<headers>` 中重复列出（`gz configure` 会将包级/目标级 `config_files` 生成物**自动并入**各目标编译源，与 `package-target-xml-spec` 一致，避免与多 arch/多叶下重复维护同一生成项）。**其余**仍落在 **`.intermediate/build/<叶>/`** 下、且**未**与已声明 `config_files` 对齐的路径，反解时改写为 **`.intermediate/generated/<arch 段>/…`** 的对应相对路径（`arch` 同上前）；**不**在 XML 里用「变量」代 build 根。若存在 **`<config_files>`**（由 `configure_file` 逆出），在 **`<headers>`** 之后、**`<sources>`** 之前写出。若同时存在 **`<headers>`** 与 **`<sources>`**，**默认**先写 **`<headers>`**，再写 **`<config_files>`**（若有），再写 **`<sources>`**（与 `target` 子元素常见阅读顺序「对外头 → 生成配置 → 源」一致；`gz` 合并解析不依赖子块物理顺序，见 `package-target-xml-spec` §1.1）。
5. **`target.xml` 中 `<sources>` 与 `<headers>`**（与 `package-target-xml-spec` 中「头文件**安装**」一致，**不**把编译期 `-I` 混成安装块）：
   - 含未展开 ``${...}`` 的源路径**不**写出，并**注**至 stderr，由用户按真实 configure 手补。
   - **不**将 `include_directories` / `target_include_directories` 的目录列成 `<headers><dir from=…/>`；实现仍可在解释阶段用其信息，**默认**不在 XML 中反映为「待安装头」的 `<dir>`。
   - **`<sources><file>`**：常见编译扩展名（如 `.c`、`.cpp` 等）及**未**判为「对外安装」的头文件；其余头（如 `deflate.h`）归此块，作为与实现同列的**私有/实现头**初稿。
   - **`<headers>`**：仅对**可判为 public 安装**的头使用 **`<file from="…"/>`**（启发式：相对 `--source` 路径中**出现**名为 `include` 的**路径段**，或**无** `include/` 时头文件主名与 `project` 导出的包名同 stem、且扩展名为头文件，如 `zlib` → `zlib.h`）。**不**解析 `install(FILES|DIRECTORY|…)` 时，与真实安装集可能不一致（例：`zconf.h` 常需 configure 后手改补入）。

## 与 File API 路线的关系（L7 摄入）

- **不**在工具内作为子进程调用本机 `cmake`；**不**自动向 `build/` 发 File API 查询。
- **可**经 **`--file-api <path>`** 读入**用户**提供的、已生成的 File API 风格 JSON 回复（以 **`codemodel-*.json`** 一类为常见输入）；**仅**为**无第三方**依赖的**手撸** JSON 子集解析（不依赖 `nlohmann/json` 等；支持的键/形状以本文件与 `mapping.md` 为准，随实现迭代）。用途：与静态反解的 target 名等**交叉对照**、stderr **注记**；**不**用该 JSON 替代静态扫描为主路径。

## 扩展层次 L1–L7（与实现的对应）

| 层 | 含义 | 本仓库落点/说明 |
|----|------|-----------------|
| L1 | Listfile 词法 + 命令流 | `cmake_parse.*`；每命令含路径/行号/未知名诊断 |
| L2 | 变量、注入 `CMAKE_*` 等 | `cmake_interpret.*`；非完整 `PARENT_SCOPE`/`CACHE` |
| L3 | `if` 压平子集 | `filter_if_flat` |
| L4 | `include` 内联 + `add_subdirectory` 图 | `include_inline_for_listfile` + `subdir_visited` 环/重复 |
| L5 | `function`/`macro` 子集 | `remove_macro_function_defs` / `expand_macro_repeatedly` |
| L6 | `$<…>` 子集 | `cmake_genex.*` |
| L7 | 预置 File API JSON 摄入 | `file_api_ingest.*`、`--file-api`、**不**在工具内 configure |

**非目标（诚实边界）**：不跑 `cmake` 则**不**保证与**在线** configure 的任意版本**结果等价**；以**金测/文档化子集** + **L7 用户自证产物** 为主。CMake 参考大版本见实现注释或本文变更记录。

## 过滤与映射

- 跳过名称与常见伪目标同名的目标（如 `ALL_BUILD` 等，实现中大小写不敏感子集）；
- **可执行 / 静库 / 动库**（或模块作共享库）映射为 `target.xml` 的 `type=`：`executable`、`static_library`、`shared_library`；**`add_custom_target`** 映为 **`custom_target`**。正向 **`gz configure`**（CMake 后端）对 `type=custom_target` 生成 `add_custom_target` 与 `add_dependencies`（见 `gz spec` / `GroundZero/.../configure.cpp`）；Ninja 后端当前与 `asset_bundle` 类似**不**为该类目标生成编译边。
- 同包 `<dependency name="…">` 仅当链接名在本轮已登记目标名集合中；`::` 与明显路径/生成式不写字面条目。

## 重要边界

- **非**完整 CMake：宏/函数**全**语义、生成器表达式**全集**、`file(GLOB)` 与运行期、未覆盖的 `if` 模式、`PARENT_SCOPE`/`CACHE` 的完整行为、`add_subdirectory` **二次**进同一目录的 CMake 合法用法等，均**不保证**与真实 configure 一致；结果均为**初稿**；**`configure_file` 的 COPYONLY/条件输出**等与 GZ `config_files` 模板管线**非一一对应**。
- **L7**：JSON 与**当前** `--source` 扫描**可不同步**；对照结论**仅供人工**排查，不上升为强约束。
- 当无法计算相对路径（如跨盘符）时打印警告，可能与 `gz configure` 路径规则冲突，应手改或调整 `--out` 位置。
- **`file(WRITE …)` 等到 `CMAKE_CURRENT_BINARY_DIR` 的生成源**：静态反解可能将其映成相对 `target.xml` 的**源码树**路径；若该路径在仓库中尚不存在，正向 `gz configure` 会在 `add_library` 处失败。**烟测** `test_projects/gz_reverse_file_read_write_smoke` 在源码根提供与反解相对路径一致的 **`gen_frw.c`** 占位，便于对 `_gz_rev_frw_out` 做 `gz configure` 全链路验证；更精细的 `generated/…/to=` 自动映射属后续扩展，见实现与 GZ 规范。

## 分发与安装

- 与根工程一体构建时，可执行文件通过 **`install(..., COMPONENT gz_runtime)`** 安装到 **`bin/`**；与 **`gz`**、**`gz-gui`** 同属默认 `python install.py` 的运行时分量。

## 错误与退出

- 常见失败：无顶层 `CMakeLists.txt`、解析后无可用目标。工具向 stderr 输出说明并以非零退出（与 `gz` 主程序退出码表无强制对齐）。
