# gz 用户手册（GroundZero）

本手册面向第一次接触 `gz` 的用户，目标是让你从零开始理解并完成一次完整使用（CLI 与 gz-gui 两条路径）。

> **文档索引**（`doc/zh` / `doc/en` 全部入口表）：[`../README.md`](../README.md)

- **`gz` 命令行参数与工作模式**（逐项说明与范例）：[`cli-reference.md`](cli-reference.md)（英文入口：[`../en/cli-reference.md`](../en/cli-reference.md)）
- 分步入门（Hello World → 库/宏/条件/第三方/移植）：[`getting-started.md`](getting-started.md)（英文对照页：[`../en/getting-started.md`](../en/getting-started.md)）
- 产品方向（**纯手写** XML）：[`package-target-xml-spec.md`](package-target-xml-spec.md) 文首（英文：[`../en/package-target-xml-spec.md`](../en/package-target-xml-spec.md)）
- 设计文档：[`DESIGN.md`](../../DESIGN.md)
- XML 规范：[`package-target-xml-spec.md`](package-target-xml-spec.md)
- 示例工程：[`test_projects/README.md`](../../test_projects/README.md)

---

## 功能总览

| 功能域 | 主要功能 | 说明 |
|---|---|---|
| 运行形态与最小依赖 | 命令行（CLI）+ 图形界面（GUI），只依赖操作系统与本机工具链 | 同一套能力同时提供 `gz.exe` 与 `gz-gui.exe` 两种入口，GUI 本质调用 CLI；不依赖额外服务端，在本机环境（编译器、构建工具）即可运行完整流程。 |
| 包+目标的描述结构 | 包级/目标级依赖关系图 + `package.xml`/`target.xml` | 把工程管理抽象为“包 + 目标 + 依赖关系”，并通过 XML 数据描述构建行为；支持同包与跨包引用，不把依赖散落在脚本中，结构更清晰、可迁移。 |
| 支持多种目标类型 | 可执行程序 / 库 / 插件 / 资源文件 | 支持 `executable`、`library`（随 **`GZ_TARGET_DYNAMIC_LIBRARY`** 解析为静/动库）、`static_library`、`shared_library` 等目标类型，并可组织插件与资源文件等工程产物。 |
| 配置过程支持变量操作 | 全局变量 + 用户自定义变量 + 变量驱动 target 行为 | 系统会注入默认全局变量，用户可定义变量并在配置阶段参与目标行为控制，例如按 `win32/linux` 条件切换源码、依赖或构建选项。 |
| 基于现有建造系统和编译器环境 + 不设立复杂规则不增加学习难度 | 复用 CMake / Ninja 与现有编译器链路 | 不额外发明复杂 DSL 或新规则体系，尽量沿用团队已有构建习惯，降低迁移与学习成本。 |
| 全流程命令 + 全平台开发 + 全平台发布 | configure / build / test / run / pack | 覆盖从扫描、构建、测试到运行与打包的完整流程。**推荐**纯手写 **`package.xml` / `target.xml`**（见 **`package-target-xml-spec.md`** 文首）。 |
| 最简单的外部依赖关系扩展 | 通过设置 `scan dir` 搜索外部包建立依赖关系 | 当前可把外部包目录加入扫描来源来建立包依赖；后续可扩展为 `git clone` 或下载并解压 `zip` 后自动纳入 `scan` 来源。 |
| 可扩展元编程工具接入 | 支持 `moc` / `uic` / `rc` 等工具模式 | 可将代码生成工具接入包体系，支持输入文件转换、生成产物管理与测试验证。 |

## 0. 10 分钟快速上手

如果你只想尽快跑起来，按下面做即可。

### 步骤 1：构建 `gz.exe`

在仓库根目录执行：

```powershell
cmake -S . -B _build -G "Visual Studio 17 2022" -A x64
cmake --build _build --config Release
```

> **构建目录**：**`_build`** 仅为示例；请与你执行 **`cmake -B`** 时使用的目录名一致（例如 **`_build_gz`**）。下一步命令中的 **`.\_build\Release\`** 前缀需同步替换。

### 步骤 2：跑通示例工程

```powershell
.\_build\Release\gz.exe configure --scan test_projects
$ARCH = .\_build\Release\gz.exe print-build-dir-name
.\_build\Release\gz.exe build --build-dir-name default
.\_build\Release\gz.exe test --install-dir-name $ARCH
.\_build\Release\gz.exe run --install-dir-name $ARCH hello_demo
```

**说明：** `build` 必须使用 **`--build-dir-name`**（与 `configure` 使用的叶子名一致，未指定 configure 时一般为 **`default`**）。`run` / `test` / `pack` 必须使用 **`--install-dir-name`**，其值为 **`.intermediate/install/` 下的子目录名**，通常等于 **`gz print-build-dir-name`**（或 `gz_cache.txt` 里的 **`arch=`**），**不要**误用构建叶子名 `default`。

### 步骤 3：检查结果

- 构建与生成目录：`.intermediate/build/<叶子>/`（示例为 **`default/`**）
- 安装结果目录：`.intermediate/install/<arch>/`（**`<arch>`** 见上一步 `$ARCH`）
- 如果运行成功，你会看到 `hello_demo` 的输出日志（含 `hello_foo`、`hello_simple_lib`、`rock_stack` 调用）

### 步骤 4：继续深入

- 读概念与细节：从本文 **第 1 节** 开始
- 查 XML 字段定义：[`package-target-xml-spec.md`](package-target-xml-spec.md)

---

## 1. 概述：这是什么，为什么做

`gz`（GroundZero）是一个“用数据描述构建行为”的包与构建编排工具。你通过 `package.xml` 和 `target.xml` 描述包、目标、依赖，`gz` 负责：

1. 扫描描述文件并建立包/目标关系
2. 生成后端构建文件（当前主要是 CMake，也支持 Ninja）
3. 执行构建、测试、运行、打包

### 为什么做这个软件

在 C/C++ 项目里，常见痛点是：

- 多个库与可执行程序的关系越来越复杂
- 依赖声明分散在不同脚本中，可读性差
- 构建后端切换成本高

`gz` 的核心思路是“数据即行为”：

- 用 XML 把结构和依赖写清楚
- 用统一命令驱动 configure/build/test/run/pack
- 让工程组织更可视、更可迁移

### 和仓库脚本的边界

仓库根目录的 `build.py` / `install.py` 只负责构建并安装宿主工具 `gz.exe` / `gz-gui.exe`（可选安装 `gz.lib` 开发库）。它们**不会**构建 `test_projects` 里的示例包。

源码结构上，`gz` 已拆分为：

- `GroundZero/exe/`：CLI 入口与命令分发（`gz.exe`）
- `GroundZero/lib/`：核心实现（`gz.lib`，含 `engine` / `infra`）

---

## 2. 细节逻辑：系统如何工作

下面是 `gz` 的主流程：

1. **扫描**：在 `cwd`（或 `--scan` 指定目录）递归查找 `package.xml` 和 `target.xml`
2. **归属**：每个 `target.xml` 归属到路径上最近的 `package.xml`
3. **校验**：
   - 包依赖是否在扫描集中存在
   - 目标依赖是否能解析到库目标
4. **生成**：在 **`.intermediate/build/<叶子>/`** 生成构建文件（**`<叶子>`** 由 **`configure --build-dir-name`** 决定，省略时为 **`default`**），并写入 **`gz_cache.txt`**（含 **`arch=`**）
5. **执行**：`build` 将产物安装到 **`.intermediate/install/<arch>/`**（**`<arch>`** 来自缓存，通常 **不等于** **`<叶子>`**）
6. **后续**：`run` / `test` / `pack` 基于安装与构建元数据继续工作（CLI 上通过 **`--install-dir-name <arch>`** 指向安装前缀）

### 目录流转（相对执行命令时的 cwd）

- `.intermediate/build/<叶子>/`：生成与构建目录（含 `gz_cache.txt`）
- `.intermediate/install/<arch>/`：安装目录（`bin/`、`include/`）
- `.intermediate/pack/<arch>/`：打包产物目录

### 关于后端

- **CMake 模式**：生成 `CMakeLists.txt` 再调用 CMake
- **Ninja 模式**：直接生成 `out/build.ninja`

---

## 3. 概念介绍

### 3.1 package 与 target

- **package**：由 `package.xml` 定义，表示一个包（带包级依赖）
- **target**：由 `target.xml` 定义，表示构建目标
  - `executable`
  - `library`（configure 时按 **`GZ_TARGET_DYNAMIC_LIBRARY`** / **`GZ_DYNAMIC_LIBRARY`** 决定静态或动态原生库）
  - `static_library` / `shared_library`（分别**强制**静态或动态，不受上述全局偏好覆盖）
  - `asset_bundle`（仅安装资源，无编译单元）
  - `prebuilt_static_library` / `prebuilt_shared_library`：预置二进制 SDK，配合 `<prebuilt .../>`（路径相对 `target.xml` 目录）。完整示例见 [`test_projects/prebuilt_static_stub/`](../../test_projects/prebuilt_static_stub/README.md)（内含已提交的 MSVC `stub_import.lib`；子库输出目录使用 `lib/import/` 以免被仓库根 `.gitignore` 的 `dist/` 规则误忽略）。
  - `imported_installed_static_library` / `imported_installed_shared_library`：声明**已安装到本包安装前缀**（`CMAKE_INSTALL_PREFIX`）下的库；**推荐**在包外先 `cmake --install`（或等价安装），再手写 `<install artifact="..."/>`（`artifact` / 可选 `interface_include` 均相对 `CMAKE_INSTALL_PREFIX`）。Windows 下 shared 还需 `implib="..."`。此类目标不能与磁盘预置的 `<prebuilt/>` 混用。

规则约束：`gz` / `gz-gui` 不对任何具体第三方代码库做内置特判；依赖关系、安装产物、头文件目录等信息都必须通过 `package.xml` / `target.xml` 显式声明。

### 3.1b `CMAKE_PREFIX_PATH` 合并

聚合工程会带上 **`CMAKE_PREFIX_PATH`**，由以下部分**去重后**拼接（分号分隔，与 CMake 列表一致）：

1. 当前 `cwd` 下本配置的安装前缀 **`.intermediate/install/<arch>/`**（始终排在最前，便于优先找到本工作区已安装的包）。
2. **`--opt GZ_CMAKE_PREFIX_PATH=路径1;路径2`** 中的额外前缀（如第三方 SDK 的 CMake 包根）；相对路径按 **`cwd`** 解析。

此外，当主包声明依赖且依赖包存在 `imported_installed_*` 目标时，`configure` 会尝试把这些已知安装信息映射为常见 `find_package` 缓存变量并传给主包聚合 CMake（例如 `<PKG>_LIBRARY` / `<PKG>_LIBRARY_DEBUG` / `<PKG>_INCLUDE_DIR` 等），以降低仅靠 `CMAKE_PREFIX_PATH` 仍解析不稳的情况。Windows 下会优先选择 `implib` 或 `.lib`，避免把 `.dll` 误传入链接变量。

### 3.2 依赖层次

- **包级依赖**：写在 `package.xml` 的 `<dependency name="..."/>`
- **目标级依赖**：写在 `target.xml` 的 `<dependency name="..."/>`
  - 同包引用：`<dependency name="myLib"/>`
  - 跨包引用：`<dependency name="otherPkg:otherLib"/>`
  - **可见性（CMake 后端）**：可写 **`visibility="private|public|interface"`**（可选，默认 **`private`**，大小写不敏感），对应 `target_link_libraries` 的 **`PRIVATE` / `PUBLIC` / `INTERFACE`**。**可执行目标**若对某依赖写 **`interface`**，configure 会失败。库目标之间的链式 `target_link_libraries` 当前不生成，多库符号由消费方（例如 exe）自行链接齐全。
  - **`when`** 目前**不**作用于 `<dependency/>`；哪些标签支持条件、表达式语法以 **`gz spec`** 与 **[package-target-xml-spec.md](package-target-xml-spec.md)** 为准。

### 3.3 `<headers>` 头文件块（from/to）

`<headers>` 里统一使用自闭合条目：

- `<dir from="..." to="..."/>`
- `<file from="..." to="..."/>`
- `<glob from="..." to="..."/>`

语义：

- `from`：相对 `target.xml` 所在目录
- `to`：安装到 `include/` 下的子目录（可省略）

旧写法 `<dir>...</dir>` 已不支持。

### 3.4 arch 标签

`<arch>` 不是单纯 CPU 字符串，而是组合信息（系统、CPU、构建后端、工具链、Debug/Release、CRT 等），用于区分不同构建配置目录。

---

## 4. 主要使用方法

下面按“先构建工具，再使用工具”展开。

### 4.1 准备环境

- CMake 3.20+
- C++17 编译器
- Windows 推荐 Visual Studio 2022 + MSVC

### 4.2 构建 gz（仓库根目录）

方式 A：手工 CMake

```powershell
cmake -S . -B _build -G "Visual Studio 17 2022" -A x64
cmake --build _build --config Release
```

（**`_build`** 仅为示例目录名，含义与替换规则见上文「**0. 10 分钟快速上手**」中的构建目录说明。）

历史上曾使用子命令 **`project`** / 选项 **`--project-dir`**，当前版本已删除且无别名。

方式 B：Python 脚本

```powershell
python build.py
python install.py --prefix dist
python package.py
```

### 4.3 CLI 快速上手（推荐先跑示例）

在仓库根目录运行：

```powershell
.\_build\Release\gz.exe configure --scan test_projects
$ARCH = .\_build\Release\gz.exe print-build-dir-name
.\_build\Release\gz.exe build --build-dir-name default
.\_build\Release\gz.exe test --install-dir-name $ARCH
.\_build\Release\gz.exe run --install-dir-name $ARCH hello_demo
```

常用命令（与 **`gz --help`** 一致）：

- `gz configure [--build-dir-name <叶子>] [--scan <dir>]... [--opt KEY=VALUE]...`
- `gz build --build-dir-name <叶子>`
- `gz run --install-dir-name <名> <target-name>`
- `gz test --install-dir-name <名> [test-target-name]`
- `gz pack --install-dir-name <名>...`
- `gz list [--format tree|json|xml] [--xml <path>] [--json <path>] [--quiet]`
- `gz spec` / `gz print-build-dir-name`

`list` 参数行为摘要：

- stdout 由 `--format` 决定：`tree`（默认）/`json`/`xml`。
- `--xml <path>` 与 `--json <path>` 是文件导出，可与任意 stdout 格式并存。
- `--quiet` 抑制树形文本与导出提示，但不抑制 `--format json|xml` 的主载荷。
- 某些组合只告警不失败（例如 `--format json + --xml <path>`）。

### 4.4 在单包目录工作

如果你已在某个包目录（例如 `test_projects\rock_stack`）并已将 `gz` 加入 PATH，可直接：

```powershell
gz configure
$ARCH = gz print-build-dir-name
gz build --build-dir-name default
gz test --install-dir-name $ARCH
gz run --install-dir-name $ARCH rock_app_one
```

### 4.5 gz-gui 快速上手

`gz-gui` 是图形壳（Windows：Win32；Linux：GTK3；macOS：Cocoa），核心仍是调用同目录下的 `gz` / `gz.exe`。

建议流程：

1. 打开 `gz-gui`
2. 在“编译环境设置”中检查/选择：
   - Build system
   - Compiler
   - 可选 Android / emsdk 路径
3. 选择工作目录（package 或 test_projects 根）
4. 点击 `Configure`
5. 点击 `Build`
6. 视需要执行 `Test` / `Run`

说明：GUI 会把设置转成 `--opt` 参数传给 `gz.exe`。

### 4.6 常见工作流模板

#### 模板 A：新增一个库目标

1. 新建子目录（例如 `myLib/`）
2. 放入 `target.xml` 与源码
3. 在需要的可执行目标里声明 `<dependency name="myLib"/>`
4. 按 §4.3 的约定依次执行 `configure`、`print-build-dir-name`、`build --build-dir-name`、`test`/`run`（`build`/`run`/`test` 的参数为必填）

#### 模板 B：新增跨包依赖

1. 在 `package.xml` 声明包依赖
2. 在 `target.xml` 用 `otherPkg:otherTarget` 引用
3. 确保 configure 扫描范围包含两个包

#### 模板 C：调试 `<headers>` 安装布局

1. 用 `from/to` 声明 `dir/file/glob`
2. `gz configure`，再 `gz build --build-dir-name default`（或与 configure 一致的叶子名）
3. 检查 `.intermediate/install/<arch>/include/` 是否符合预期（**`<arch>`** 见 `gz print-build-dir-name` 或 `gz_cache.txt`）

#### 模板 D：导入第三方 CMake SDK（手写描述）

1. 在**包外**对上游执行 **`cmake --install`**（或安装官方 SDK），得到确定的 `lib/`、`include/` 等布局。
2. 在本工作区**手写** **`package.xml`** 与 **`target.xml`**：用 **`imported_installed_*` + `<install …/>`** 或 **`prebuilt_*` + `<prebuilt>`** 描述库与头文件（见 **`getting-started.md`** 第 7～8 步）。
3. **`gz configure` / `gz build`**（见 §4.3）；在消费方 **`target.xml`** 用 **`<dependency name="pkg:target"/>`** 引用。
4. 纯库包（无 executable）也支持 configure/build。

---

## 5. 常见问题问答（FAQ）

### Q1：`configure` 报“required dependency package not found in scan set”

**原因**：目标引用了外包，但扫描范围里没有该包。  
**处理**：

- 扩大 `--scan` 范围，确保依赖包被扫描到
- 并确认 `package.xml` 已声明对应包级依赖

### Q2：路径相关报错（非 ASCII 路径）

**原因**：当前实现对路径有 ASCII 限制，避免后端工具链不一致问题。  
**处理**：把工程路径迁到仅英文路径目录后重试。

### Q3：`run` 找不到目标

**常见原因**：

- 未传 **`--install-dir-name`**，或传入的名字不是 **`.intermediate/install/<arch>/`** 这一段（误把 **`default`** 当成安装目录名）
- 目标名写错
- 尚未 `build` 成功
- 当前目录对应的是另一个 package

**建议**：用 **`gz print-build-dir-name`** 得到 **`<arch>`**，执行 **`gz run --install-dir-name <arch> <name>`**；或先 **`gz test --install-dir-name <arch>`** 核对安装树。

### Q4：`test` 没有发现测试

`gz` 当前通过 CTest 执行测试；通常可执行目标会被注册为测试条目。若无测试：

- 确认 **`gz test --install-dir-name <arch>`** 中的 **`<arch>`** 与本次 `configure`/`build` 一致
- 确认 configure/build 成功
- 确认目标确实是 `executable`
- 在正确的包/扫描范围下运行

### Q5：`<headers>` 嵌套 `<dir>` 旧写法报错

如果你还在用嵌套目录旧写法：

```xml
<headers>
  <dir>../../include/xxx</dir>
</headers>
```

请改为 `from/to` 自闭合：

```xml
<headers>
  <dir from="../../include/xxx" to="xxx"/>
</headers>
```

`file`、`glob` 同理都用 `from/to`。

### Q6：为什么单包 configure 过不了，整仓扫描却可以

因为单包目录通常看不到跨包依赖目标。对于跨包依赖场景，请在更高层目录执行并用 `--scan` 覆盖所有相关包。

### Q7：纯库包（没有 executable）能不能 configure/build？

可以。当前实现已支持“仅库目标”的包（包括 `imported_installed_*` 包装目标）。  
`configure` 只要求主包至少有一个 `target.xml`，不再强制必须有可执行目标。

---

## 附：建议阅读顺序

1. 本文（用户手册）
2. [`README.md`](../../README.md)（命令总览与仓库入口）
3. [`package-target-xml-spec.md`](package-target-xml-spec.md)（字段级规范）
4. [`DESIGN.md`](../../DESIGN.md)（设计背景与路线）
5. [`test_projects/README.md`](../../test_projects/README.md)（可执行示例）

