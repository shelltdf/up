# `package.xml` 与 `target.xml` 规范说明

本文档描述 **up** 当前实现对两种描述文件的**解析约定**与 **configure** 阶段的**语义约束**。实现采用轻量正则扫描（见 `src/simple_xml.cpp`），**不是**完整 XML 校验器：建议仍写成良构 XML，并遵守下列可识别形态。

---

## 1. 文件角色与放置

| 文件 | 放置位置 | 作用 |
|------|----------|------|
| **`package.xml`** | 每个**独立包**的根目录（与包内 `target.xml` 树的上层一致） | 声明包名、版本、**包级**依赖（其他包名）；可选 **`<vars>`**、**`<defines>`** 等。 |
| **`target.xml`** | 包内**每个构建目标独占一个子目录**，该目录下**恰好一个** `target.xml` | 声明目标名、类型、源文件、可选的头文件搜索路径、**目标级**依赖（其他 target，通常为库）。 |

- **归属**：`target.xml` 必须位于某一 `package.xml` 所在目录的**子树内**；`configure` 会把 target 归到路径上**最近**的包根下（见 `configure.cpp` 中 `nearest_package_parent`）。
- **扫描**：`up configure` 在扫描根（默认 cwd，或 `--scan` 指定目录）下**递归**查找所有 `package.xml` 与 `target.xml`。

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
  - **`name`**：CMake 目标名（与可执行文件名、`up run` 所用名等一致）。
- **可选属性**
  - **`type`**：目标类型。省略时默认为 **`executable`**。实现识别（大小写按生成后端使用前为准，建议小写）：
    - **`executable`**：可执行程序。
    - **`static_library`**：静态库。
    - **`shared_library`**：动态库。
    - **`asset_bundle`**：仅资源安装，不参与编译链接。
    - **`imported_static_library`**：磁盘预置静态库（需配 `<prebuilt .../>`）。
    - **`imported_shared_library`**：磁盘预置动态库（需配 `<prebuilt .../>`）。
    - **`imported_installed_static_library`**：由同包 `<cmake/>` 子工程先安装到前缀后，再以 `<install artifact=\"...\"/>` 包装的静态库。
    - **`imported_installed_shared_library`**：由同包 `<cmake/>` 子工程先安装到前缀后，再以 `<install artifact=\"...\" implib=\"...\"/>` 包装的动态库。

类型与关键子标签约束（当前实现）：

| type | `<sources>` | `<prebuilt/>` | `<install .../>` | 备注 |
|------|-------------|---------------|------------------|------|
| `executable` / `static_library` / `shared_library` | 需要（至少解析到一个源文件） | 否 | 否 | 常规编译目标 |
| `asset_bundle` | 可空 | 否 | 否 | 需至少有 `sources` / `assets` / `<headers>` 之一 |
| `imported_static_library` | 不需要 | 需要 | 否 | 路径相对 `target.xml`（或绝对路径） |
| `imported_shared_library` | 不需要 | 需要 | 否 | Windows 需可解析 dll + import lib |
| `imported_installed_static_library` | 不需要 | 否 | 需要 | `artifact` 相对 `CMAKE_INSTALL_PREFIX` |
| `imported_installed_shared_library` | 不需要 | 否 | 需要 | Windows 需 `implib` |

### 3.2 源文件 `<file> ... </file>`

- 每个源文件一对标签：**`<file>`** 与 **`</file>`**（标签名前后允许空白，如 `</file >` 等形式需与实现正则一致；**建议**使用规范写法 `<file>...</file>`）。
- 标签内文本（去掉首尾空白）为**相对路径**，**相对于本 `target.xml` 所在目录**。
- 同一文件可出现**多个** `<file>`，顺序即加入构建的源文件列表顺序。

```xml
<sources>
  <file>main.cpp</file>
</sources>
```

说明：**当前解析器不依赖 `<sources>` 父节点**，只要文件中存在符合模式的 `<file>...</file>` 即会收录；使用 `<sources>` 仅为可读性与与 DESIGN 叙述一致。

### 3.3 头文件输入与安装输出 `<headers>`

`<headers>` 统一使用 **`from/to`** 自闭合条目，支持三种类型：

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

- 可选块：**`<defines>`** … **`</defines>`**，内为若干自闭合 **`<define name="..." value="..."/>`**。
- **`name`**：必填，须为 **C 标识符**（`[A-Za-z_][A-Za-z0-9_]*`），对应预处理器宏名。
- **`value`**：可选。省略时仅定义宏名（等价于 `#ifdef NAME` 为真、未赋替换文本）；给出时生成 **`NAME=value`**。
- **`value` 字符集**（当前实现）：仅允许字母、数字及 **`._+-/`**（不允许空格，以便 Ninja/MSVC 命令行稳定传递）。

**生成行为**：

- **CMake**：对 `executable` / `static_library` / `shared_library` 生成 **`target_compile_definitions(<target> PRIVATE ...)`**（导入库 / `asset_bundle` 等不参与编译的目标忽略本块）。
- **Ninja**：在同一目标的各 `cxx` 编译命令上追加 **`/D...`**（Windows `cl`）或 **`-D...`**（类 Unix）。

示例：

```xml
<defines>
  <define name="USE_ROCK"/>
  <define name="APP_VERSION" value="1.0.0"/>
</defines>
```

### 2.4 包级变量 `<vars>`（可选）

- 块 **`<vars>...</vars>`**，内为自闭合 **`<var name="KEY" value="VAL"/>`**（`value` 可省略表示空字符串）。
- 每个 **KEY** 表示**默认值**，供本包下所有 `target.xml` 在 **`@KEY@` 替换**与 **`when`** 中使用；同一 KEY 还可在 **`up configure --opt KEY=...`** 或 **`up_cache.txt`** 中写入（键须符合实现允许的格式：所有 `UP_*` / `UPSTREAM_*`，以及除保留键外的 C 风格标识符），**configure 阶段最后应用，覆盖 XML 默认值**（见 §3.5 合并顺序）。

### 2.5 包级编译宏 `<defines>`（可选）

- 与 **`target.xml` 中 `<defines>`** 同形：**`<defines>...</defines>`** 内若干 **`<define name="标识符" value="..."/>`**（`name`、`value` 规则与目标级一致，见 §3.4）。
- **作用域**：作用于本包内所有 **`executable` / `static_library` / `shared_library`** 目标的编译命令；**先于**各目标自身的 `<defines>` 注入（目标级宏在命令行上靠后，一般由编译器后写覆盖同名宏）。

### 3.5 变量合并、`@KEY@`、`config_files` 与 `when`

#### 变量层（后者覆盖前者）

用于 `<config_files>` 模板中的 **`@NAME@`** 替换，以及 `<sources>` / `<headers>` 条目的 **`when="..."`** 求值：

1. **内置**：`UP_OS`（`windows` | `linux` | `darwin`）、`UP_PACKAGE_NAME`、`UP_PACKAGE_VERSION`、`UP_TARGET_NAME`、`UP_TARGET_BUILD_SYSTEM`（`cmake` | `ninja`）、`UP_CONFIG`（`debug` | `release`）。
2. **`package.xml` 中 `<vars>`**（包级默认值）。
3. **`target.xml` 中 `<vars>`**（目标级默认值；与包级同名时覆盖包级）。
4. **工作区**：`--opt` 与 `up_cache.txt` 中与上述规则一致的 **KEY=VALUE**（与 `UP_*` 等选项共用一张表；**最后应用**，覆盖 XML 中的同名 `<vars>` 默认值）。

仅支持 **`@NAME@`** 占位符（类 CMake 子集）；未定义的 `NAME` 保持原文不替换。`up configure` 生成的 **`packages.md`** 中会列出各包/目标的 `<vars>` 默认值，并标出该 KEY 是否也出现在本次 configure 的选项映射中（便于核对是否被 `--opt` / 缓存覆盖）。

#### `when` 语义（configure 报错或跳过）

- 省略或空白：恒为真。
- **`true` / `false`**（大小写不敏感）。
- **`KEY==token`** 或 **`KEY!=token`**：对合并变量表中 `KEY` 的值与 `token` 做 **ASCII 大小写不敏感**比较。
- **单独一个标识符**：必须在合并表中有定义，再按「真值」判断（`0`、`false`、`off`、`no`、空串为假，其余为真）。
- 无法解析的裸表达式：**configure 失败**并给出错误信息。
- 为假时：该条目不参与编译源列表 / 不参与头文件 `include` 与安装规则收集。

#### `<config_files>`（类 configure_file 的最小子集）

- 块 **`<config_files>...</config_files>`**，内为 **`<file in="模板相对路径" to="输出相对路径"/>`**（`in`、`to` 均必填）。
- `in` 相对 `target.xml` 所在目录；`to` 相对 **`.intermediate/generated/<包名>/<目标名>/`**，且必须为安全相对路径（**不得**含 `..` 段、不得为绝对路径）。
- **`up configure`** 阶段读取模板、做 `@NAME@` 替换、写入生成目录，并将生成文件加入该目标的 **编译源列表**；同时对 `executable` / `static_library` / `shared_library` 把生成目录加入 **编译期 include 路径**。

**CMake 与 Ninja**：两后端均直接消费 **已由 up 写出的生成文件**（与普通源文件相同），当前实现**不**为此生成 CMake 的 `configure_file()`。若需要完整上游 CMake `configure_file` 语义，请使用 **`<cmake/>`** 子工程。

#### `<sources>` 上的 `when`

- 自闭合 **`<file from="..." when="..."/>`**、**`<glob from="..." when="..."/>`**（须带 `from`）。
- 或配对标签开标签上的 **`when`**。
- 为假时该源条目被跳过（glob 不展开）。

### 3.6 目标依赖 `<dependency name="..."/>`

- 自闭合 **` <dependency name="..."/> `**，可多次出现。
- **`name`** 支持两种形式：
  1. **`目标名`**：与本包内某一**库** target 同名，表示依赖本包该库。
  2. **`包名:目标名`**：依赖**其他包**中的某一库 target（该包须在 `package.xml` 中声明，且该 target 须在扫描集中存在）。
- **约束**：依赖指向的目标类型须为库目标（`static_library` / `shared_library` / `imported_static_library` / `imported_shared_library` / `imported_installed_static_library` / `imported_installed_shared_library`）；解析不到或类型不对则 **configure 失败**。

**生成行为（CMake 模式，当前实现）**：

- **可执行目标**：除显式 `<dependency>` 外，会 **PRIVATE 链接本包内全部库 target**；再将各 `<dependency>` 解析出的库名并入链接列表（去重、排序）。
- **库目标**：`<dependency>` 会参与依赖校验与「外包包内库」加入生成图；**当前不会**为静态库与静态库之间生成 `target_link_libraries` 链式链接。若甲库实现需调用乙库符号，需在工程层面自行保证链接顺序或合并目标（例如由最终可执行文件链接全部库）；详见 `test_projects` 中示例取舍。

---

## 4. 编码与路径

- 文件内容建议使用 **UTF-8**（与 [DESIGN.md](../DESIGN.md) 中包描述编码说明一致）。
- **configure** 会对相关路径做 **ASCII 路径**等校验；路径中含非 ASCII 等可能按实现直接报错，请避免。

---

## 5. configure 对「主包」与目标的额外要求

- **主包**：优先取 **cwd 等于其 `package.xml` 父目录`** 的那一个包；若无匹配，则取扫描到的**第一个**包作为主包（多包同扫时顺序依赖文件系统，建议单包目录下执行或明确文档化扫描顺序风险）。
- 主包下至少需要 **一个 target.xml**（可执行/库/导入库均可）；纯库包（例如只含 `imported_installed_*`）允许 configure/build。
- 同一 **`包名:目标名`** 在扫描结果中不得重复。

---

## 6. 测试与示例

仓库 **`test_projects/`** 下各子目录为完整示例（含跨包依赖、多库、分离的 `include/` / `src/` / `app/` / `test/` 等），见 [test_projects/README.md](../test_projects/README.md)。

---

## 7. 与实现的对应关系

| 话题 | 源码位置 |
|------|----------|
| 解析 `package.xml` / `target.xml` | `src/engine/xml/simple_xml.cpp`（`load_package_xml` / `load_target_xml`） |
| 依赖校验、生成后端 | `src/engine/commands/configure.cpp` |
| 变量合并、`@KEY@`、`when` | `src/engine/xml/var_subst.cpp` |
| 数据结构 | `src/engine/xml/simple_xml.hpp`（`PackageDesc` / `TargetDesc`） |

后续若引入 XSD/JSON Schema，可在本文件顶部增加版本号与变更记录。

---

## 11. `up project` 对 CMake 的默认生成行为（补充）

- 当探测到 CMake 工程时，`up project` 默认会：
  - 生成 `package.xml`（包含 `<cmake source_dir="..."/>`）
  - 尝试解析 `install(TARGETS ...)`，自动生成 `imported_installed_*` 的 `target.xml`（默认写到 `.targets/<name>/target.xml`）
  - 尝试解析 `find_package(...)`，自动写入包级 `<dependency .../>`：
    - `find_package(Xxx REQUIRED ...)` -> `<dependency name="xxx" optional="false"/>`
    - 其它 `find_package(Xxx ...)` -> `<dependency name="xxx" optional="true"/>`
- 规则边界：`up` / `up-gui` 不内置针对具体第三方项目（库名、仓库名、目录布局）的特判逻辑；行为必须由 `package.xml` / `target.xml` 显式配置驱动。
- 目标名优先使用 CMake target 名（必要时做 sanitize / 去重）。
- 包名默认优先取 `CMakeLists.txt` 中 `project(...)` 名；若无法解析则回退目录名（`--package-name` 仍最高优先）。
- `install(TARGETS ...)` 解析不到库规则时，仅生成 `package.xml` 并输出提示；可改用 `--legacy-cmake-parse` 或手工补充 `target.xml`。
- 对声明的依赖包，若可从其 `imported_installed_*` 目标推导出安装库/头路径，`configure` 会向主包上游 CMake 额外注入常见缓存变量（如 `<PKG>_LIBRARY` / `<PKG>_LIBRARY_DEBUG` / `<PKG>_INCLUDE_DIR` 等）以辅助 `find_package`；Windows 下优先使用 `implib` 或 `.lib`，避免把 `.dll` 注入链接库变量。
