# gz 用户手册（GroundZero）

本手册面向**第一次接触** `gz` 的用户：建立**心智模型**、说明**与其它文档的分工**、给出 **gz-gui** 简介、**常见问题（FAQ）** 与少量**工作流模板**。  
**刻意不重复**维护：argv 逐项说明、退出码全表、XML 字段级定义、脚本 `trigger` 全表等——它们由下方「文档分工」中的**专题页**单一维护，本文只给出链接与易错提醒。

> **文档索引**（`doc/zh` / `doc/en` 全部入口表）：[`../README.md`](../README.md)

### 文档分工（请选对入口）

| 你想查… | 打开 |
|--------|------|
| **子命令、每个开关、cwd、中间目录、退出码、PowerShell 范例** | [`cli-reference.md`](cli-reference.md)（[`../en/cli-reference.md`](../en/cli-reference.md)） |
| **从 Hello World 到库 / 第三方 / 移植** 的分步教程与可复制 XML | [`getting-started.md`](getting-started.md)（[`../en/getting-started.md`](../en/getting-started.md)） |
| **`package.xml` / `target.xml` 字段**、`when`、合并顺序、`prebuilt` / `install` | [`package-target-xml-spec.md`](package-target-xml-spec.md) + **`gz spec`** |
| **`gz_cache.txt`、`GZ_*`、内置变量、`arch` 含义** | [`internal-variables.md`](internal-variables.md) |
| **脚本 `trigger`、派发阶段** | [`script-messages.md`](script-messages.md) |
| **预处理 / Qt 等脚本实操** | [`script-tutorial.md`](script-tutorial.md) |
| **从 CMake 静态反解为 `package.xml` / `target.xml`（`gz_reverse_cmake`）** | [`gz-reverse-cmake.md`](gz-reverse-cmake.md)（[`../en/gz-reverse-cmake.md`](../en/gz-reverse-cmake.md)） |
| **仓库命令表、从源码构建** | 根目录 [`README.md`](../../README.md) |
| **设计与中间目录大图** | [`DESIGN.md`](../../DESIGN.md) |
| **可跑示例树** | [`test_projects/README.md`](../../test_projects/README.md) |

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

## 0. 快速入口与构建目录约定

### 构建目录名（`_build` 仅为示例）

各文档中的 **`.\_build\Release\gz.exe`**：目录 **`_build`** 只是 **`cmake -B`** 的**示例名**（也可以是 **`_build_gz`** 等）；**所有命令里的该前缀须与本机实际构建输出目录一致**。亦可用根目录 **`python build.py`** / **`install.py`** 装配宿主工具（见 **`README.md`**）。

### 第一次跑通（不重复粘贴长命令）

1. **构建 `gz` 本体**：根目录 **`README.md`** 或 **`build.py`**。  
2. **跑 Hello World 或 `test_projects`**：按 **[getting-started.md](getting-started.md)** 从第 1 步做；示例树说明见 **[test_projects/README.md](../../test_projects/README.md)**。  
3. **易混参数**：**`gz build`** 必须 **`--build-dir-name <叶子>`**；**`gz run` / `test` / `pack`** 必须 **`--install-dir-name <名>`**，其中 **`<名>`** 是 **`.intermediate/install/` 下的子目录名**（通常即 **`gz print-build-dir-name`** 或 **`gz_cache.txt`** 的 **`arch=`**），**不是** 构建叶子 **`default`**。完整说明与范例命令见 **[cli-reference.md](cli-reference.md)** 第 2 节；排错见本文 **§5 FAQ Q3**。

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

## 2. 细节逻辑：系统如何工作（鸟瞰）

1. **扫描**：在 **cwd**（或 **`--scan`**）递归查找 **`package.xml`** / **`target.xml`**
2. **归属**：每个 **`target.xml`** 归到路径上最近的 **`package.xml`**
3. **校验**：包依赖是否在扫描集内；目标依赖是否解析到库目标
4. **生成**：在 **`.intermediate/build/<叶子>/`** 写生成物与 **`gz_cache.txt`**（**`<叶子>`** 由 **`configure --build-dir-name`** 决定，常省略为 **`default`**）
5. **构建与安装**：**`build`** 编译并把产物装进 **`.intermediate/install/<名>/`**（**`<名>`** 多为缓存 **`arch=`**，与 **`<叶子>`** 不必相同）
6. **后续**：**`run` / `test` / `pack`** 基于安装树工作，CLI 用 **`--install-dir-name`** 指向 **`<名>`**

**目录名、合法字符、与 `print-build-dir-name` 的关系**：以 **[cli-reference.md](cli-reference.md)** 第 2 节为准（与 **`gz_cache.txt`** 字段对照见 **[internal-variables.md](internal-variables.md)** §3）。

**后端**：**CMake**（生成 `CMakeLists.txt` 再调 CMake）或 **Ninja**（生成 `build.ninja` 等）。

---

## 3. 概念索引（细节见专题文档）

### 3.1 package / target、类型、依赖、`<headers>`

- **package** / **target** 的角色，**`type`** 全集（`executable`、`library`、`static_library`、`shared_library`、`asset_bundle`、`prebuilt_*` 等）、**`<dependency>`**（含 **`visibility`**）、**`<headers>`** 的 **`from`/`to`**、**`<prebuilt …/>`**：一律以 **[package-target-xml-spec.md](package-target-xml-spec.md)**（§3.1、§3.3、§3.6 等）与 **`gz spec`** 为准。  
- **预置库示例**：[`test_projects/prebuilt_static_stub/README.md`](../../test_projects/prebuilt_static_stub/README.md)。  
- **原则**：`gz` / `gz-gui` 不对具体第三方库做内置特判；关系与路径均在 XML 中显式声明。

### 3.2 聚合 CMake 与 `CMAKE_PREFIX_PATH`

见 **[internal-variables.md](internal-variables.md)** §4.1（与 **`GZ_CMAKE_PREFIX_PATH`** 说明同文档）。

### 3.3 `arch` 与安装目录名

**`<arch>`**（缓存 **`arch=`**、**`gz print-build-dir-name`**）是组合标签，**不必**等于构建叶子名： **[internal-variables.md](internal-variables.md)** §3 表项 **`arch`**；命令行语义见 **cli-reference** 第 2 节。

---

## 4. 使用方式速览

### 4.1 准备环境与构建宿主工具

- CMake 3.20+、C++17；Windows 推荐 Visual Studio 2022 + MSVC。  
- **从源码构建 `gz` / `gz-gui`**：根目录 **`README.md`**（手工 CMake），或 **`python build.py`** / **`install.py`** / **`package.py`**。  
- 历史上曾使用子命令 **`project`** / 选项 **`--project-dir`**，当前版本已删除且无别名。

### 4.2 CLI（argv 与范例）

**子命令、每个开关、退出码、`list` 的 `--format`/`--quiet` 组合、典型 PowerShell 片段**：以 **[cli-reference.md](cli-reference.md)**（与 **`gz --help`**）为**唯一事实来源**；**入门教程**中的可复制命令见 **[getting-started.md](getting-started.md)**。

### 4.3 在单包目录工作

若 cwd 已在某包树内且 **`gz` 在 PATH**：依次 **`gz configure`** → **`gz print-build-dir-name`**（得到 **`<名>`**）→ **`gz build --build-dir-name default`**（或与 configure 一致的叶子名）→ **`gz test` / `run --install-dir-name <名> …`**。完整 XML 与目录约定仍以 **getting-started** 为准。

### 4.4 gz-gui 快速上手

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

说明：GUI 会把设置转成 **`--opt`** 传给 **`gz.exe`**（与 CLI 一致）。

### 4.5 常见工作流模板（仅列步骤，字段见 spec / getting-started）

- **模板 A：新增本包库目标** — 新建目标子目录 + **`target.xml`** + 源码 → 在 exe 的 **`target.xml`** 写 **`<dependency name="库名"/>`** → **`configure` → `print-build-dir-name` → `build` → `test`/`run`**（参数规则见 **cli-reference**）。  
- **模板 B：跨包依赖** — **`package.xml`** 声明包级 **`<dependency/>`** → 目标里 **`otherPkg:otherTarget`** → **`configure --scan`** 覆盖各包根。  
- **模板 C：头文件安装布局** — **`<headers>`** 的 **`from`/`to`** 见 **package-target-xml-spec** §3.3；装完看 **`.intermediate/install/<名>/include/`**。  
- **模板 D：第三方 CMake SDK** — 包外安装或官方 SDK → **vendor** 进本包或设 **`GZ_CMAKE_PREFIX_PATH`**，手写 **`prebuilt_*` + `<prebuilt>`** + **`<headers>`**（**getting-started** 第 7～8 步）→ **`configure` / `build`** → 消费方 **`<dependency name="pkg:target"/>`**。

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

旧写法 **`<dir>路径</dir>`** 已不支持；须改为 **`from`/`to` 自闭合**（**`<dir from="..." to="..."/>`** 等）。示例与语义见 **[package-target-xml-spec.md](package-target-xml-spec.md)** §3.3。

### Q6：为什么单包 configure 过不了，整仓扫描却可以

因为单包目录通常看不到跨包依赖目标。对于跨包依赖场景，请在更高层目录执行并用 `--scan` 覆盖所有相关包。

### Q7：纯库包（没有 executable）能不能 configure/build？

可以。当前实现已支持“仅库目标”的包（例如仅 **`prebuilt_*`** 或仅编译型库目标）。  
`configure` 只要求主包至少有一个 `target.xml`，不再强制必须有可执行目标。

---

## 附：建议阅读顺序

1. **本文**（分工表 + 鸟瞰 + FAQ + gz-gui）  
2. **[getting-started.md](getting-started.md)**（动手跑通）  
3. **[cli-reference.md](cli-reference.md)**（参数与退出码不再猜）  
4. **[package-target-xml-spec.md](package-target-xml-spec.md)** / **`gz spec`**（写 XML）  
5. 需要变量与缓存 → **[internal-variables.md](internal-variables.md)**；脚本 → **script-messages** / **script-tutorial**  
6. **[README.md](../../README.md)**、**[DESIGN.md](../../DESIGN.md)**、**[test_projects/README.md](../../test_projects/README.md)**

