# gz_reverse_cmake

在**不运行** `cmake`、**不经过** configure / File API 的前提下，对 `--source` 下顶层及 `add_subdirectory` 可达的 **`CMakeLists.txt`** 做**语句级浅层解析**（`name(…)` 命令流），再按**固定子集**重解释为 GroundZero 风格的 **`package.xml`** 与各子目录中的 **`target.xml`** 初稿。

- **工程文档**（规格、UML、映射）在仓库 **`ai-software-engineering/02-physical/gz-reverse-cmake/`**（AI 文档目录）。
- **本目录**为可执行程序源码与 `CMakeLists.txt`（实现产物，与 `ai-software-engineering/` 分离）。实现为 **C++17 标准库 + 自研解析**，**不**使用 `nlohmann/json`、**不**解析 `codemodel` JSON。

## 构建

**推荐**与仓库根工程一体构建（`gz_reverse_cmake` 已加入根 `add_subdirectory`）：

```powershell
cd <仓库根>
python build.py
# 或：cmake -S . -B _build && cmake --build _build --config Release --target gz_reverse_cmake
```

仅需 C++17；**无** `FetchContent` 外网依赖。仅开发本工具时，也可在 **`gz_reverse_cmake/`** 下单独 `cmake -S . -B build`（子目录内 `CMakeLists.txt` 不拉第三方库）。

**安装（与 `gz` 同发）**：根目录执行 **`python install.py --prefix <前缀>`** 时，**`gz_runtime`** 分量会在 **`bin/`** 下同时安装 **`gz_reverse_cmake`**（与 **`gz`**、**`gz-gui`** 一致；**不**需要 `--with-dev`）。

## 用法

```text
path\to\gz_reverse_cmake --source E:\path\to\cmake\project --out E:\out\reversed
```

可选：`--package-name`、`--package-version`；`--help` 查看全部参数。

## 能力边界（与真实 CMake 的差异）

- **会**处理：`project`、`set`（部分 `${VAR}` 展开）、`include_directories`、`add_subdirectory`、`add_executable` / `add_library(STATIC|SHARED|MODULE|…)`、`target_sources`、`target_link_libraries`、`target_include_directories`；`function`/`macro`/`foreach`/`while` 块内对 `add_*` 等**不做**宏展开/循环展开（块深度屏蔽）。
- **不**求值 `if/else/endif`：两分支里若都有 `add_*`，可能**重复/后写覆盖**；**不**执行 `include()` 拉入的外部模块、**不**展开 `file(GLOB)` 等需运行期的逻辑；含 `$<…>` 的实参在相关位置**跳过**。

生成结果需按业务核对；复杂场景请对照 **`doc/zh/package-target-xml-spec.md`** 手改或补全。
