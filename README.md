# uni-package (up)

**uni-package**（缩写 **`up`**）是一个面向「用数据结构驱动构建与包关系」的原型命令行工具：用 **`package.xml`** 与各目标子目录中的 **`target.xml`**（每目录至多一个）描述包与目标，`up` 负责扫描、生成 CMake 工程、构建、测试与运行。新手从零上手建议先读 **[doc/user-manual.md](doc/user-manual.md)**（中文）或 **[doc/user-manual.en.md](doc/user-manual.en.md)**（English）；`package.xml` / `target.xml` 的字段与解析约定见 **[doc/package-target-xml-spec.md](doc/package-target-xml-spec.md)**；设计背景与完整约定见 **[DESIGN.md](DESIGN.md)**，思维导图见 **[mindmap.mmd](mindmap.mmd)**。

## 依赖

- **CMake** 3.20+
- **C++17** 编译器（当前主要在 **Windows + MSVC** 上验证；生成出的子工程同样走 CMake/MSVC 或本机默认工具链）

## 构建 up

在仓库根目录执行（Visual Studio 2022 x64 示例）：

```powershell
cmake -S . -B _build -G "Visual Studio 17 2022" -A x64
cmake --build _build --config Release
```

生成可执行文件（默认）：`_build\Release\up.exe`。MSVC 下工程启用 **静态 CRT**（`/MT`）与 **`/utf-8`**，与 [DESIGN.md](DESIGN.md) 中对 `up.exe` 的取向一致。

可选：首次 `cmake` 时加上 **`-DUP_ENABLE_PROJECT=ON`** 以编译并启用 **`up project`** 子命令（以及 GUI 中对应能力）。默认 **OFF** 时执行 `up project` 会提示需重编译。

也可用 Python 脚本（在仓库根目录）：

```powershell
python build.py
python install.py --prefix dist
python package.py
```

**`package.py`**：将 **`up`** 与 **`up-gui`** 打进 **`dist/`** 下的归档（Windows 默认 **`.zip`**，其它系统默认 **`.tar.gz`**）。脚本会先执行 `install.py`（其内部会先执行 `build.py`）；`-o` 指定输出文件；`--format zip|tgz` 强制格式。归档内含 **`bin/`** 与简短 **`README_PACKAGE.txt`**。

`build.py` 在 Windows 上默认使用 **Visual Studio 17 2022**（x64）；可通过环境变量 **`UP_CMAKE_GENERATOR`** 或参数 **`--generator`** 覆盖。脚本**只编译** CMake 目标 **`up`** 与 **`up-gui`**（不构建工程中其它可能新增的目标）。

`install.py` 通过 **`cmake --install --component up_runtime`** 仅安装 **`up`** 与 **`up-gui`** 到 **`dist/`**（例如 `dist/bin/up.exe`、`dist/bin/up-gui.exe`；与 CMake 的 `COMPONENT` 名一致，见 `CMakeLists.txt`）。

上述两脚本**只作用于仓库根 CMake 工程**（默认 `_build` / `dist`），**不会编译或安装 `test_projects/` 里的内容**；那些示例包仍须用已安装的 **`up.exe`** 自行执行 `configure` / `build` / `test` 等（见 [test_projects/README.md](test_projects/README.md)）。

## 命令一览

| 命令 | 说明 |
|------|------|
| `up configure [--build-dir-name <叶子>] [--scan <目录>]... [--opt KEY=VALUE]...` | 扫描 `package.xml` 及包树内各 **`target.xml`**；在 **`.intermediate/build/<叶子>/`**（省略时为 **`default`**）下：`cmake` 模式生成 `CMakeLists.txt`，`ninja` 模式直接生成 `out/build.ninja`（不生成 CMakeLists），并写入 **`up_cache.txt`**（含 **`arch=`** 供安装目录命名） |
| `up build --build-dir-name <叶子>` | **必填** `--build-dir-name`；读取对应目录下 `up_cache.txt`，对已生成工程编译并 **install** 到 **`.intermediate/install/<arch>/`** |
| `up run --install-dir-name <名> <目标名>` | **必填** `--install-dir-name`：`<名>` 为 **`.intermediate/install/` 下的直接子目录名**（通常等于 `up_cache.txt` 的 **`arch`**，与构建叶子名不必相同）；运行其下 `bin/` 中可执行文件（Windows 下目标名可省略 `.exe`） |
| `up test --install-dir-name <名> [测试目标名]` | **必填** `--install-dir-name`；在对应安装树关联的构建元数据上执行 **CTest**（可选单个测试名） |
| `up pack --install-dir-name <名>...` | **至少一次** `--install-dir-name`（可重复，多架构）；打包到 **`.intermediate/pack/<arch>/`**（Windows: zip；其他平台: tar.gz） |
| `up spec` | 向 stdout 输出内嵌的英文 `package.xml` / `target.xml` 规则说明（供工具/AI） |
| `up print-build-dir-name [--build-dir-name <叶子>] [--opt ...]` | 打印当前配置对应的 **`<arch>`** 字符串（便于脚本传给 `run`/`test`/`pack` 的 `--install-dir-name`） |
| `up project ...` | **仅当**构建时启用 **`-DUP_ENABLE_PROJECT=ON`**。探测现有工程并生成 `package.xml` / `target.xml`。CMake 默认写 `<cmake source_dir=\"...\"/>`，并尽量从 `install(TARGETS ...)` 自动生成 `.targets/` 下的 `imported_installed_*` 包装目标；在需要时还会尝试 **CMake File API**（需本机 `cmake` 能成功配置该工程）。若依赖未齐或不想跑配置，可加 **`--cmake-no-file-api`**，仅做安装规则扫描 + **CMakeLists 启发式扫描**（含对 `target_link_libraries` / `target_include_directories` 的尽力解析），精度低于 codemodel |

无子命令或未知子命令时会打印简短用法。

`up-gui` 已提供“编译环境设置”窗口（菜单与工具栏入口），包含本地环境 / Android 环境 / emsdk 环境三个 Tab。设置保存到 `up_gui_settings.txt`，并在执行 `configure` 时追加为 `--opt` 参数传给 `up.exe`。

## 快速试用（示例工程）

示例目录：**[test_projects/](test_projects/)** — 其下**每个子目录**为一个独立测试包（各自 **`package.xml`**，目标拆在子目录、**每目录一个 `target.xml`**），说明见该目录内 [README.md](test_projects/README.md)。

在**仓库根目录**、已构建好 `up.exe` 时：

```powershell
.\_build\Release\up.exe configure --scan test_projects
$ARCH = .\_build\Release\up.exe print-build-dir-name
.\_build\Release\up.exe build --build-dir-name default
.\_build\Release\up.exe test --install-dir-name $ARCH
.\_build\Release\up.exe run --install-dir-name $ARCH hello_demo
```

若将 `up.exe` 加入 `PATH`，也可在 **`test_projects` 下某一子包目录**（例如 `test_projects\hello_demo`）内直接执行 `up configure`、`up build --build-dir-name default` 等（此时默认只扫描当前目录对应的那一个包）；`run`/`test`/`pack` 仍需带 **`--install-dir-name`**（一般为 **`up print-build-dir-name`** 的输出）。

## 第三方 SDK 导入（zlib / FBX 风格最小流程）

适用于“上游是 CMake 工程、你希望在 `up` 里把它当预编译库目标消费”的场景。

在 SDK 根目录执行：

```powershell
# 需要宿主 up 以 -DUP_ENABLE_PROJECT=ON 构建
up project
up configure
$ARCH = up print-build-dir-name
up build --build-dir-name default
```

安装与运行仍使用 **`--install-dir-name $ARCH`**（或从 `.intermediate/build/default/up_cache.txt` 读取 `arch=`）。

默认行为（CMake 探测）：

- `up project` 会生成 `package.xml`（含 `<cmake source_dir="..."/>`）
- 并尽量按 `install(TARGETS ...)` 自动生成 `.targets/<name>/target.xml`
- 自动目标类型为 `imported_installed_static_library` / `imported_installed_shared_library`
- 包名默认优先取 `CMakeLists.txt` 里的 `project(...)`（可被 `--package-name` 覆盖）
- `configure` 会把依赖包安装前缀并入 `CMAKE_PREFIX_PATH`，并在可推导时补充常见 `find_package` 缓存变量（如 `<PKG>_LIBRARY` / `<PKG>_LIBRARY_DEBUG` / `<PKG>_INCLUDE_DIR`）

规则约束（重要）：

- `up` / `up-gui` 是泛化规则引擎，不内置针对具体第三方代码库（库名、仓库名、目录布局）的特判逻辑。
- 依赖关系、安装产物、头文件目录等差异化行为，必须通过 `package.xml` / `target.xml` 显式声明。
- 当自动探测信息不足时，请手工补齐 `.targets/*/target.xml`、`<dependency .../>`、`<install artifact=\"...\" implib=\"...\"/>`、`<interface_include .../>` 等规则字段。

最小示例（可手工补齐）：

```xml
<!-- package.xml -->
<package name="zlib" version="0.1.0">
  <cmake source_dir="."/>
  <dependency name="openssl" optional="true"/>
</package>
```

```xml
<!-- .targets/zlibstatic/target.xml -->
<target name="zlibstatic" type="imported_installed_static_library">
  <install artifact="lib/zlibstatic.lib"/>
  <interface_include dir="include"/>
</target>
```

```xml
<!-- .targets/zlib_shared/target.xml (Windows) -->
<target name="zlib_shared" type="imported_installed_shared_library">
  <install artifact="bin/zlib1.dll" implib="lib/zlib.lib"/>
  <interface_include dir="include"/>
</target>
```

生成后可在其它包里通过目标依赖引用（示例）：

```xml
<dependency name="zlib:zlibstatic"/>
```

说明：

- 纯库包（没有 executable）也支持 `configure/build`
- Windows 下依赖库变量优先使用 `implib` / `.lib`，避免将 `.dll` 误传给链接器
- 若 `install(TARGETS ...)` 解析不到可包装库，只会生成 `package.xml`，此时可手工补 `.targets/*/target.xml`，或改用 `--legacy-cmake-parse`

## 工作目录约定（相对于执行 `up` 时的 cwd）

| 路径 | 含义 |
|------|------|
| `.intermediate/build/<叶子>/` | **configure** 生成根（如 **`default`**）：`cmake` 模式写 `CMakeLists.txt`；`ninja` 模式写 `out/build.ninja`；同目录含 **`up_cache.txt`** |
| `.intermediate/install/<arch>/` | **install** 前缀（如 `bin/`、`include/`）；`<arch>` 来自 `up_cache.txt` 的 **`arch=`**，与 **`<叶子>`** 通常不同 |
| `.intermediate/pack/<arch>/` | 打包输出（例如 `up-<arch>.zip` 或 `up-<arch>.tar.gz`） |

`<arch>` 当前按组合信息生成（如 `windows_x86_64_cmake_msvc_dynamic_release` 或 `windows_x86_64_ninja_msvc_dynamic_release`），来源于 `UP_TARGET_*` 配置与主机工具链探测。中间目录名 **`.intermediate`** 与 [mindmap.mmd](mindmap.mmd) 一致，建议加入 `.gitignore`。

**并行编译（加速 `up build`）**：`cmake --build` 与 `ninja install` 会带上并行度；**默认并行度**按本机**当前可见的逻辑处理器数**（Windows：`GetActiveProcessorCount` / `GetNativeSystemInfo`；Linux：`sysconf(_SC_NPROCESSORS_ONLN)`；macOS：`sysctl`；再回退 `std::thread::hardware_concurrency()`，仍未知则用 4），并写入 **`up_cache.txt`** 的 **`UP_BUILD_PARALLEL`** 与 **`UP_BUILD_JOBS`**（二者同值，与 ninja `-j` / cmake `--parallel` 一致；仍可在命令行只改其一）。可用 `up configure --opt UP_BUILD_PARALLEL=16` 覆盖；**不再读取** `CMAKE_BUILD_PARALLEL_LEVEL` 等环境变量。含 `<cmake/>` 子工程时，生成根 `CMakeLists.txt` 里 **ExternalProject** 的 `BUILD_COMMAND` 会在 **configure 当刻** 按当前选项把并行数写进脚本；若要改子工程并行度需重新 `configure`。

## 仓库结构（摘要）

```
├── CMakeLists.txt      # 构建 up
├── build.py            # CMake 配置 + 编译（可选替代手工 cmake）
├── install.py          # cmake --install（默认前缀 dist/）
├── package.py          # 将 up / up-gui 打成 zip 或 tar.gz
├── DESIGN.md           # 设计文档
├── ai-software-engineering/  # 四阶段工程文档（概念/逻辑/物理/运维），与实现同步维护
├── doc/                # 补充规范（如 XML 描述文件）
├── README.md           # 本文件
├── mindmap.mmd         # 设计思维导图
├── src/                # up CLI 源码（*.hpp 与 *.cpp）
├── src_gui/            # up-gui 外壳（Windows: up_gui_win32.cpp；Linux: GTK3；macOS: Cocoa；共享 gui_unix_shared）
└── test_projects/      # 测试包集合（每子目录一包）
```

## 许可

未在仓库中统一声明许可证前，默认保留所有权利；如需开源许可可自行补充 `LICENSE` 并更新本段。
