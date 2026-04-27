# 入门教程：从 Hello World 到第三方库与移植

本教程按**步骤**扩展一个 `gz` 工程，与仓库内 **`test_projects/`** 示例一致处会给出引用。字段级语义以 **`package-target-xml-spec.md`** 与 **`gz spec`** 为准；命令行以 **`user-manual.md`** 与根目录 **`README.md`**（[`../../README.md`](../../README.md)）为准。**`doc/` 索引导航**：[`../README.md`](../README.md)。英文对照：[`../en/getting-started.md`](../en/getting-started.md)。

---

## 0. 约定与文档地图

- **包**：含 **`package.xml`** 的目录为包根；其下每个构建目标独占**一个子目录**，内有 **`target.xml`**。
- **扫描**：在含 `package.xml` 的树的**上一级**（或任意 cwd + `--scan`）执行 **`gz configure`**。
- **构建 / 运行**：`gz build` 须带 **`--build-dir-name`**；`gz run` 须带 **`--install-dir-name`**（值为 `.intermediate/install/` 下**架构子目录名**，见 **`script-tutorial.md` §1**）。
- **`gz` 可执行文件路径**：本教程默认 **`gz` 已在 PATH**。若你按根目录 **`README.md` / `user-manual.md`** 从源码构建并看到形如 **`.\_build\Release\gz.exe`** 的示例，其中的 **`_build`** 仅为 **`cmake -B`** 的示例目录名（亦可是 **`_build_gz`** 等），须与本机实际构建目录一致；详见 **`user-manual.md`**「**0. 10 分钟快速上手**」中的构建目录说明。
- **产品方向**：**纯手写** **`package.xml` / `target.xml`**（见 **`package-target-xml-spec.md`** 文首）。**`gz reverse` 子命令已移除**。
- **延伸阅读**：[cli-reference.md](cli-reference.md)（**`gz` 命令行参数与工作模式**）、[internal-variables.md](internal-variables.md)（变量）、[script-messages.md](script-messages.md)（**`trigger` 消息表**与脚本 var）、[script-tutorial.md](script-tutorial.md)（预处理 / Qt）、[package-target-xml-spec.md](package-target-xml-spec.md)（`when` / `config_files` / `<dependency>`）；英文目录见 [`../en/`](../en/）。

---

## 第 1 步：Hello World（单包单可执行目标）

目录示例：

```text
hello_pkg/
  package.xml
  app/
    target.xml
    main.cpp
```

**`package.xml`**

```xml
<?xml version="1.0" encoding="UTF-8"?>
<package name="hello_pkg" version="0.1.0">
</package>
```

**`app/target.xml`**

```xml
<?xml version="1.0" encoding="UTF-8"?>
<target name="hello" type="executable">
  <sources>
    <file>main.cpp</file>
  </sources>
</target>
```

**`app/main.cpp`**：`int main() { return 0; }`（或打印一行字符串）。

执行：`gz configure` → `gz build --build-dir-name default` → `gz print-build-dir-name --build-dir-name default` → `gz run --install-dir-name <arch 名> hello`。

---

## 第 2 步：添加静态库，并让可执行文件使用它

在同一包内再建子目录 **`mylib/`**，类型 **`static_library`**；可执行目标通过 **`<dependency name="库目标名"/>`** 显式链接，或依赖默认行为（见下）。

**要点（当前 CMake 生成语义）**：

- 若可执行文件**没有任何** `<dependency>`，configure 会按 **`GZ_TARGET_DYNAMIC_LIBRARY`** 等策略**自动链接本包内符合条件的库**；若存在**任意**显式 `<dependency>`，则**只**使用你声明的链接关系（见 **`package-target-xml-spec.md` §3.6**）。
- 库目标的 **`<headers>`** 与 **目标级 `<config_files>`** 生成目录会进入该库的 **include 路径**；在 CMake 后端中，原生 **`static_library` / `shared_library`** 使用 **`target_include_directories(... PUBLIC ...)`**，因此**链接该库**的可执行文件一般可直接 **`#include`** 库公开头文件及**该库** `config_files` 生成物（参见 **`test_projects/hello_simple_lib/`**）。

参考：`test_projects/hello_simple_lib/`（包级 `defines` / `config_files` + 静态库 + 工具 exe + 测试）。

**`mylib/target.xml` 骨架**

```xml
<?xml version="1.0" encoding="UTF-8"?>
<target name="mylib" type="static_library">
  <sources>
    <file>mylib.cpp</file>
  </sources>
  <headers>
    <dir from="."/>
  </headers>
</target>
```

**`app/target.xml` 增加依赖**

```xml
<target name="hello" type="executable">
  <sources>
    <file>main.cpp</file>
  </sources>
  <dependency name="mylib"/>
</target>
```

---

## 第 3 步：添加编译宏 `<defines>`

- **包级** `package.xml` 内 **`<defines>...</defines>`**：作用于本包内所有**参与编译**的原生目标（先于目标级宏合并，目标级可覆盖同名宏）。
- **目标级** `target.xml` 内 **`<defines>`**：仅该目标；**`value`** 仅允许字母数字及 **`._+-/`**（无空格），见规范 §3.4。

示例：

```xml
<defines>
  <define name="USE_FEATURE"/>
  <define name="APP_VERSION" value="1.0.0"/>
</defines>
```

---

## 第 4 步：添加 `config_files`（生成头文件）

分两种落点（**不要混用路径规则**）：

| 位置 | `in` 相对谁 | 生成目录（相对 `.intermediate/generated/`） | 谁收到生成物 |
|------|-------------|-----------------------------------------------|--------------|
| **包级** `package.xml` | 包根 | `<包名>/_package/` | 包内**每个** exe/静态库/动态库目标的**源列表 + include** |
| **目标级** `target.xml` | 该 `target.xml` 所在目录 | `<包名>/<目标名>/` | **仅该目标**；其它目标若需包含，应 **`<dependency>`** 链接到提供 `PUBLIC` include 的库等 |

模板内使用 **`@NAME@`** / **`${NAME}`** 与可选 **`#cmakedefine`**；合并变量栈见规范 §3.5。

示例参考：`test_projects/hello_simple_lib/pkg_gen.hpp.in`（包级）、`hello_simple_lib/hello_simple_lib_gen.hpp.in`（目标级）。

---

## 第 5 步：操作系统与配置判断（`when` + 内置变量）

- **`when`** 仅对**已实现**的标签生效（**`<sources>`** 的 `<file>`/`<glob>`，**`<headers>`** 的 dir/file/glob）；**不要**写在 `<defines>` / `<assets>` 等上并指望生效（规范 §3.5 已列出未实现项）。
- 常用内置键：**`GZ_OS`**（`windows` / `linux` / `darwin`）、**`GZ_CONFIG`**（`debug` / `release`）、**`GZ_TARGET_BUILD_SYSTEM`**（`cmake` / `ninja`）等，见 **`internal-variables.md`**。
- **语法限制**：无 `&&` / `||`；可用 **`KEY == token`**、**`KEY != token`** 或**单个键**的真值判断。

示例：仅 Windows 编译某源文件：

```xml
<file from="win_only.cpp" when="GZ_OS==windows"/>
```

---

## 第 6 步：跨包依赖（多包扫描）

1. 子包目录有自己的 **`package.xml`**（不同 **`name`**）。
2. 父包 **`package.xml`** 声明 **`<dependency name="子包包名"/>`**（可选 **`optional="true"`**）。
3. 子包内的库目标 **`target name="bar"`**，父包可执行文件写 **`<dependency name="子包包名:bar"/>`**。

示例：**`test_projects/hello_parent_child/`**（父包依赖嵌套子包）。

---

## 第 7 步：第三方库 — 预编译二进制（`imported_*` + `<prebuilt>`）

适用于：Vendor 提供 **`.lib` / `.a` / `.dll`+.lib / `.so`**，无需从源码编译。

1. 新建目标 **`type="imported_static_library"`** 或 **`imported_shared_library`**。
2. 填写 **`<prebuilt .../>`**：`imported_static_library` 常用 **`import_lib=`** 或 **`location=`**；Windows 下 **`imported_shared_library`** 需要 **`dll=`**（或 **`location=`** 指向 `.dll`）与 **`import_lib=`**（`.lib`）。详见 **`package-target-xml-spec.md`** 类型表。
3. 用 **`<headers>`** 指到第三方头文件目录，便于消费者 **include**。
4. 可执行文件 **`<dependency name="该导入目标名"/>`**。

示例：**`test_projects/prebuilt_import_demo/`**（`import_stub_lib` + `app`）。

**注意**：路径相对 **`target.xml` 所在目录** 解析；提交到仓库的预置库需注意**平台架构**与许可证。

---

## 第 8 步：第三方库 — 包外 `install` + 手写 **`imported_installed_*`**

适用于：上游是 **CMake**（或其它构建），你已在 **包外** 把库 **`install` 到与本包一致的安装前缀**（或与 **`artifact=`** 相对关系一致的路径），希望在 `gz` 里只**声明**产物。

1. **不要**依赖自动逆向：在仓库旁或 CI 中单独对上游执行 **`cmake --install`**（或上游官方安装包），得到 `.lib` / `.so` / 头文件等固定布局。
2. 在本包 **`target.xml`** 中声明 **`imported_installed_static_library` / `imported_installed_shared_library`**，用 **`<install artifact="..." />`**（及 Windows 下 **`implib`** 等）指向**相对 `CMAKE_INSTALL_PREFIX`** 的安装路径；见 **`package-target-xml-spec.md`** 类型表。
3. 用 **`<headers>`** 或 **`interface_include`**（若适用）暴露给消费方。
4. 可执行目标 **`<dependency name="…"/>`** 链接该导入目标。

**对照**：仓库 **`test_projects/smoke_minimal_exe/`** 为**最小可执行目标**冒烟示例。

---

## 移植专题 A：把现有 **CMake** 工程迁到 `gz`（纯手写）

**推荐路径**：

1. **阅读上游**：列出源文件、公共头、宏、`link_libraries`、安装规则；在 `gz` 侧为每个编译单元建 **`target.xml`**，用 **`<sources>` / `<headers>` / `<defines>` / `<dependency>`** 重写依赖图。
2. **第三方 / 子树**：若在包外已安装，用 **`imported_installed_*` + `<install …/>`**；若只有预编译 SDK，用 **`imported_*` + `<prebuilt>`**（见第 7 步）。
3. **渐进迁移**：可先让主程序依赖 **`imported_*`** 包装现有二进制，再逐步把源码移入 **`static_library`** 目标。
4. **历史说明**：旧版若曾依赖已移除的子命令或内嵌构建描述，请按 **`gz spec`** 与 **`package-target-xml-spec.md`** 改为纯手写 XML。

---

## 移植专题 B：把 **Qt** 程序迁到 `gz`

**思路**：Qt 由 **官方安装或包外 CMake install** 提供；`gz` **只手写**源、宏、**`<dependency>`** 指向 **`imported_*`** 或本包自编译库；**moc / uic / rcc** 用 **`<preprocess>`** 或包在 **`static_library`** 里以规避 CMake 后端对 **exe 源级 preprocess** 的限制。

| 原 CMake/Qt 概念 | 在 `gz` 中的对应 |
|------------------|------------------|
| `target_sources` / `add_executable` | **`<sources><file>…`** |
| `target_include_directories` | **`<headers>`**（安装 + include 推导） |
| `target_compile_definitions` | **`<defines>`** |
| `find_package(Qt…)` + 链接 | **`<dependency>`** 指向 **`imported_*`**（或本包 **`static_library`/`shared_library`**）；**手写** Qt 安装前缀下的库与头 |
| `AUTOMOC` / `AUTOUIC` / `AUTORCC` | **手写** **`moc`/`uic`/`rcc`** 命令行，见 **`script-tutorial.md` §8** |

---

## 移植专题 C：第三方库的常见形态（选型表）

| 形态 | 建议做法 |
|------|----------|
| **Header-only** | 无链接目标：用 **`<headers>`** 暴露 include；或 **`asset_bundle`** / 安装规则按项目需要。 |
| **官方提供 CMake + install** | **包外** `cmake --install` 到约定前缀，再 **`imported_installed_*` + `<install …/>`** 手写描述。 |
| **仅预编译 .lib/.dll/.so** | **`imported_static_library` / `imported_shared_library`** + **`<prebuilt>`** + **`<headers>`**。 |
| **需打补丁、多步配置** | 仍在**包外**脚本化上游构建；`gz` 侧用 **`preprocess`** 或 vendored 源码目标承接生成物，见 **`script-tutorial.md`**。 |
| **与本仓库另一 `gz` 包协作** | 多包 **`<dependency name="包名"/>`** + **`包名:目标名`**。 |

---

## 检查清单（发布前）

- [ ] **`gz configure`** 无报错；查看 **`.intermediate/build/<叶子>/packages.md`** 中变量与依赖是否符合预期。  
- [ ] **`when`** 仅用于已支持标签；复杂条件用多个键 + **`--opt`** 预置。  
- [ ] 跨包引用已在 **`package.xml`** 声明 **`dependency`**。  
- [ ] **`gz run`** 使用正确的 **`--install-dir-name`**（与 **`gz_cache.txt` `arch=`** 一致）。  
- [ ] 第三方许可证与预置二进制是否允许分发。  

更多命令与目录约定：**`user-manual.md`**。
