# `package.xml` 与 `target.xml` 规范说明

> **文档索引**（`doc/zh` / `doc/en` 全部入口表）：[`../README.md`](../README.md)  
> **English full text**: [`../en/package-target-xml-spec.md`](../en/package-target-xml-spec.md)

本文档描述 **gz（GroundZero）** 当前实现对两种描述文件的**解析约定**与 **configure** 阶段的**语义约束**。实现采用轻量正则扫描（见 `GroundZero/lib/engine/xml/simple_xml.cpp`），**不是**完整 XML 校验器：建议仍写成良构 XML，并遵守下列可识别形态。

**权威英文机器可比对文本** 为 **`gz spec`** 命令在标准输出打印的全文，版本以 `GroundZero/lib/engine/commands/spec.cpp` 中的 **`GZ_XML_SPEC_REVISION`** 为准（当前 **33**）。内嵌文本按 **DOM 树**（`package` / `target`）、**变量体系**（内置、合并、`when`、模板）、**脚本与 trigger**（细节另见 `script-messages.md`）三条主线组织。**本中文文档** 保留叙述、字段表、示例与交叉引用；若与 **`gz spec`** 或实现冲突，以 **`gz spec` 与源码** 为准。

### 与其它文档的分工

| 需求 | 文档 |
|------|------|
| **子命令、argv、退出码、中间目录、`--build-dir-name` / `--install-dir-name`** | [`cli-reference.md`](cli-reference.md) |
| **从零跟做的步骤与示例 XML** | [`getting-started.md`](getting-started.md) |
| **文档地图、FAQ、gz-gui、不写 XML 字段时的鸟瞰** | [`user-manual.md`](user-manual.md) |
| **内置变量、`gz_cache.txt`、`GZ_*`、`CMAKE_PREFIX_PATH` 聚合** | [`internal-variables.md`](internal-variables.md) |
| **本文**：`package.xml` / `target.xml` **字段、多块合并、`when`、依赖与类型约束** | （当前页）+ **`gz spec`** |

### 与内嵌 `gz spec` 的对照（rev 33）

| `gz spec` 中的节 | 本文与仓库中对应位置 |
|------------------|------------------------|
| §1 校验命令（`gz configure` / `gz list`） | 子命令全文见 [`cli-reference.md`](cli-reference.md)；`gz spec` §1 亦概括 list 的 stdout/导出行为。 |
| §2 DOM 式树 | **§1**（角色与合并）、**§2–3**（分字段说明）。 |
| §3 变量 | **§2.4–2.6**、**§3.5**、[`internal-variables.md`](internal-variables.md)。 |
| §4 脚本与 trigger | **§2.7**、[`script-messages.md`](script-messages.md)。 |
| §5–6 `package.xml` / `target.xml` 字段 | 下述 **§2**、**§3**。 |
| §7 编码与主包 | **§4–5**。 |
| §8 示例 | **§6**、[`../../test_projects/README.md`](../../test_projects/README.md)。 |
| §9–10 实现指针与 pack/再分发 | **§7–8**；源码见 `gz spec` 表。 |

### 产品方向（推荐工作流）

- **推荐**：用户**纯手写** **`package.xml`** 与 **`target.xml`**，显式描述源文件、`<headers>`、`<defines>`、`<dependency>`、**`<prebuilt …/>`** 与 **`prebuilt_static_library` / `prebuilt_shared_library`** 等；分步说明见 **[getting-started.md](getting-started.md)**。
- **集成上游 CMake 第三方库时**：在 **包外** 构建或安装后，将 **`.lib` / `.a` / `.dll` / `.so`** 等置于本仓库可引用路径（或固定 SDK 目录），再在 **`target.xml`** 用 **`prebuilt_static_library` / `prebuilt_shared_library`** + **`<prebuilt …/>`** 指向这些**磁盘路径**（相对 **`target.xml`** 所在目录或绝对路径）。

---

## 1. 文件角色与放置

| 文件 | 放置位置 | 作用 |
|------|----------|------|
| **`package.xml`** | 每个**独立包**的根目录（与包内 `target.xml` 树的上层一致） | 声明包名、版本、**包级**依赖（其他包名）；可选 **`<vars>`**、**`<defines>`**、**`<config_files>`** 等。 |
| **`target.xml`** | 包内**每个构建目标独占一个子目录**，该目录下**恰好一个** `target.xml` | 声明目标名、类型、源文件、可选的头文件搜索路径、**目标级**依赖（其他 target，通常为库）。 |

- **归属**：`target.xml` 必须位于某一 `package.xml` 所在目录的**子树内**；`configure` 会把 target 归到路径上**最近**的包根下（见 `configure.cpp` 中 `nearest_package_parent`）。
- **扫描**：`gz configure` / **`gz list`** 在扫描根（默认 cwd，或 `--scan` 指定目录）下**递归**查找所有 `package.xml` 与 `target.xml`。递归时**不进入**名为 **`.intermediate`** 的子目录；此外，若 **`--scan`** 指向的路径落在 **`<cwd>/.intermediate/`** 之下（规范化后），该扫描根会被**丢弃**并告警，以免把构建产物里的 **`gz-redist/`** 等误当作源码包。

### 1.1 同名块可多次出现（合并顺序）

在 **`package.xml`** 的 **`<package>`** 内、以及 **`target.xml`** 的 **`<target>`** 内，下列**成对闭合**子块可出现**任意多次**（按**文档中出现顺序**依次解析，结果**追加**到同一列表或同一合并语义；块内条目相对顺序仍按各块内规则）：

- **`package.xml`**：**`<vars>`**、**`<defines>`**、**`<config_files>`**
- **`target.xml`**：**`<sources>`**、**`<headers>`**、**`<assets>`**、**`<vars>`**、**`<defines>`**、**`<config_files>`**

**自闭合/无体标签**（如 **`<prebuilt …/>`**、**`<dependency …/>`**）可出现多次：**`<prebuilt/>`** 以**最后一次**有效声明覆盖预置库信息；**`<dependency/>`** 仍为全文正则匹配，顺序即依赖列表顺序。

**裸 `<file>…</file>`**（无 **`<sources>`** 包裹）：仅当文件中**不存在任何** **`<sources>…</sources>`** 块时，才对全文匹配裸 `<file>`；只要出现过至少一个 `<sources>` 块，则**只**从各 `<sources>` 块体收集源，**不再**从块外收集裸 `<file>`。

---

## 2. `package.xml`

### 2.1 根元素

- 文件中须出现**第一个**匹配子串 **`<package`** 的片段，解析器取其到**第一个 `>`** 为止的**起始标签**作为包头（不要求完整 XML 树校验）。
- **必选属性**
  - **`name`**（字符串）：包名。在一次扫描内必须**全局唯一**；与 `target.xml` 里 `package:target` 形式引用时的包名一致。
- **可选属性**
  - **`version`**（字符串）：版本号。省略时实现上默认为 **`0.0.0`**。

示例：

```xml
<?xml version="1.0" encoding="UTF-8"?>
<package name="hello_demo" version="0.1.0">
  <dependency name="hello_lib"/>
</package>
```

### 2.2 包依赖 `<dependency ... />`

- 形式：自闭合标签 **`<dependency ... />`**，且标签内需包含 **`name="..."`**（双引号字符串）。
- **`name`**：所依赖的**其他包**的包名；该包必须在**同一次 configure 扫描结果**中出现，否则：
  - 若 **`optional="true"`**（或值为 **`1` / `yes`**，大小写按实现解析）：仅作可选声明；
  - 否则 **configure 失败**，报错提示缺少该包。
- **`optional`**：可选。若存在 **`optional="..."`**，仅当值为 **`true` / `1` / `yes`** 时视为可选依赖；其它写法视为非可选。

实现用正则匹配所有 `<dependency` … `/>` 片段，**不**要求它们嵌在某个父节点下；为可读性建议写在 `<package>` 内。

### 2.3 与 `target.xml` 的关系

若本包内某 `target.xml` 使用 **`外包包名:目标名`** 引用依赖，则被引用包名**必须**在本 `package.xml` 的 `<dependency name="该包名"/>` 中声明（可选依赖除外按上述规则），否则 **configure 失败**。

---

## 3. `target.xml`

### 3.1 根元素

- 须存在 **`<target`** 起始标签；解析方式与 `package` 相同，取到第一个 **`>`** 为止。
- **必选属性**
  - **`name`**：CMake 目标名（与可执行文件名、`gz run` 所用名等一致）。
- **可选属性**
  - **`type`**：目标类型。省略时默认为 **`executable`**。实现识别（大小写按生成后端使用前为准，建议小写）：
    - **`executable`**：可执行程序。
    - **`library`**：由**工作区选项**决定生成静态库还是动态库：与 **`GZ_TARGET_DYNAMIC_LIBRARY`**（及兼容键 **`GZ_DYNAMIC_LIBRARY`**）一致——为真时按动态库参与构建与链接，为假时按静态库。用于默认服从全局/用户选择的常规库目标。
    - **`static_library`**：**强制**静态库；**不**因上述全局动态/静态偏好而改变链接形态（用于必须固定为静态的特殊约束）。
    - **`shared_library`**：**强制**动态库；**不**因全局偏好而改变（用于必须固定为动态的特殊约束）。
    - **`asset_bundle`**：不参与编译链接；安装内容由 **`sources` / `<assets>…</assets>` / `<headers>`** 三者至少其一描述（见下表），其中 **`asset_bundle`** 的典型用法是 **`<assets>`**。
    - **`prebuilt_static_library`**：**`<prebuilt …/>`** 指向磁盘上的预置静态库（`.lib` / `.a` 等）；路径相对 **`target.xml`** 所在目录或绝对路径。
    - **`prebuilt_shared_library`**：**`<prebuilt …/>`** 指向磁盘上的预置动态库；Windows 需可解析 **`.dll`**（**`dll=`** / **`location=`**）与 **import `.lib`**（**`import_lib=`**）。
  - 未在以上各条列出的 **`type`** 在 **configure** 时按**未知目标类型**拒绝：stderr 含 **`configure: unknown target type "…" for target "…"`**；**退出码 5**（**[`cli-reference.md`](cli-reference.md) §3 / §5**；实现见 **`configure.cpp`**）。

类型与关键子标签约束（当前实现）：

| type | `<sources>` | `<prebuilt/>` | 备注 |
|------|-------------|---------------|------|
| `executable` / `library` / `static_library` / `shared_library` | 需要（至少解析到一个源文件） | 否 | 常规编译目标；`library` 在 configure 时按 `GZ_TARGET_DYNAMIC_LIBRARY` 解析为静态或动态库产物 |
| `asset_bundle` | 可空 | 否 | 需至少有 `sources` / `assets` / `<headers>` 之一 |
| `prebuilt_static_library` | 不需要 | 需要 | 路径相对 `target.xml`（或绝对路径） |
| `prebuilt_shared_library` | 不需要 | 需要 | Windows 需可解析 dll + import lib |

#### 3.1.1 预置库 **`<prebuilt/>`**（二进制**布局**元数据）

- 安装根下单段目录名由 **`gz configure` 写入的 `arch`（`compose_arch_tag` 结果）/ `gz_cache.txt` 的 `arch=`** 等表达；**`<prebuilt/>` 上的布局分字段不重复**该名单段名。
- 在 **`import_lib` / `location` / `dll`** 之外，使用**多个可选属性**表达布局维度：
  - **`os` / `cpu` / `build_system` / `toolchain` / `link`（`static`｜`dynamic`）/ `config`（`debug`｜`release`）/ `crt`**：与 build 时选项一致（**`crt`** 在 Windows 上常见，非 Windows 常为空）；定义见 `GroundZero/lib/infra/platform/paths.cpp` 的 **`compose_arch_tag`**、**`try_decompose_compose_arch_tag`**。
- **`gz build` 二次分发** 写出的 **`target.xml` / `schema` 3 的 `gz_redist_manifest.json`** 只写**上述分字段**；**JSON** 无单独的「安装目录名单段」键，与 **`<prebuilt/>`** 分字段一一对应。
- **读入兼容（弃用 `arch` 长串）**：**`<prebuilt/>`** 上旧式 **`arch="…"` 单长串**仍可在解析时**反拆**为分字段（不原样回写）；**manifest** 的 **schema 1** 中 **`arch`** 同理。新文件应写分字段。第三方已安装到磁盘其它前缀的库，请用 **`<prebuilt/>`** 指向**相对 `target.xml` 或绝对路径**下的 `.lib` / `.dll` / `.so` 等，并配合 **`<headers>`**。

### 3.2 源文件 `<file> ... </file>`

- 每个源文件一对标签：**`<file>`** 与 **`</file>`**（标签名前后允许空白，如 `</file >` 等形式需与实现正则一致；**建议**使用规范写法 `<file>...</file>`）。
- 标签内文本（去掉首尾空白）为**相对路径**，**相对于本 `target.xml` 所在目录**。
- 同一文件可出现**多个** `<file>`，顺序即加入构建的源文件列表顺序。

```xml
<sources>
  <file>main.cpp</file>
</sources>
```

说明：若文件中**没有任何** **`<sources>…</sources>`** 块，解析器会对**全文**匹配裸 **`<file>…</file>`** 并收录；若存在至少一个 **`<sources>`** 块，则**仅**解析各 **`<sources>`** 块体内的条目，**忽略块外**裸 `<file>`。多个 **`<sources>`** 块按 §1.1 顺序合并。

### 3.3 头文件输入与安装输出 `<headers>`

**`<headers>…</headers>`** 可出现多个（见 §1.1）。`<headers>` 统一使用 **`from/to`** 自闭合条目，支持三种类型：

- **`<dir from="..." to="..."/>`**
- **`<file from="..." to="..."/>`**
- **`<glob from="..." to="..."/>`**

其中：

- **`from`**：必填，路径相对 `target.xml` 所在目录。
- **`to`**：可选，安装目标目录，相对安装前缀下的 `include/`；空值或省略表示安装到 `include/` 根。
- **`when`**：可选；为假时该条目不参与编译期 include 推导，也不参与安装规则（语义见 §3.5）。

示例：

```xml
<headers>
  <dir from="../../include/rockBase" to="rockBase"/>
  <file from="../../include/common/version.hpp" to="common"/>
  <glob from="../../include/rockBase/*.hpp" to="rockBase"/>
</headers>
```

编译期（CMake 生成）：

- `dir.from` 直接作为 include 目录。
- `file.from` 取父目录作为 include 目录。
- `glob.from` 取 glob 基目录作为 include 目录。

安装期（CMake 生成）：

- `dir` 生成目录安装规则：`install(DIRECTORY ... DESTINATION include/<to>)`。
- `file` 生成文件安装规则：`install(FILES ... DESTINATION include/<to>)`。
- `glob` 在 configure 阶段解析匹配文件并按文件安装到 `include/<to>`（无匹配会给 warning）。

> **命名**：使用 **`<headers>`** 表示「头文件来源与安装到 `include/` 下的布局」，避免与编译器 **`-I` / `include_directories`** 等泛泛的 “includes” 混淆。**不再支持** `<includes>`，须使用 `<headers>...</headers>`。嵌套旧写法 `<headers><dir>path</dir></headers>` 不支持，须改为 `<dir from="..." to="..."/>`。

### 3.4 编译宏 `<defines>`

- 可选块：**`<defines>`** … **`</defines>`**（可出现多个，见 §1.1），内为若干自闭合 **`<define name="..." value="..."/>`**。
- **`name`**：必填，须为 **C 标识符**（`[A-Za-z_][A-Za-z0-9_]*`），对应预处理器宏名。
- **`value`**：可选。省略时仅定义宏名（等价于 `#ifdef NAME` 为真、未赋替换文本）；给出时生成 **`NAME=value`**。
- **`value` 字符集**（当前实现）：仅允许字母、数字及 **`._+-/`**（不允许空格，以便 Ninja/MSVC 命令行稳定传递）。

**生成行为**：

- **CMake**：对 `executable` / `library` / `static_library` / `shared_library` 生成 **`target_compile_definitions(<target> PRIVATE ...)`**（导入库 / `asset_bundle` 等不参与编译的目标忽略本块；**`library`** 在 configure 内已解析为静/动库后再生成）。
- **Ninja**：在同一目标的各 `cxx` 编译命令上追加 **`/D...`**（Windows `cl`）或 **`-D...`**（类 Unix）。

示例：

```xml
<defines>
  <define name="USE_ROCK"/>
  <define name="APP_VERSION" value="1.0.0"/>
</defines>
```

### 2.4 包级变量 `<vars>`（可选）

- 块 **`<vars>...</vars>`**（可出现多个，见 §1.1），内为自闭合 **`<var name="KEY" value="VAL"/>`**（`value` 可省略表示空字符串）。
- 每个 **KEY** 表示**默认值**，供本包下所有 `target.xml` 在 **`@KEY@` 替换**与 **`when`** 中使用；同一 KEY 还可在 **`gz configure --opt KEY=...`** 或 **`gz_cache.txt`** 中写入（键须符合实现允许的格式：所有 `GZ_*`，以及除保留键外的 C 风格标识符），**configure 阶段最后应用，覆盖 XML 默认值**（见 §3.5 合并顺序）。

### 2.5 包级编译宏 `<defines>`（可选）

- 与 **`target.xml` 中 `<defines>`** 同形：**`<defines>...</defines>`**（可出现多个，见 §1.1）内若干 **`<define name="标识符" value="..."/>`**（`name`、`value` 规则与目标级一致，见 §3.4）。
- **作用域**：作用于本包内所有 **`executable` / `library` / `static_library` / `shared_library`** 目标的编译命令；**先于**各目标自身的 `<defines>` 注入（目标级宏在命令行上靠后，一般由编译器后写覆盖同名宏；**`library`** 同上，在 configure 内解析后再注入）。

### 2.6 包级配置模板 `<config_files>`（可选）

- 与 **`target.xml` 中 `<config_files>`** 同形：**`<config_files>...</config_files>`**（可出现多个，见 §1.1）内若干 **`<file in="相对路径" to="相对路径"/>`**（`in`、`to` 均必填）。
- **`in`**：相对 **`package.xml` 所在目录**（包根）。
- **`to`**：相对 **`.intermediate/generated/<arch 段>/<包名>/_package/`**（`<arch 段>` 与本次 configure 的 **`gz_cache.txt` 中 `arch=`** / **`compose_arch_tag`** 一致，按构建配置区分多架构/多叶；实现保留目录名 **`_package`**；请勿在本包内再使用同名编译目标目录以免混淆）。
- **`@NAME@` / `${NAME}` 替换**：使用 **内置变量 + `package.xml` 中 `<vars>` + 本包内所有 `target.xml` 的 `<vars>`**（按 `configure` 收集目标的顺序依次叠加；**同名键以后写入者为准**，含同一 `<vars>` 块内靠后的 `<var/>`）+ **`--opt` / `gz_cache.txt`**。内置 **`GZ_TARGET_NAME` 在包级模板中默认为空串**，除非某个目标用 `<var name="GZ_TARGET_NAME" …/>` 覆盖。
- **生成时机**：`configure` 对每个含条目的包生成一次；生成文件加入本包内**每一个** **`executable` / `library` / `static_library` / `shared_library`** 目标的编译源列表，并把 **`generated/<arch 段>/<包名>/_package/`** 加入这些目标的编译期 **include 路径**（**`library`** 解析后同上）。

### 2.7 脚本型 `<var type="script">`（消息 / trigger）

- **位置**：`package.xml` 与 **`target.xml`** 的 **`<vars>...</vars>`** 内均可声明，与标量 **`<var name="KEY" value="VAL"/>`** 混排。
- **形状**：**`<var name="…" type="script" script_type="lua" trigger="…" value="…"/>`**（`script_type` 可省略，默认 `lua`；`trigger` 可省略，默认 **`manual`**）。
- **合法 `trigger`、configure 派发点、与 `<preprocess command>` 的优先级、Dom 父链覆盖顺序**：见 **`script-messages.md`**（[`script-messages.md`](script-messages.md)；与实现 **`script_execution.cpp`** / **`configure.cpp`** 对齐）。
- **重要**：当前实现将匹配到的 **`value` 作为 shell 命令字符串** 使用，**不**在宿主内执行 Lua 字节码；产品名中的「Lua」表示类型筛选与未来扩展槽位。

### 3.5 变量合并、`@KEY@`、`config_files` 与 `when`

#### 变量层（后者覆盖前者）

用于 **目标级** `<config_files>` 模板中的 **`@NAME@` / `${NAME}`** 替换（完整合并栈），**包级** `<config_files>` 使用 **第 1、2 层**后将本包内 **各目标第 3 层** 的 `<vars>` **按目标收集顺序串成一层**再应用 **第 4 层**（工作区）；**`GZ_TARGET_NAME`** 仍由内置给出（包级默认为空），可被目标 `<vars>` 覆盖。另用于 `<sources>` / `<headers>` 条目的 **`when="..."`** 求值：

1. **内置**：`GZ_OS`（`windows` | `linux` | `darwin`）、`GZ_PACKAGE_NAME`、`GZ_PACKAGE_VERSION`、`GZ_TARGET_NAME`、`GZ_TARGET_BUILD_SYSTEM`（`cmake` | `ninja`）、`GZ_CONFIG`（`debug` | `release`）。
2. **`package.xml` 中 `<vars>`**（包级默认值）。
3. **`target.xml` 中 `<vars>`**（目标级默认值；与包级同名时覆盖包级）。
4. **工作区**：`--opt` 与 `gz_cache.txt` 中与上述规则一致的 **KEY=VALUE**（与 `GZ_*` 等选项共用一张表；**最后应用**，覆盖 XML 中的同名 `<vars>` 默认值）。

支持 **`@NAME@`** 与 **`${NAME}`**（`NAME` 为 C 标识符；类 CMake `configure_file` 子集）。**`<config_files>` 模板**：在替换前会扫描模板正文，凡 **`@NAME@`** / **`${NAME}`** 中的 `NAME` 为 C 标识符且**尚不在**合并变量表中，则**自动加入该键、值为空串**，占位符会被**删掉**（不再保留 `${…}` 原文）；需要真实类型名等请在 **`package.xml` / `target.xml` 的 `<vars>`** 或 **`--opt`** 中显式赋值。不解析 **`$<...>`**。**`when="..."`** 仍只用合并表，不会从模板收集键。`gz configure` 生成的 **`packages.md`** 中会列出各包/目标的 `<vars>` 默认值，并标出该 KEY 是否也出现在本次 configure 的选项映射中（便于核对是否被 `--opt` / 缓存覆盖）。

#### `when` 适用的标签（当前实现）

- **已实现求值**：**`<sources>`** 内带 `from=` 的 **`<file>` / `<glob>`**（自闭合或配对开标签上的 `when`）；**`<headers>`** 内 **`<dir>` / `<file>` / `<glob>`** 上的 `when`。
- **未实现（勿依赖；写了也不会按条件生效）**：**`<assets>`**、**`<define>`**、**`<dependency/>`**、**`<config_files>`** 内 **`<file>`**、**`<var>`**、包级行、**`<prebuilt/>`**、预处理/后处理子标签、以及 **`<package>` / `<target>` 根标签**。（历史文档中的 **`<install …/>`** 若仍出现在文件里，当前解析器**不识别**；请改用 **`<prebuilt/>`**。）
- 产品上可为更多「行级」配置逐步统一 `when`，但需逐类定义语义并实现（例如跳过依赖与「未声明依赖」的边界）。

#### `when` 表达式语法（与 `eval_when`，`GroundZero/lib/engine/xml/var_subst.cpp` 一致）

对 `when="..."` 的字符串先做 **ASCII 首尾空白** 修剪，再按下列 **唯一一种** 形式匹配（**自上而下**；整串须完全属于该形式）：

1. **空串**（修剪后）：视为 **真**（恒包含该项）。
2. **布尔字面量**：整串为 **`true`** 或 **`false`**（ASCII，**大小写不敏感**）。
3. **比较式**：整串须匹配 **`KEY == RHS`** 或 **`KEY != RHS`**（`==` / `!=` 两侧可有空白），其中：
   - **`KEY`**：单个 C 风格标识符 **`[A-Za-z_][A-Za-z0-9_]*`**，在 **§3.5 变量合并** 得到的映射表中查找。
   - **`RHS`**：单个 token，**仅** **`[A-Za-z0-9_.]+`**（**不能**含空格、引号、`-`、`/` 等；若值里含连字符等，请改用「单独 KEY + 裸变量真值」或其它变量设计，勿指望在字面 RHS 里写 `dynamic-md` 这类 token）。
   - 将表中 **`KEY` 的值** 与 **`RHS` 字面** 均转为 **ASCII 小写** 后做字符串相等 / 不等比较。
   - 若表中 **无 `KEY`**：比较时 **`KEY` 的值按空串** 参与。
4. **裸标识符**：整串为 **一个** 标识符 **`[A-Za-z_][A-Za-z0-9_]*`**，且 **必须** 在合并表中 **存在**；取其值做 **真值**判断：
   - **假**：值为空，或（大小写不敏感）为 **`0`**、**`false`**、**`off`**、**`no`**；
   - **真**：其它任意值。
   - 若 **键不存在**：**configure 失败**（未知 `when`）。

**不支持**：逻辑与/或（`&&` `||`）、括号分组、函数调用、子串匹配、引号包裹的任意文本、或上述形式以外的自由表达式。

**为假时**（且该标签已实现 `when`）：对应 **`<sources>` / `<headers>`** 条目被跳过（不进入编译源列表 / 不参与头文件 include 推导与安装规则）。

#### `<config_files>`（类 configure_file 的最小子集）

- **目标级**（`target.xml`）：块 **`<config_files>...</config_files>`**（可出现多个，见 §1.1），内为 **`<file in="模板相对路径" to="输出相对路径"/>`**（`in`、`to` 均必填）。`in` 相对 **`target.xml` 所在目录**；`to` 相对 **`.intermediate/generated/<arch 段>/<包名>/<目标名>/`**，且须为安全相对路径（**不得**含 `..` 段、不得为绝对路径）。**`@NAME@`** 使用 **§3.5 完整变量层**。生成文件加入**该目标**的编译源列表，并把 **`generated/<arch 段>/<包名>/<目标名>/`** 加入该目标的 **编译期 include 路径**。
- **包级**（`package.xml`）：形状相同（可出现多个，见 §1.1）；`in` 相对 **包根**；`to` 相对 **`.intermediate/generated/<arch 段>/<包名>/_package/`**（保留名 **`_package`**）。**`@NAME@` / `${NAME}`** 使用 **包级 `<vars>` + 本包内全部目标的 `<vars>`**（顺序与叠加规则见上）+ 工作区；**`GZ_TARGET_NAME`** 默认为空。生成文件加入本包内**所有**原生编译目标的源列表，并把 **`generated/<arch 段>/<包名>/_package/`** 加入这些目标的 **include 路径**。
- **`${NAME}`**（CMake **`configure_file`** 风格）：与 **`@NAME@`** 使用**同一套**合并后的变量表；`NAME` 须为 C 标识符（`${` 与 `}` 之间允许首尾空白）。**先做占位符收集**：模板里出现但表中**没有**的 `NAME` 会**以空串加入表**，再交替替换，故未在 XML / `--opt` 声明的占位符会变成**空展开**（不再保留 `${NAME}`）；需要非空内容必须在 **`<vars>`** 或 **`--opt`** 中提供。不解析 **`$<...>`** 生成器表达式。处理顺序：**反复交替 `@NAME@` 与 `${NAME}` 至不再变化（有上限）**，最后再做 **`#cmakedefine`**。
- **`#cmakedefine` / `#cmakedefine01`**：在上述占位符替换之后，按与 CMake **`configure_file`** 相近的**子集**处理（见 `GroundZero/lib/engine/xml/var_subst.cpp` 中 `apply_cmakedefine_directives`）：未出现在合并变量表、或值为空 / `0` / `false` / `off` / `no` 视为假；`#cmakedefine01` 展开为 **`#define NAME 0`** 或 **`1`**；`#cmakedefine NAME` 为真则 **`#define NAME`**，否则 **`/* #undef NAME */`**；带尾部内容的 **`#cmakedefine NAME …`** 为真则输出 **`#define NAME …`**。用于 zlib 等 **`zconf.h.in`** 类模板；**不是**完整 CMake 生成器。

**CMake 与 Ninja**：两后端均直接消费 **已由 gz 写出的生成文件**（与普通源文件相同），当前实现**不**为此生成 CMake 的 `configure_file()`。若需要完整上游 CMake `configure_file` 语义，请在**包外**用上游工程处理。

#### `<sources>` 上的 `when`

- 自闭合 **`<file from="..." when="..."/>`**、**`<glob from="..." when="..."/>`**（须带 `from`）。
- 或配对标签开标签上的 **`when`**。
- 为假时该源条目被跳过（glob 不展开）。

### 3.6 目标依赖 `<dependency name="..." visibility="..."/>`

- 自闭合 **`<dependency .../>`**，可多次出现。
- **`name`** 支持两种形式：
  1. **`目标名`**：与本包内某一**库** target 同名，表示依赖本包该库。
  2. **`包名:目标名`**：依赖**其他包**中的某一库 target（该包须在 `package.xml` 中声明，且该 target 须在扫描集中存在）。
- **`visibility`**（可选，默认 **`private`**，解析时大小写不敏感）：取值为 **`private` / `public` / `interface`**，对应 CMake `target_link_libraries` 的 **`PRIVATE` / `PUBLIC` / `INTERFACE`**。**可执行文件**作为消费者时，**不允许**使用 **`interface`**（可执行文件必须实际链接该库；否则 configure 失败）。
- **约束**：依赖指向的目标类型须为库目标（`static_library` / `shared_library` / `library` / `prebuilt_static_library` / `prebuilt_shared_library`）或 **`asset_bundle`**（仅校验图，不参与链接）；解析不到或类型不对则 **configure 失败**。

**生成行为（CMake 模式，当前实现）**：

- **可执行目标**：若 **没有任何** 显式 `<dependency>` 链接行，则 **PRIVATE 链接本包内全部库 target**；若存在显式 `<dependency>`，则使用其中解析出的库名及 **`visibility`** 生成 `target_link_libraries`（同名库若重复声明，以后写覆盖；并排序、去重）。
- **库目标**：`<dependency>` 会参与依赖校验与「外包包内库」加入生成图；**当前不会**为静态库与静态库之间生成 `target_link_libraries` 链式链接。若甲库实现需调用乙库符号，需在工程层面自行保证链接顺序或合并目标（例如由最终可执行文件链接全部库）；详见 `test_projects` 中示例取舍。

---

## 4. 编码与路径

- 文件内容建议使用 **UTF-8**（与 [DESIGN.md](../DESIGN.md) 中包描述编码说明一致）。
- **configure** 会对相关路径做 **ASCII 路径**等校验；路径中含非 ASCII 等可能按实现直接报错，请避免。

---

## 5. configure 对「主包」与目标的额外要求

- **主包**：优先取 **cwd 等于其 `package.xml` 父目录`** 的那一个包；若无匹配，则取扫描到的**第一个**包作为主包（多包同扫时顺序依赖文件系统，建议单包目录下执行或明确文档化扫描顺序风险）。
- 主包下至少需要 **一个 target.xml**（可执行/库/导入库均可）；纯库包（例如只含 **`prebuilt_*` + `<prebuilt/>`**）允许 configure/build。
- 同一 **`包名:目标名`** 在扫描结果中不得重复。

---

## 6. 测试与示例

仓库 **`test_projects/`** 下各子目录为完整示例（含跨包依赖、多库、分离的 `include/` / `src/` / `app/` / `test/` 等），见 [test_projects/README.md](../test_projects/README.md)。

---

## 7. 与实现的对应关系

| 话题 | 源码位置 |
|------|----------|
| 解析 `package.xml` / `target.xml` | `GroundZero/lib/engine/xml/simple_xml.cpp`（`load_package_xml` / `load_target_xml`） |
| 依赖校验、生成后端 | `GroundZero/lib/engine/commands/configure.cpp` |
| 变量合并、`@KEY@`、`when` | `GroundZero/lib/engine/xml/var_subst.cpp` |
| 数据结构 | `GroundZero/lib/engine/xml/simple_xml.hpp`（`PackageDesc` / `TargetDesc`） |

后续若引入 XSD/JSON Schema，可在本文件顶部增加版本号与变更记录。

---

## 8. 安装树、**`gz pack`** 与「二次分发用 XML」（职责划分 / 与实现对齐）

- **`gz pack`（仅归档）**：只对 **`--install-dir-name`** 所指的 **安装树根**（**`.intermediate/install/<arch>/`**，内含 **`bin/`**、**`lib/`**、**`include/`** 等）做 **归档**（PowerShell **`Compress-Archive`** / **`tar`**，或 **CPack** 可用时走 **CPack**）。**`pack` 不生成**新的 **`package.xml` / `target.xml`**，也**不改写**源码型目标。若安装树下已存在 **`gz-redist/`**（见下），会**随安装树一并**被打进归档。实现：**`GroundZero/lib/engine/commands/pack.cpp`**、**`run_pack_backend`**（**`backend_dispatch.cpp`**）。

- **`gz build`（默认生成二次分发 XML）**：在 **编译 + install 成功之后**，默认读取 **`.intermediate/build/<叶子>/gz_redist_manifest.json`**（由 **`gz configure`** 根据当前图写入；**`schema` 3** 为分字段；若主包无可导出库目标则可能**不创建**该文件）。当 manifest 存在且含 **`targets`** 时，在安装根下写入 **`gz-redist/package.xml`** 及 **`gz-redist/<emit-name>/target.xml`**；二次分发的 **`target.xml`** 中**仅**出现 **`<prebuilt …/>`**（**§3.1.1** 分字段；旧 **`schema` 1** 的 **`arch`** 可读入后**反拆**为布局；**`schema` 2/3** 自 JSON 读分字段）。**`executable`** 与 **`asset_bundle`** 当前**不写入** manifest（MVP）。可用 **`--no-emit-redistribution-xml`** 或 **`GZ_EMIT_REDIST_XML=0` / `false` / `off` / `no`**（大小写不敏感）关闭。实现：**`build.cpp`**、**`redist_emit.cpp`**；清单生成：**`configure.cpp`**（`try_write_gz_redist_manifest_json`）。

- **消费提示**：二次分发包可将 **`gz-redist/`** 与 **`bin/`、`lib/`、`include/`** 置于同一安装前缀下分发；下游将 **`gz-redist` 的父目录** 当作安装前缀、将 **`gz-redist`** 当作**包根**扫描时，应与上述路径约定一致。
