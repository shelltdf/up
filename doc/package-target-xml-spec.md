# `package.xml` 与 `target.xml` 规范说明

本文档描述 **up** 当前实现对两种描述文件的**解析约定**与 **configure** 阶段的**语义约束**。实现采用轻量正则扫描（见 `src/simple_xml.cpp`），**不是**完整 XML 校验器：建议仍写成良构 XML，并遵守下列可识别形态。

---

## 1. 文件角色与放置

| 文件 | 放置位置 | 作用 |
|------|----------|------|
| **`package.xml`** | 每个**独立包**的根目录（与包内 `target.xml` 树的上层一致） | 声明包名、版本、**包级**依赖（其他包名）。 |
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

### 3.3 头文件路径 `<includes>` / `<dir>`

- 任意 **` <dir>相对路径</dir> `** 形式的片段会被收集为 **include 目录**（同样**相对于 `target.xml` 所在目录**）。
- 生成 CMake 时，对**库**目标会生成 **`target_include_directories(... PUBLIC ...)`**；对**可执行**目标为 **`PRIVATE`**（若该 target 自身声明了 `dir`）。

同样，**不强制**要求外层 `<includes>` 包裹；建议写上以便阅读。

### 3.4 目标依赖 `<dependency name="..."/>`

- 自闭合 **` <dependency name="..."/> `**，可多次出现。
- **`name`** 支持两种形式：
  1. **`目标名`**：与本包内某一**库** target 同名，表示依赖本包该库。
  2. **`包名:目标名`**：依赖**其他包**中的某一库 target（该包须在 `package.xml` 中声明，且该 target 须在扫描集中存在）。
- **约束**：依赖指向的目标类型须为 **`static_library` 或 `shared_library`**；解析不到或类型不对则 **configure 失败**。

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
- 主包下须至少有 **一个可执行** target，否则 configure 失败。
- 同一 **`包名:目标名`** 在扫描结果中不得重复。

---

## 6. 测试与示例

仓库 **`test_projects/`** 下各子目录为完整示例（含跨包依赖、多库、分离的 `include/` / `src/` / `app/` / `test/` 等），见 [test_projects/README.md](../test_projects/README.md)。

---

## 7. 与实现的对应关系

| 话题 | 源码位置 |
|------|----------|
| 解析 `package.xml` / `target.xml` | `src/simple_xml.cpp`（`load_package_xml` / `load_target_xml`） |
| 依赖校验、生成 CMake | `src/configure.cpp` |
| 数据结构 | `src/simple_xml.hpp`（`PackageDesc` / `TargetDesc`） |

后续若引入 XSD/JSON Schema，可在本文件顶部增加版本号与变更记录。
