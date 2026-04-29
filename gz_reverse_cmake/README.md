# gz_reverse_cmake

在**不运行** `cmake`、**不经过**本工具内 configure 的前提下，对 `--source` 下顶层及 `add_subdirectory` 可达的 **`CMakeLists.txt`** 做**语句级浅层解析**（`name(…)` 命令流），再按**固定子集**重解释为 GroundZero 风格的 **`package.xml`** 与各子目录中的 **`target.xml`** 初稿。

- **工程文档**（规格、UML、映射）在仓库 **`ai-software-engineering/02-physical/gz-reverse-cmake/`**（AI 文档目录）。
- **本目录**为可执行程序源码与 `CMakeLists.txt`（实现产物，与 `ai-software-engineering/` 分离）。实现为 **C++17 标准库 + 自研解析**；**不**依赖 `nlohmann/json`。**L7** 下可选 **`--file-api`** 读入用户预置的 `codemodel-*.json` 等，采用**无第三方**的 JSON **子集**解析（与 `spec.md` 同进）。

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

- **`--source`**：含顶层 `CMakeLists.txt` 的目录（**只读**扫描）。**省略 = 当前工作目录**（先 `cd` 到该根再跑）。
- **`--out`**：**输出根**；下建 **`<包名>/…`**。**省略 = `<--source>/gz_reverse/`**（例如 `…\zlib-1.2.13\gz_reverse\<包名>\`，与 **上游源码根同树**、子目录名固定为 `gz_reverse`）。
- **最简**：在含 `CMakeLists.txt` 的目录下**无参**执行即可（路径不必打）：

```text
cd E:\dev\egg_next\3rdparty\zlib-1.2.13
path\to\gz_reverse_cmake
```

- **工作区级或其它输出根**（仍扫描同一 zlib 源时）：

```text
path\to\gz_reverse_cmake --source E:\dev\egg_next\3rdparty\zlib-1.2.13 --out E:\dev\egg_next\gz_reverse
```

其它：`--package-name`、`--package-version`；**`--file-api <path>`** 可选，传入用户在本机外生成的 File API / `codemodel` 回复 JSON，**仅**抽取 target 名等与静态反解**对照**（**不**调用 `cmake`），详见 `ai-software-engineering/02-physical/gz-reverse-cmake/spec.md`。**`CMAKE_BINARY_DIR` / `CMAKE_CURRENT_BINARY_DIR`** 在工具内**仅**按与 **gz** 相同的 **`<--source>/.intermediate/build/<叶>`** 规则自动推算（叶名：存在则用 `default`，或 `build` 下**唯一**子目录，见 `spec.md`），**无** `--build-dir` 类参数。`--help` 查看全部参数。`target.xml` 中 **`<sources>`** 为编译单元 + 非对外头；**`<headers>`** 仅 **`<file from=…/>`** 表示可安装 public 头（启发式，见 `02-physical/gz-reverse-cmake/spec.md`）。将 `--out` 显式指到与 `--source` 同一路径时可能产生警告。

## 能力边界（与真实 CMake 的差异）

- **会**处理：`project`、`set`（部分 `${VAR}` 展开）、`include`（在文件存在时**内联**，环/重复跳过）、`include_directories`、`add_subdirectory`（**环/重复** Listfile 跳过）、`add_executable` / `add_library(STATIC|SHARED|MODULE|…)`、`target_sources`、`target_link_libraries`、`target_include_directories`、**`configure_file`（含 `INPUT`/`OUTPUT` 或两位置实参，映射为 `<config_files>`，归属规则见 `02-physical/gz-reverse-cmake/spec.md`）**；`if/elseif/else/endif` **可判定**子集压平后解释；`macro` **多轮**展开子集；`function` 体与 `foreach`/`while` 内仍以块深度屏蔽本工具主路径命令。
- **不**在工具内**执行** `cmake` 或**自动**发 File API；`if` 未覆盖的写法、未展开的 `$<…>` 在相关位置**跳过**；**不**执行 `file(GLOB)` 等需运行期逻辑。详见 **`spec.md`** 中 L1–L7 与「重要边界」。

**`configure_file` 与 GZ `config_files`（摘要）**：

- 支持 **`configure_file(a b …)`** 两位置实参，或 **`configure_file(INPUT x OUTPUT y …)`**；`COPYONLY` / `@ONLY` 等仅被忽略，**不**与真实 CMake 逐条等价。
- **归属**：同 `CMakeLists.txt` 内，若此前已有成功的 `add_executable` / `add_library`，`configure_file` 记入**该目标**的 `target.xml`；否则记入 **`package.xml`** 包级 `config_files`。
- **`to=`**：按 GZ 规范为**相对** `generated/<包>/<_package 或目标名>/` 的路径；反解**默认**只写**输出文件名**（不写整条 `.intermediate/build/...`）；子目录或改名需手改；与 `@VAR@` 替换无涉（见 `package-target-xml-spec`）。

生成结果需按业务核对；复杂场景请对照 **`doc/zh/package-target-xml-spec.md`** 手改或补全。
