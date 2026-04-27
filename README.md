# GroundZero（gz）

**GroundZero**（命令行缩写 **`gz`**）是一个面向「用数据结构驱动构建与包关系」的原型命令行工具：用 **`package.xml`** 与各目标子目录中的 **`target.xml`**（每目录至多一个）描述包与目标，`gz` 负责扫描、生成 CMake 工程、构建、测试与运行。**推荐**用户**纯手写**上述 XML（见 **`doc/zh/getting-started.md`**、**`doc/zh/package-target-xml-spec.md`** 文首）。**`gz reverse` 子命令已移除**；迁移请手写 **`package.xml` / `target.xml`**。新手从零上手建议先读 **[doc/zh/getting-started.md](doc/zh/getting-started.md)**（分步教程）与 **[doc/zh/user-manual.md](doc/zh/user-manual.md)**（中文）、**[doc/en/user-manual.md](doc/en/user-manual.md)**（English）；**`gz` 命令行全部参数与工作模式**见 **[doc/zh/cli-reference.md](doc/zh/cli-reference.md)**；`package.xml` / `target.xml` 的字段与解析约定见 **[doc/zh/package-target-xml-spec.md](doc/zh/package-target-xml-spec.md)**；**内置变量、`gz_cache.txt` 与 `GZ_*` 键**见 **[doc/zh/internal-variables.md](doc/zh/internal-variables.md)**；**从最小包到 `when`/预处理/子工程**见 **[doc/zh/script-tutorial.md](doc/zh/script-tutorial.md)**；**脚本消息 `trigger` 总表与 Lua 绑定语义**见 **[doc/zh/script-messages.md](doc/zh/script-messages.md)**；`doc/` 双语索引见 **[doc/README.md](doc/README.md)**。设计背景与完整约定见 **[DESIGN.md](DESIGN.md)**，思维导图见 **[mindmap.mmd](mindmap.mmd)**。近期变更记录见 **[CHANGELOG.md](CHANGELOG.md)**。

## 依赖

- **CMake** 3.20+
- **C++17** 编译器（当前主要在 **Windows + MSVC** 上验证；生成出的子工程同样走 CMake/MSVC 或本机默认工具链）

## 构建 gz

在仓库根目录执行（Visual Studio 2022 x64 示例）：

```powershell
cmake -S . -B _build -G "Visual Studio 17 2022" -A x64
cmake --build _build --config Release
```

> **构建目录**：上文 **`_build`** 仅为示例名，请与实际 **`cmake -B <目录>`** 一致（例如 **`_build_gz`**）。后文 **`.\_build\Release\`** 中的 **`_build`** 亦需按你的目录名替换。仓库 **`.gitignore`** 已忽略根目录 **`/_build*/`** 与 **`_build/`**。

默认构建产物：

- `gz.exe`（CLI 入口）
- `gz.lib`（静态库实现）

位于 `_build\Release\`（Windows + Release 示例）。MSVC 下工程启用 **静态 CRT**（`/MT`）与 **`/utf-8`**，与 [DESIGN.md](DESIGN.md) 中对 `gz.exe` 的取向一致。

也可用 Python 脚本（在仓库根目录）：

```powershell
python build.py
python install.py --prefix dist
# 如需同时安装开发静态库 gz.lib：
python install.py --prefix dist --with-dev
python package.py
# 如需把 gz.lib 一并打包：
python package.py --with-dev
```

**`package.py`**：将 **`gz`** 与 **`gz-gui`** 打进 **`dist/`** 下的归档（Windows 默认 **`.zip`**，其它系统默认 **`.tar.gz`**）。脚本会先执行 `install.py`（其内部会先执行 `build.py`）；`-o` 指定输出文件；`--format zip|tgz` 强制格式；`--with-dev` 可把 **`gz.lib`** 一并打包。归档内含 **`bin/`**（可选 `lib/`）与简短 **`README_PACKAGE.txt`**。

`build.py` 在 Windows 上默认使用 **Visual Studio 17 2022**（x64）；可通过环境变量 **`GZ_CMAKE_GENERATOR`** 或参数 **`--generator`** 覆盖。脚本**只编译** CMake 目标 **`gz`** 与 **`gz-gui`**（`gz` 由 `GroundZero/exe/` 入口 + `GroundZero/lib/` 静态库组成，不构建工程中其它可能新增的目标）。

`install.py` 默认通过 **`cmake --install --component gz_runtime`** 安装 **`gz`** 与 **`gz-gui`** 到 **`dist/`**（例如 `dist/bin/gz.exe`、`dist/bin/gz-gui.exe`；与 CMake 的 `COMPONENT` 名一致，见 `CMakeLists.txt`）。若加 **`--with-dev`**，会额外安装 **`gz_dev`** 组件（当前为 `dist/lib/gz.lib`）。

上述两脚本**只作用于仓库根 CMake 工程**（默认 `_build` / `dist`），**不会编译或安装 `test_projects/` 里的内容**；那些示例包仍须用已安装的 **`gz.exe`** 自行执行 `configure` / `build` / `test` 等（见 [test_projects/README.md](test_projects/README.md)）。

## 命令一览

| 命令 | 说明 |
|------|------|
| `gz configure [--build-dir-name <叶子>] [--scan <目录>]... [--opt KEY=VALUE]...` | 扫描 `package.xml` 及包树内各 **`target.xml`**；在 **`.intermediate/build/<叶子>/`**（省略时为 **`default`**）下：`cmake` 模式生成 `CMakeLists.txt`，`ninja` 模式直接生成 `out/build.ninja`（不生成 CMakeLists），并写入 **`gz_cache.txt`**（含 **`arch=`** 供安装目录命名） |
| `gz build --build-dir-name <叶子>` | **必填** `--build-dir-name`；读取对应目录下 `gz_cache.txt`，对已生成工程编译并 **install** 到 **`.intermediate/install/<arch>/`** |
| `gz run --install-dir-name <名> <目标名>` | **必填** `--install-dir-name`：`<名>` 为 **`.intermediate/install/` 下的直接子目录名**（通常等于 `gz_cache.txt` 的 **`arch`**，与构建叶子名不必相同）；运行其下 `bin/` 中可执行文件（Windows 下目标名可省略 `.exe`） |
| `gz test --install-dir-name <名> [测试目标名]` | **必填** `--install-dir-name`；在对应安装树关联的构建元数据上执行 **CTest**（可选单个测试名） |
| `gz pack --install-dir-name <名>...` | **至少一次** `--install-dir-name`（可重复，多架构）；打包到 **`.intermediate/pack/<arch>/`**（Windows: zip；其他平台: tar.gz） |
| `gz spec` | 向 stdout 输出内嵌的英文 `package.xml` / `target.xml` 规则说明（供工具/AI） |
| `gz list [--format tree\|json\|xml] [--xml <路径>] [--json <路径>] [--quiet]` | 输出当前 DOM 结构（默认 tree）；`--format` 控制 stdout 载荷；`--xml/--json` 导出文件；`--quiet` 抑制树形与导出提示。部分组合参数会给 warning 但不失败 |
| `gz print-build-dir-name [--build-dir-name <叶子>] [--opt ...]` | 打印当前配置对应的 **`<arch>`** 字符串（便于脚本传给 `run`/`test`/`pack` 的 `--install-dir-name`） |

无子命令或未知子命令时会打印简短用法。

`gz-gui` 已提供“编译环境设置”窗口（菜单与工具栏入口），包含本地环境 / Android 环境 / emsdk 环境三个 Tab。设置保存到 `gz_gui_settings.txt`，并在执行 `configure` 时追加为 `--opt` 参数传给 `gz.exe`。

### `gz list` 常用示例

```powershell
# 1) 默认树形输出（stdout）
gz list

# 2) JSON 输出到 stdout（推荐写法）
gz list --format json

# 3) 同时导出 XML + JSON 文件（保留默认 tree stdout）
gz list --xml .intermediate/dom.xml --json .intermediate/dom.json

# 4) 仅导出文件，不打印树形和导出提示
gz list --xml .intermediate/dom.xml --json .intermediate/dom.json --quiet
```

## 快速试用（示例工程）

示例目录：**[test_projects/](test_projects/)** — 其下**每个子目录**为一个独立测试包（各自 **`package.xml`**，目标拆在子目录、**每目录一个 `target.xml`**），说明见该目录内 [README.md](test_projects/README.md)。

在**仓库根目录**、已构建好 `gz.exe` 时：

```powershell
.\_build\Release\gz.exe configure --scan test_projects
$ARCH = .\_build\Release\gz.exe print-build-dir-name
.\_build\Release\gz.exe build --build-dir-name default
.\_build\Release\gz.exe test --install-dir-name $ARCH
.\_build\Release\gz.exe run --install-dir-name $ARCH hello_demo
```

若将 `gz.exe` 加入 `PATH`，也可在 **`test_projects` 下某一子包目录**（例如 `test_projects\hello_demo`）内直接执行 `gz configure`、`gz build --build-dir-name default` 等（此时默认只扫描当前目录对应的那一个包）；`run`/`test`/`pack` 仍需带 **`--install-dir-name`**（一般为 **`gz print-build-dir-name`** 的输出）。

## 第三方 SDK 导入（手写 `package.xml` / `target.xml`）

适用于：上游是 CMake 或其它构建，你已在**包外**完成 **`install`**（或拿到官方预编译布局），希望在 `gz` 里**显式**描述头文件与库路径。

**推荐流程**：

1. 在包外对上游执行 **`cmake --install`**（或解压 SDK），得到确定的 `lib/`、`include/` 等目录。
2. 在本仓库或你的工程里**手写** **`package.xml`**（包名、可选包级依赖）与 **`target.xml`**：用 **`imported_installed_*` + `<install artifact="..."/>`** 指向安装前缀下的 `.lib` / `.so`，或用 **`imported_*` + `<prebuilt>`** 指向 vendored 二进制；用 **`<headers>`** 暴露头文件。
3. **`gz configure --scan …`** → **`gz build --build-dir-name …`**；`run` / `test` / `pack` 仍使用 **`--install-dir-name $ARCH`**（`$ARCH` 来自 **`gz print-build-dir-name`** 或 **`gz_cache.txt` 的 `arch=`**）。

**`configure`** 仍会把本工作区 **`.intermediate/install/<arch>/`** 并入 **`CMAKE_PREFIX_PATH`**，并在多包场景下尽量为 **`find_package`** 推导常见缓存变量（若你仍在 CMake 聚合后端中消费上游 CMake 工程）。规则约束不变：**无**针对 zlib/FBX 等具体库名的内置特判，一切靠 XML 声明。

最小示例（`artifact` 相对安装前缀）：

```xml
<!-- package.xml -->
<package name="zlib" version="0.1.0">
  <dependency name="openssl" optional="true"/>
</package>
```

```xml
<!-- 例如 zlibstatic/target.xml -->
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
- 若安装布局与上游 CMake 不一致，请在 **`target.xml`** 中核对 **`artifact` / `implib` / `<headers>`** 路径
- 在 **`target.xml`** 里写目标级依赖时，CMake 后端可选 **`visibility="private|public|interface"`**（默认 `private`，对应 `target_link_libraries` 的可见性）；**可执行目标**不得对依赖使用 **`interface`**。完整语法与 `when` 适用范围以 **`gz spec`** 与 **`doc/zh/package-target-xml-spec.md`** 为准。

## 工作目录约定（相对于执行 `gz` 时的 cwd）

| 路径 | 含义 |
|------|------|
| `.intermediate/build/<叶子>/` | **configure** 生成根（如 **`default`**）：`cmake` 模式写 `CMakeLists.txt`；`ninja` 模式写 `out/build.ninja`；同目录含 **`gz_cache.txt`** |
| `.intermediate/install/<arch>/` | **install** 前缀（如 `bin/`、`include/`）；`<arch>` 来自 `gz_cache.txt` 的 **`arch=`**，与 **`<叶子>`** 通常不同 |
| `.intermediate/pack/<arch>/` | 打包输出（例如 `gz-<arch>.zip` 或 `gz-<arch>.tar.gz`） |

`<arch>` 当前按组合信息生成（如 `windows_x86_64_cmake_msvc_dynamic_release` 或 `windows_x86_64_ninja_msvc_dynamic_release`），来源于 `GZ_TARGET_*` 配置与主机工具链探测。中间目录名 **`.intermediate`** 与 [mindmap.mmd](mindmap.mmd) 一致，建议加入 `.gitignore`。

**并行编译（加速 `gz build`）**：`cmake --build` 与 `ninja install` 会带上并行度；**默认并行度**按本机**当前可见的逻辑处理器数**（Windows：`GetActiveProcessorCount` / `GetNativeSystemInfo`；Linux：`sysconf(_SC_NPROCESSORS_ONLN)`；macOS：`sysctl`；再回退 `std::thread::hardware_concurrency()`，仍未知则用 4），并写入 **`gz_cache.txt`** 的 **`GZ_BUILD_PARALLEL`** 与 **`GZ_BUILD_JOBS`**（二者同值，与 ninja `-j` / cmake `--parallel` 一致；仍可在命令行只改其一）。可用 `gz configure --opt GZ_BUILD_PARALLEL=16` 覆盖；**不再读取** `CMAKE_BUILD_PARALLEL_LEVEL` 等环境变量。

## 仓库结构（摘要）

```
├── CMakeLists.txt      # 构建 GroundZero（gz / gz-gui）
├── build.py            # CMake 配置 + 编译（可选替代手工 cmake）
├── install.py          # cmake --install（默认前缀 dist/）
├── package.py          # 将 gz / gz-gui 打成 zip 或 tar.gz
├── DESIGN.md           # 设计文档
├── ai-software-engineering/  # 四阶段工程文档（概念/逻辑/物理/运维），与实现同步维护
├── doc/                # 用户手册、XML 规范、变量总览、脚本教程等（见 doc/README.md）
├── README.md           # 本文件
├── mindmap.mmd         # 设计思维导图
├── GroundZero/
│   ├── exe/            # gz.exe 入口（main 与命令分发）
│   └── lib/            # gz.lib 实现（engine + infra）
├── GroundZeroGUI/      # gz-gui 外壳（platform/win32|gtk|cocoa/ 三子目录 + core/；各平台 main 见 main/main_*）
└── test_projects/      # 测试包集合（每子目录一包）
```

## 许可

未在仓库中统一声明许可证前，默认保留所有权利；如需开源许可可自行补充 `LICENSE` 并更新本段。
