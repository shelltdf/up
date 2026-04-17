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
| `up configure [--scan <目录>]... [--opt KEY=VALUE]...` | 扫描 `package.xml` 及包树内各 **`target.xml`**；在 `.intermediate/build/<arch>/` 下：`cmake` 模式生成 `CMakeLists.txt`，`ninja` 模式直接生成 `out/build.ninja`（不生成 CMakeLists），并更新 `up_cache.txt` |
| `up build` | 对已生成的 CMake 工程配置、编译并 **install** 到 `.intermediate/install/<arch>/` |
| `up run <目标名>` | 运行 `.intermediate/install/<arch>/bin/` 下的可执行文件（Windows 下可省略 `.exe`） |
| `up test [测试目标名]` | 在构建目录中执行 **CTest**（可选指定单个测试目标） |
| `up pack` | 将 `.intermediate/install/<arch>/` 打包为归档文件输出到 `.intermediate/pack/<arch>/`（Windows: zip；其他平台: tar.gz） |
| `up project` | 占位：将现有工程迁移为 `package.xml` 与各子目录 `target.xml` 的向导 |

无子命令或未知子命令时会打印简短用法。

`up-gui` 已提供“编译环境设置”窗口（菜单与工具栏入口），包含本地环境 / Android 环境 / emsdk 环境三个 Tab。设置保存到 `up_gui_settings.txt`，并在执行 `configure` 时追加为 `--opt` 参数传给 `up.exe`。

## 快速试用（示例工程）

示例目录：**[test_projects/](test_projects/)** — 其下**每个子目录**为一个独立测试包（各自 **`package.xml`**，目标拆在子目录、**每目录一个 `target.xml`**），说明见该目录内 [README.md](test_projects/README.md)。

在**仓库根目录**、已构建好 `up.exe` 时：

```powershell
.\_build\Release\up.exe configure --scan test_projects
.\_build\Release\up.exe build
.\_build\Release\up.exe test
.\_build\Release\up.exe run hello_demo
```

若将 `up.exe` 加入 `PATH`，也可在 **`test_projects` 下某一子包目录**（例如 `test_projects\hello_demo`）内直接执行 `up configure`、`up build` 等（此时默认只扫描当前目录对应的那一个包）。

## 工作目录约定（相对于执行 `up` 时的 cwd）

| 路径 | 含义 |
|------|------|
| `.intermediate/build/<arch>/` | 构建根目录：`cmake` 模式写 `CMakeLists.txt`；`ninja` 模式写 `out/build.ninja` |
| `.intermediate/install/<arch>/` | 安装前缀（如 `bin/`、`include/`） |
| `.intermediate/pack/<arch>/` | 打包输出（例如 `up-<arch>.zip` 或 `up-<arch>.tar.gz`） |

`<arch>` 当前按组合信息生成（如 `windows_x86_64_cmake_msvc_dynamic_release` 或 `windows_x86_64_ninja_msvc_dynamic_release`），来源于 `UP_TARGET_*` 配置与主机工具链探测。中间目录名 **`.intermediate`** 与 [mindmap.mmd](mindmap.mmd) 一致，建议加入 `.gitignore`。

## 仓库结构（摘要）

```
├── CMakeLists.txt      # 构建 up
├── build.py            # CMake 配置 + 编译（可选替代手工 cmake）
├── install.py          # cmake --install（默认前缀 dist/）
├── package.py          # 将 up / up-gui 打成 zip 或 tar.gz
├── DESIGN.md           # 设计文档
├── doc/                # 补充规范（如 XML 描述文件）
├── README.md           # 本文件
├── mindmap.mmd         # 设计思维导图
├── src/                # up CLI 源码（*.hpp 与 *.cpp）
├── src_gui/            # up-gui 外壳（Windows: up_gui_win32.cpp；Linux: GTK3；macOS: Cocoa；共享 gui_unix_shared）
└── test_projects/      # 测试包集合（每子目录一包）
```

## 许可

未在仓库中统一声明许可证前，默认保留所有权利；如需开源许可可自行补充 `LICENSE` 并更新本段。
