# 物理规格：`gz_reverse_cmake`

## 角色

在**不修改**被扫描的 CMake 源码树的前提下，**不运行** `cmake` 配置，对顶层及 `add_subdirectory` 可达的 **`CMakeLists.txt`** 做**静态、可解释子集**的解析，在指定输出根目录下生成与 GroundZero 约定一致的 `package.xml` 与各 `target.xml` 初稿，供 `gz configure` 消费或人工精修。

## 输入

- `--source`：含顶层 `CMakeLists.txt` 的源目录（**只读**扫描）。**省略**时默认为**当前工作目录**（先 `cd` 到该根，可无参执行）。
- `--out`：输出根，其下将创建 **`<包名>/package.xml`** 与 **`<包名>/<目标名>/target.xml`**。**省略**时默认为 **`<--source>/gz_reverse/`**（与 `CMakeLists.txt` 同根、子目录名固定，每个第三方库自管一份反解结果）。要写到**其它**根（如工作区级目录），在命令行**显式**传 `--out`。
- **路径重叠**：当**显式**的 `--out` 与 `--source` 为**同一路径**时**警告**；**默认**的 `<--source>/gz_reverse/` 不警告。
- **隐式需求**：无。不要求本机已安装或能成功运行 `cmake` 配置；**仅**读取与解释 `.txt` 脚本。

## 过程（实现策略）

1. **结构表示（与「AST」的关系）**：不构建 CMake 的完整语法树（不解析表达式/生成器/条件体含义），而将每个 `CMakeLists.txt` 规约为 **有序命令节点序列**（`identifier(实参…)` 一条一节点，文档中称 **Listfile 命令流 / 语句级浅层 AST**），再在其上做离线重解释，等价于「在受限指令集上重做语义」而非运行 CMake。
2. **重解释**（非完整 CMake 语义）：
   - `project`：取包名（首参，经简单清洗）；
   - `set`：维护 `${VAR}` 展开表（`;` 列表合并为单串，与 `target.xml` 中多 `<file>` 展开在实现中处理）；
   - `include_directories`：与自父链继承的 include 路径合并，供同目录下目标使用；
   - `add_subdirectory`：递归处理子目录 `CMakeLists.txt`；
   - `add_executable` / `add_library(STATIC/SHARED/MODULE…)`：登记目标与源；允许无源占位（与后续 `target_sources` 组合，贴近现代 CMake 写法）；
   - `target_sources`：向已存在目标**追加**源；`IMPORTED`/`INTERFACE`/`ALIAS`/`OBJECT` 等仍按实现跳过或不出现在可编译目标集；
   - `target_link_libraries` / `target_include_directories`：在已登记目标上追加依赖与头路径（跳过多数关键字与 `$<…>` 生成式片段）。
3. `function`/`macro`/`foreach`/`while` 块内：用块深度屏蔽 `add_*` / `add_subdirectory` 等，**不**模拟宏展开与循环实例化；块外 `if` 不建执行流，**两分支内**的 `add_*` 在命令流中**均可被扫到**（与互斥配置可能冲突，见边界）。
4. 写出 `package.xml` 与各 `target.xml`；路径均为**相对**对应 `target.xml` 所在目录的 portable 形式。若同时存在 **`<headers>`** 与 **`<sources>`**，**默认**先写 **`<headers>`** 再写 **`<sources>`**（与 `target` 子元素常见阅读顺序「对外头 → 源」一致；`gz` 合并解析不依赖子块物理顺序，见 `package-target-xml-spec` §1.1）。
5. **`target.xml` 中 `<sources>` 与 `<headers>`**（与 `package-target-xml-spec` 中「头文件**安装**」一致，**不**把编译期 `-I` 混成安装块）：
   - 含未展开 ``${...}`` 的源路径**不**写出，并**注**至 stderr，由用户按真实 configure 手补。
   - **不**将 `include_directories` / `target_include_directories` 的目录列成 `<headers><dir from=…/>`；实现仍可在解释阶段用其信息，**默认**不在 XML 中反映为「待安装头」的 `<dir>`。
   - **`<sources><file>`**：常见编译扩展名（如 `.c`、`.cpp` 等）及**未**判为「对外安装」的头文件；其余头（如 `deflate.h`）归此块，作为与实现同列的**私有/实现头**初稿。
   - **`<headers>`**：仅对**可判为 public 安装**的头使用 **`<file from="…"/>`**（启发式：相对 `--source` 路径中**出现**名为 `include` 的**路径段**，或**无** `include/` 时头文件主名与 `project` 导出的包名同 stem、且扩展名为头文件，如 `zlib` → `zlib.h`）。**不**解析 `install(FILES|DIRECTORY|…)` 时，与真实安装集可能不一致（例：`zconf.h` 常需 configure 后手改补入）。

## 与 File API 路线的关系

- **不**使用 CMake File API、**不**读 `codemodel` JSON、**不**依赖 `nlohmann/json` 等解析库。

## 过滤与映射

- 跳过名称与常见伪目标同名的目标（如 `ALL_BUILD` 等，实现中大小写不敏感子集）；
- **可执行 / 静库 / 动库**（或模块作共享库）映射为 `target.xml` 的 `type=`：`executable`、`static_library`、`shared_library`。
- 同包 `<dependency name="…">` 仅当链接名在本轮已登记目标名集合中；`::` 与明显路径/生成式不写字面条目。

## 重要边界

- **非**完整 CMake：宏/函数体实例化、生成器表达式、`file(GLOB)` 动态、复杂 `if` 分支、外部 `include()` 的模块逻辑等均**不保证**与真实 configure 一致；结果均为**初稿**。
- 当无法计算相对路径（如跨盘符）时打印警告，可能与 `gz configure` 路径规则冲突，应手改或调整 `--out` 位置。

## 分发与安装

- 与根工程一体构建时，可执行文件通过 **`install(..., COMPONENT gz_runtime)`** 安装到 **`bin/`**；与 **`gz`**、**`gz-gui`** 同属默认 `python install.py` 的运行时分量。

## 错误与退出

- 常见失败：无顶层 `CMakeLists.txt`、解析后无可用目标。工具向 stderr 输出说明并以非零退出（与 `gz` 主程序退出码表无强制对齐）。
