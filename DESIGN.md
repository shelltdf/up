# 通用包管理机制（up）设计文档

本文档由仓库根目录 **[mindmap.mmd](mindmap.mmd)** 思维导图整理并随其迭代，描述 **up** 的目标、国际化与路径策略、命令、包描述格式、工作流程与目录约定。

## 1. 背景与目标

### 1.1 为什么需要

- **语言与生态**：C++ 缺乏统一、顺手的包管理模式；同时 AI 辅助下，同一套设计可落地为 JS/TS/Java/Python/C# 等多种实现，需要一种与具体语言弱耦合、以**数据结构驱动行为**的编排方式。
- **建造系统多样性**：同一套包描述应能生成 **CMake、Ninja、Visual Studio 解决方案（sln）** 等不同后端（首期实现以 CMake 为主，架构预留扩展点）。
- **元构建与资源**：类似 Qt 的 **moc、uic**，以及 **qrc、多语言翻译** 等，要求包管理流程支持 **meta 元编程 / 代码生成** 的可扩展管线。
- **项目模板**：内置模板（如 C++ 带 `main` 的 HelloWorld），并允许用户添加**自定义模板**。

### 1.2 设计原则

- **数据即行为**：用 `package.xml` 与各子目录中的 **`target.xml`** 等声明式结构表达依赖、目标与生成规则，行为由解释与生成器完成（类比**行为树**：节点是数据，执行路径由图/树决定）。
- **可扩展**：依赖图扫描、生成器（CMake 等）、元工具链均以可插拔方式演进，避免硬编码单一工具链。

### 1.3 国际化（i18n）与文本编码

与思维导图「多语言支持」一致：

| 范围 | 约定 |
|------|------|
| **CLI（`up.exe`）与 GUI（`up-gui.exe`）界面语言** | 根据**操作系统默认语言**选择文案：默认语言为中文时显示中文，否则显示英文。 |
| **包描述与源文件内容** | 支持 **Unicode**，允许 **UTF-8、UTF-16** 等编码的文本内容（含中文注释、字符串、XML 内文本）。实现上应能正确读写常见编码。 |
| **文件系统路径** | **路径本身不支持「多语言路径」作为常规能力**：工具链与编译器对非 ASCII 路径的支持不一致（例如即便 `up` 接受中文路径，GCC/其他后端仍可能失败）。 |

**路径策略（configure 阶段）**：

- 在 **configure**（及任何依赖路径解析的步骤）中，对**不符合约定或可能导致后端失败的路径**（例如包含非 ASCII 字符的路径，若当前策略禁止）应**明确报错**，并给出可读错误信息，而不是静默生成不可用的工程。

> 设计意图：把「路径能否用于构建」在配置期查清，避免用户在链接/编译阶段才遇到晦涩失败。

## 2. up 命令设计

命令行入口为 **`up`**（实现形态要求：**C++ 的 `up.exe`，静态链接 CRT**）。可选配套 **`up-gui.exe`**：仅使用**本地窗口系统**，交互类似 **cmake-gui**（后续阶段）。当前 Win32 参考实现已提供“编译环境设置”窗口（菜单与工具栏入口），含本地环境 / Android / emsdk 三个 Tab，可自动探测并在 configure 时通过 `--opt` 传入 `up.exe`。

编译环境自动探测策略（Win32 参考实现）：

- 本地环境（建造系统 / vcvars / 编译器）：按 **PATH 优先** 搜索真实可执行文件路径，并补充常见安装目录扫描；UI 以列表表格展示 `工具/路径/来源(PATH|扫描)`，支持多命中并可点选当前项。
- VS vcvars：搜索 `vcvars64.bat` / `vcvars32.bat` / `VsDevCmd.bat`（环境变量推导 + 常见 Visual Studio 目录）；不作为 `--opt` 注入，而是在 GUI 启动 `up.exe` 时先 `call vcvars` 再执行命令。
- Android NDK：按 `ANDROID_NDK_ROOT` / `ANDROID_NDK_HOME`、`ANDROID_SDK_ROOT` / `ANDROID_HOME` 下 `ndk` 与 `ndk-bundle`、常见 SDK 路径逐级探测；多版本命中时按版本号与更新时间选推荐值。
- emsdk（Windows）：按 `EMSDK`、常见路径（用户目录/Program Files）探测；若未命中，再执行各盘根一级兜底（如 `C:\emsdk`、`D:\emsdk`），不做深度递归以控制耗时。
- 路径回填：自动搜索后，`Android SDK/NDK/emsdk` 输入框会回填当前推荐命中路径（若已有值且仍在命中列表中则保留）。
- 状态提示：自动搜索后仅显示“命中数量 + 关键未命中项”，不展示冗长检测说明文本。

| 子命令 | 职责 |
|--------|------|
| `configure` | 解析包依赖关系，生成具体建造系统文件（如 CMake）；可写**设置结果/缓存**（类比 CMake cache，保存用户开关与变量）。 |
| `build` | 调用建造系统（由 CMake 产生的 Makefile、Ninja、sln 等），**内建（in-tree）构建**并安装到约定 `install` 目录。 |
| `run` | 运行已编译完成的目标（如 `exe`）。 |
| `test` | 运行单元测试；**无参数时运行全部**测试。 |
| `pack` | 将编译结果打包为目标形态（安装程序、apk 等，依后端而定）。 |

### 2.1 configure 要点

- 可配置**扫描路径**；**未配置时默认递归搜索当前 cwd**。
- 扫描到 `package.xml` 及包树内各目录下的 **`target.xml`**（**每个目录至多一个**），建立 **package 关系图**，并在控制台**打印树状图**便于核对。
- **归属规则**：扫描到的 **package** 作为当前包节点，其下的 **target** 作为子节点；**每个 target 必须能归属到某个 package 父节点**。
- 生成结果落在 **`.intermediate/build/<叶子>/`**（由 **`configure --build-dir-name`** 指定，省略时为 **`default`**）：`cmake` 模式产出 `CMakeLists.txt`，`ninja` 模式产出 `out/build.ninja`；同目录写入 **`up_cache.txt`**，其中 **`arch=`** 字段用于命名 **`.intermediate/install/<arch>/`**（构建目录名与 `arch` 目录名不必相同）。

### 2.2 build / run / test / pack

- **build**：CLI 需指定 **`--build-dir-name`** 以定位 **`.intermediate/build/<叶子>/`**；在该目录按所选后端执行构建（`cmake` 或 `ninja`），产物安装到 **`install/<arch>/`**（含 `bin`、`include` 等）。实现上对 **`cmake --build`** 传入 **`--parallel N`**、对 **ninja** 传入 **`-j N`**（`N` 来自 **`up_cache.txt` / `--opt`** 中的 **`UP_BUILD_PARALLEL` 或 `UP_BUILD_JOBS`**（别名）；缺省由 **`configure`** 按 OS 报告的可用逻辑 CPU 数写入（Windows/Linux/macOS 各有首选 API，再回退 STL）；不读并行相关环境变量），以缩短编译阶段耗时。
- **run**：CLI 需 **`--install-dir-name`**（安装前缀在 **`.intermediate/install/`** 下的子目录名，通常等于 **`up_cache.txt` 的 `arch=`**）；再指定可执行目标名。
- **test**：同上，需 **`--install-dir-name`** 后可选测试名；底层如 CTest。
- **pack**：可重复 **`--install-dir-name`**；输出到 **`pack/<arch>/`**。

## 3. 包文件设计

细则见 **[doc/zh/package-target-xml-spec.md](doc/zh/package-target-xml-spec.md)**。

### 3.1 package.xml

- 文件内**只包含一个根 `<package>` 元素**（一个 package 对象）。
- 承载：包名、版本、依赖列表、以及后续为 moc/uic/翻译等预留的子节点。

### 3.2 `target.xml`（每目录一个）

- 文件名固定为 **`target.xml`**；**同一目录下不得出现多个** target 描述文件，多目标通过**不同子目录**各放一份 `target.xml` 解决。
- 每个文件内**只包含一个根 `<target>` 元素**。`<sources>` 中的路径相对于**该 `target.xml` 所在目录**。
- 承载：目标名、类型（`executable` / `static_library` / `shared_library`）、源文件列表、可选 **`<headers>`**（头文件来源与安装到 `include/` 的布局）、可选 **`<dependency name="..." visibility="..."/>`**（`visibility` 可选，默认 **`private`**，取值 **`private` / `public` / `interface`**，对应 CMake `target_link_libraries` 的可见性；**可执行目标**不得对依赖使用 **`interface`**，否则 configure 失败）。库与库之间**当前不**生成链式 `target_link_libraries`（由最终可执行文件等消费方链接）。**`<sources>` / `<headers>`** 等条目上的 **`when`** 与变量合并细则见 **[doc/zh/package-target-xml-spec.md](doc/zh/package-target-xml-spec.md)**（与 **`up spec`** 输出一致者优先）。
- 与父 `package.xml` 的归属关系由目录树表达（`target.xml` 须位于对应 `package.xml` 之下的子树中）。

### 3.3 最小示例与仓库布局

见 **`test_projects/<包名>/`**：**一级子目录**为独立测试包（根下放 `package.xml`）；包内**每个目标**再占**子目录**，该目录内**仅一个** `target.xml`（示例见 `hello_demo/hello_foo/`、`hello_demo/hello_demo/`）。正式 schema 可在后续迭代为 XSD 或 JSON Schema。

## 4. 工作原理（概要）

1. 用户在项目 **cwd** 下使用 **`up.exe`**（及未来的 **`up-gui.exe`**）。
2. **configure**：扫描 → 建图 → 打印树 → 在 **`.intermediate/build`** 下生成后端文件（cmake 或 ninja）；可选写入缓存文件。
3. **build**：内建构建并 **install** 到 **`.intermediate/install/<arch>/`**。
4. **run / test / pack**：基于 install 与构建元数据执行；**pack** 面向 **`.intermediate/pack/<arch>/`**。

## 5. 目录关系（cwd 下）

执行 `up` 时，在**当前工作目录（cwd）**下使用一个**真实存在的中间目录**，固定名为 **`.intermediate`**（名称带点，表示工具管理的中间产物，便于 `.gitignore`）。

其下包含：

| 路径（相对 cwd） | 说明 |
|------------------|------|
| `.intermediate/build/` | 构建根目录；其下按 **`<叶子>`**（如 **`default`**）分子目录，内部根据 `UP_TARGET_BUILD_SYSTEM` 生成 cmake 或 ninja 文件，并含 **`up_cache.txt`**。 |
| `.intermediate/install/` | 按 **`arch`** 分子目录；其下为 `bin`、`include` 等安装布局（**`arch`** 来自对应构建目录的缓存，不是 `<叶子>` 名）。 |
| `.intermediate/pack/` | 按 **`arch`** 分目录，存放打包产物。 |

另可生成**配置结果文件**（名称与格式由实现定义），保存在 `.intermediate` 内或与其并列策略由实现选定，语义上类比 **CMake cache**（保存用户输入的开关与变量）。

### 5.1 `arch` 的语义（设计目标）

**`arch` 不是仅 CPU 架构字符串**，而是下列维度的**组合标识**（具体编码格式由实现定义，可为目录名安全的一串 token）：

- 操作系统  
- CPU 架构  
- 动态 / 静态链接（若与后端相关）  
- 是否静态 CRT（Windows 等场景）  
- **Debug / Release**（或与 CMake 配置矩阵对应）

当前实现已使用组合标签（例如 `windows_x86_64_cmake_msvc_dynamic_release` 或 `windows_x86_64_ninja_msvc_dynamic_release`），并允许按 `UP_TARGET_*` 选项切换 CPU/构建系统/链接形态/Debug。

## 6. 实现路线图（建议）

| 阶段 | 内容 |
|------|------|
| P0 | CLI 子命令骨架、`.intermediate` 目录约定、`package.xml` / 子目录 `target.xml` 最小解析、configure 生成 CMake、build 调用 cmake、test 调用 ctest。 |
| P1 | 多包依赖图、完整 **arch** 元组、缓存文件、路径校验与 i18n 文案；工程迁移以**手写** `package.xml` / `target.xml` 为主（见 `doc/zh/getting-started.md`）。 |
| P2 | Ninja/sln 生成器、元工具链插件、moc/uic 类管线。 |
| P3 | `up-gui.exe`、pack 多形态、模板市场/用户模板。 |

## 7. 与思维导图章节的对应关系

| 思维导图 | 本文档 |
|----------|--------|
| 我们需要什么（任意项目、多后端、行为树式数据、meta、模板） | §1.1、§1.2、§3 |
| 多语言支持（界面语言、文件编码、路径限制与 configure 报错） | §1.3 |
| up 命令设计 | §2 |
| 包文件设计（含每目录单 `target.xml`、`test_projects`） | §3 |
| 测试与示例仓库、`hello_demo` 布局 | §3.3、导图「测试与示例仓库」 |
| 宿主工具 Python（`build.py` / `install.py` / `package.py`） | 导图「宿主工具开发与分发 Python」 |
| 工作原理（up.exe / up-gui、configure/build/run/test/pack） | §4 |
| up-gui 目标界面 vs Win32 参考实现 | 导图「工作原理」下 up-gui 分支 |
| `up_cache.txt` 与缓存语义 | §2.1、§4、§5、导图「up_cache 规划」 |
| 当前实现要点（单包 CMake、CTest、install 首个 exe、`arch` 组合标签） | §5.1、导图「当前实现要点」 |
| 目录关系（`.intermediate`、`build`/`install`/`pack`、`arch` 组合） | §5、§5.1 |
| 文档（DESIGN / README） | 导图「文档」 |

---

*文档版本：与 `mindmap.mmd` 同步；若导图更新，请同步修订本节与上文路径/行为描述。*
