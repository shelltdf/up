# gz 命令行参考（参数与工作模式）

> **文档索引**（`doc/zh` / `doc/en` 全部入口表）：[`../README.md`](../README.md)  
> **相关**：字段级 XML 语义见 [`package-target-xml-spec.md`](package-target-xml-spec.md) 与 **`gz spec`**；变量与缓存键见 [`internal-variables.md`](internal-variables.md)；易错参数与流程见 [`user-manual.md`](user-manual.md)。**物理层**（硬约束、`gz_dom` 形态、退出码摘要）见 **[`../../ai-software-engineering/02-physical/gz-cli/spec.md`](../../ai-software-engineering/02-physical/gz-cli/spec.md)**；**子命令→源码**见 **`../../ai-software-engineering/02-physical/gz-cli/mapping.md`**。

本文是 **`gz` argv 的单一事实来源**：按**当前实现**说明 argv 形态、**每个子命令**支持的开关、**工作目录（cwd）**语义、**中间目录布局**与**典型范例**。实现入口：`GroundZero/exe/cli_dispatch.cpp`（子命令分派）、`GroundZero/lib/infra/i18n/lang.cpp`（`gz --help` 文案）。

**编码**：本仓库 **Markdown / C++ 源码** 等统一为 **UTF-8 带 BOM**；根目录 **`build.py`** 等 **shebang 脚本** 为 **UTF-8 无 BOM**（见 **`ai-software-engineering/03-ops/developer-manual.md`**「源码与文档编码」）。

---

## 1. 总览：argv 形态与 cwd

- **可执行文件名**：Windows 上一般为 **`gz.exe`**；下文统一写作 **`gz`**。
- **当前工作目录（cwd）**：除另有说明外，**所有相对路径**（含 `--scan`、导出路径、`--build-dir-name` / `--install-dir-name` 解析出的树）均相对 **进程启动时的 cwd**（`std::filesystem::current_path()`）。
- **无配置文件子命令**：CLI **不**读取 `gz_gui_settings.txt`；**`--opt`** 与缓存 **`gz_cache.txt`** 才是工作区侧覆盖的主通道（GUI 会在调用 `gz configure` 时拼接 `--opt`，等价于命令行）。
- **路径字符集**：**`configure`** / **`list`** 对扫描到的 **`package.xml` / `target.xml` 路径**要求 **仅 ASCII**（非 ASCII 路径会失败并打印错误）。其它子命令主要依赖已生成的安装树/缓存路径。

### 1.1 顶层用法（与 `gz --help` 一致）

```text
gz [--verbose|-v] <子命令> ...
gz [--verbose|-v] print-build-dir-name [--build-dir-name <叶子>] [--opt KEY=VALUE]...
gz spec
gz list [--format tree|json|xml] [--xml <路径>] [--json <路径>] [--quiet] [--scan <目录>]...
gz configure [--build-dir-name <叶子>] [--scan <目录>]... [--opt KEY=VALUE]...
gz build --build-dir-name <叶子>
gz run --install-dir-name <名> <可执行目标名>
gz test --install-dir-name <名> [测试可执行文件名]
gz pack --install-dir-name <名> [--install-dir-name <名>]...
gz --help | -h | help
```

- **`help` / `-h` / `--help`**：可作为**第一个**参数（`gz help`），效果与 **`gz --help`** 相同（打印用法后 **退出码 0**）。
- **空参数**：仅输入 `gz` 且无其它参数时，**打印用法**，**退出码 0**。

### 1.2 全局选项：`--verbose` / `-v` 与 `GZ_VERBOSE`

- **位置**：**必须出现在子命令之前**（实现会先全局剥离再分派）。  
  **合法**：`gz -v configure ...`、`gz --verbose list`  
  **无效**：`gz configure -v ...`（`-v` 会落入 configure 的未知参数路径；configure 自身不识别 `-v`）
- **等价环境变量**：`GZ_VERBOSE` 为 **`1` / `true` / `yes` / `on`**（大小写不敏感）时，等价于开启 verbose（仍可与 `--verbose` 叠加，结果为开）。
- **行为**：打开后，部分阶段会向 **stderr** 打印进度类信息（与 **`lang.cpp`** 描述一致）；**不改变**子命令参数语义。

---

## 2. 中间目录与「叶子 / 架构名」

以下路径均在 **cwd** 下：

| 概念 | 典型路径 | 说明 |
|------|-----------|------|
| **构建叶子 `<叶子>`** | `.intermediate/build/<叶子>/` | **`configure`** 写入生成物与 **`gz_cache.txt`**；**`build`** 通过 **`--build-dir-name <叶子>`** 选中该目录。省略 **`configure --build-dir-name`** 时，实现使用 **`default`**。 |
| **安装目录名 `<名>`** | `.intermediate/install/<名>/` | **`build`** 完成后安装树根；**`run` / `test` / `pack`** 通过 **`--install-dir-name <名>`** 指向这里。`<名>` 通常等于 **`gz_cache.txt`** 中 **`arch=`** 或 **`gz print-build-dir-name`** 的一行输出（与 **构建叶子** 不必相同）。 |
| **打包输出** | `.intermediate/pack/<名>/` | **`pack`** 按每个 install 目录的**末段目录名**作为 `<名>` 写入打包产物（实现见 `pack.cpp`）。 |

### 2.1 `--build-dir-name` / `--install-dir-name` 的字符串规则

**`<叶子>` / `<名>`** 必须满足（`GroundZero/exe/cli_paths.cpp` **`intermediate_leaf_name_ok`**）：

- 非空，且不能是 **`.`**、**`..`**，不能包含子串 **`..`**；
- 不能含 **`/`**、**`\`**；
- Windows 下 additionally 不能含 **`:` `<` `>` `|` `?` `*`**。

否则子命令返回 **退出码 2** 并打印 **`invalid ...`** 类错误。

---

## 3. 退出码（便于脚本判断）

| 退出码 | 含义（概括） |
|--------|----------------|
| **0** | 成功；或仅打印 **`--help`** / 无参用法。 |
| **1** | **未知子命令**（会再打印用法）。 |
| **2** | **参数缺失/非法**、常见「找不到缓存/目录」等可恢复错误（各子命令 stderr 有说明）。 |
| **3** | 部分重型步骤失败（如 **`list`** DOM 构建失败以外的写盘失败等，见各命令实现）。 |
| **4** | **`list`**：导出 XML/JSON 文件或向 stdout 写 DOM 失败。 |
| **6** | **`list`**：`package.xml` / `target.xml` 路径含 **非 ASCII**。 |
| 其它 | **`run`** 等通过 **`std::system`** 转发子进程时，可能返回子进程退出码（Windows 上大于 255 时可能被压缩为 **1**，见 `test.cpp` 实现）。 |

更细的「物理规格」摘要见 **`ai-software-engineering/02-physical/gz-cli/spec.md`**。

---

## 4. 子命令：`print-build-dir-name`

**用途**：在**不执行 configure** 的前提下，读取 **`.intermediate/build/<叶子>/gz_cache.txt`**（**不存在则视为空选项映射**），再合并命令行传入的 **`GZ_*` `--opt`**，打印 **`arch`** 字符串（一行，stdout），供脚本传给 **`--install-dir-name`**。

```text
gz print-build-dir-name [--build-dir-name <叶子>] [--opt KEY=VALUE]...
```

| 参数 | 必填 | 说明 |
|------|------|------|
| **`--build-dir-name <叶子>`** | 否 | 指定读取 **`.intermediate/build/<叶子>/gz_cache.txt`**。省略时等价于 **`default`**。 |
| **`--opt KEY=VALUE`** | 否 | 可重复。仅 **`KEY` 以 `GZ_` 开头**的项会参与合并并影响输出；其它键被 **忽略**（与 **`configure --opt`** 接受任意可合并键不同）。 |
| **`--opt=KEY=VALUE`** | 否 | 单 token 形式，等价于 **`--opt KEY=VALUE`**。 |

**合并逻辑（简）**：先加载缓存中的选项映射，再应用命令行 **`GZ_*` `--opt`** 覆盖，最后计算 **`arch`** 并打印。若 **`--build-dir-name`** 非法 → **退出码 2**。

**范例**

```powershell
# 已 configure 过 default 叶子，读取缓存并打印 arch
gz print-build-dir-name

# 显式指定叶子与生成器（影响 arch 字符串时使用）
gz print-build-dir-name --build-dir-name default --opt GZ_CMAKE_GENERATOR=Ninja
```

---

## 5. 子命令：`configure`

**用途**：扫描 **`package.xml` / `target.xml`**，生成后端工程（CMake 或 Ninja 等），并在 **`.intermediate/build/<叶子>/`** 写入 **`gz_cache.txt`**（含 **`arch=`**、**`scan_roots=`** 等元数据与合并选项）。

```text
gz configure [--build-dir-name <叶子>] [--scan <目录>]... [--opt KEY=VALUE]...
```

| 参数 | 必填 | 说明 |
|------|------|------|
| **`--build-dir-name <叶子>`** | 否 | 生成与写入的构建叶子；省略为 **`default`**。非法叶子 → **退出码 2**。 |
| **`--scan <目录>`** | 否 | 可 **重复**；每个目录加入扫描根列表，与 **cwd** 一起递归查找 `package.xml` / `target.xml`（**跳过**名为 **`.intermediate`** 的子树）。未传任何 **`--scan`** 时，仅按 **cwd** 扫描。 |
| **`--opt KEY=VALUE`** | 否 | 可重复；亦可 **`--opt=KEY=VALUE`**。用于覆盖 **`GZ_*`** 及项目自定义键（合并规则见 **`internal-variables.md`** / **`gz spec`**）。 |

**路径**：扫描到的 XML 路径必须 **ASCII**（否则 configure 失败）。

**范例**

```powershell
# 在仓库根：扫描 test_projects 下所有示例包
gz configure --scan test_projects

# 指定构建叶子 + 覆盖生成器
gz configure --build-dir-name myleaf --scan . --opt GZ_CMAKE_GENERATOR="Visual Studio 17 2022"

# 在单包目录下只扫描当前树（cwd 为包根）
Set-Location test_projects\hello_demo
gz configure
```

---

## 6. 子命令：`build`

**用途**：读取 **`.intermediate/build/<叶子>/gz_cache.txt`**，对对应已生成工程执行编译并 **install** 到 **`.intermediate/install/<arch>/`**。

```text
gz build --build-dir-name <叶子>
```

| 参数 | 必填 | 说明 |
|------|------|------|
| **`--build-dir-name <叶子>`** | **是** | 缺少时 stderr 提示 **`missing required`**，**退出码 2**。 |

**范例**

```powershell
gz build --build-dir-name default
```

---

## 7. 子命令：`run`

**用途**：在 **安装树** 的 **`bin/`** 下查找可执行文件并 **`std::system`** 启动（Windows 下若目标名无扩展名，会自动补 **`.exe`**）。

```text
gz run --install-dir-name <名> <可执行目标名>
```

| 参数 | 必填 | 说明 |
|------|------|------|
| **`--install-dir-name <名>`** | **是** | 指向 **`.intermediate/install/<名>`**；非法名 → **退出码 2**。 |
| **`<可执行目标名>`** | **是** | 对应安装后的可执行**文件名 stem**（无路径）；缺失时 **退出码 1**。 |

**范例**

```powershell
$ARCH = gz print-build-dir-name --build-dir-name default
gz run --install-dir-name $ARCH hello_demo
```

---

## 8. 子命令：`test`

**用途**：在 **`.intermediate/install/<名>/bin/`** 下查找**测试可执行文件**并依次运行（当前后端为「目录扫描 + `std::system`」，见 `test.cpp` / `backend_dispatch.cpp`）。

```text
gz test --install-dir-name <名> [测试可执行文件名]
```

| 参数 | 必填 | 说明 |
|------|------|------|
| **`--install-dir-name <名>`** | **是** | 同 **`run`**。 |
| **`[测试可执行文件名]`** | 否 | **省略时**：在 `bin` 下枚举可执行文件，**仅运行文件名（stem）中含 `test`（大小写不敏感）** 的项，可多个。  
  **指定时**：只运行 **stem 与参数完全匹配（大小写不敏感）** 的那一个。 |

若 **`bin`** 不存在 → **退出码 2**；若过滤后无匹配测试 → **退出码 2**。

**范例**

```powershell
# 运行所有名字里带 test 的测试 exe
gz test --install-dir-name $ARCH

# 只跑某一个测试 exe（stem 与参数一致）
gz test --install-dir-name $ARCH hello_foo_test
```

---

## 9. 子命令：`pack`

**用途**：将一个或多个 **安装树** 打成包（实现上先尝试 **CPack**，失败则回退 **archive**；细节见 `pack.cpp` / `backend_dispatch.cpp`）。输出在 **`.intermediate/pack/<名>/`**。

```text
gz pack --install-dir-name <名> [--install-dir-name <名>]...
```

| 参数 | 必填 | 说明 |
|------|------|------|
| **`--install-dir-name <名>`** | **至少一次** | 可 **重复**，用于多架构/多安装树依次打包。每次给出一个 **install 子目录名**（不是完整路径）。 |

无任何 **`--install-dir-name`** → **退出码 2**。

**范例**

```powershell
gz pack --install-dir-name $ARCH
gz pack --install-dir-name win-x64-msvc --install-dir-name win-arm64-msvc
```

---

## 10. 子命令：`list`

**用途**：从 **cwd + 各 `--scan` 根`** 收集 **`package.xml` / `target.xml`**，构建内存 **DOM**，打印树或 JSON/XML，并可导出文件。

```text
gz list [--format tree|json|xml] [--xml <路径>] [--json <路径>] [--quiet] [--scan <目录>]...
```

| 参数 | 必填 | 说明 |
|------|------|------|
| **`--format tree\|json\|xml`** | 否 | 选择 **stdout 主载荷**；默认 **`tree`**。非法值 → **退出码 2**。 |
| **`--xml <路径>`** | 否 | 将 DOM 写出为 **XML 文件**（根元素 **`<gz_dom>`**）。相对路径相对 **cwd**。成功且非 quiet 时打印一行导出提示。 |
| **`--json <路径>`** | 否 | 将 DOM 写出为 **JSON 文件**。规则同上。 |
| **`--quiet`** | 否 | 抑制 **树形文本**与 **导出成功提示行**；**不**抑制 **`--format json|xml`** 时 stdout 上的 JSON/XML 载荷。 |
| **`--scan <目录>`** | 否 | 可重复；指定额外扫描根。若显式传了 **`--scan`** 且列表中 **未**包含与 **cwd** 等价的根，实现会 **自动把 cwd 追加进扫描根列表**（仍会去重）。 |

**警告（不失败）**：

- **`--format xml`** 且同时 **`--json <路径>`**（stdout 为 XML、文件为 JSON）→ stderr **warning**。
- **`--format json`** 且同时 **`--xml <路径>`** → stderr **warning**。

**未找到任何 `package.xml`/`target.xml`** → **退出码 2**。

**范例**

```powershell
# 默认树形打印
gz list

# stdout JSON
gz list --format json

# 导出 XML 文件，并抑制树与提示
gz list --xml .intermediate/dom.xml --quiet

# 额外扫描其它目录（cwd 仍会被包含）
gz list --scan test_projects --scan examples
```

---

## 11. 子命令：`spec`

**用途**：向 **stdout** 打印 **内嵌英文** 的 **`package.xml` / `target.xml`** 规则摘要（`GZ_XML_SPEC_REVISION=...` 起头的长文），供离线工具/AI 使用。

```text
gz spec [任意附加参数...]
```

- **当前实现**会 **忽略** 所有附加参数（`cmd_spec` 不解析 argv），始终打印全文并 **退出码 0**。

**范例**

```powershell
gz spec > spec-embedded.txt
```

---

## 12. 端到端范例（PowerShell，仓库根）

> **`_build`** 仅为与 **`cmake -B`** 一致的**示例目录名**；若你使用 **`_build_gz`** 等，请替换下面所有 **`.\_build\Release\`** 前缀。详见 [`user-manual.md`](user-manual.md)「**0. 10 分钟快速上手**」。

```powershell
cmake -S . -B _build -G "Visual Studio 17 2022" -A x64
cmake --build _build --config Release

.\_build\Release\gz.exe configure --scan test_projects
$ARCH = .\_build\Release\gz.exe print-build-dir-name
.\_build\Release\gz.exe build --build-dir-name default
.\_build\Release\gz.exe test --install-dir-name $ARCH
.\_build\Release\gz.exe run --install-dir-name $ARCH hello_demo
.\_build\Release\gz.exe list --xml .intermediate\dom.xml --quiet
```

将 **`gz.exe` 所在目录加入 PATH** 后，可直接写 **`gz configure`** 等（仍须在正确 **cwd** 下执行）。

---

## 13. 与 GUI / 脚本的边界

- **CLI 不读 `gz_gui_settings.txt`**；GUI 在发起 configure 时会把环境页选项转成 **`--opt`**。
- **`build.py` / `install.py`** 只驱动**仓库根 CMake** 构建 **`gz` / `gz-gui`**，**不**代替你对 **`test_projects/`** 执行 **`gz configure`**（见根目录 **`README.md`**）。

---

## 14. 参考阅读（实现与规格）

| 主题 | 路径 |
|------|------|
| CLI 分派与 `print-build-dir-name` | `GroundZero/exe/cli_dispatch.cpp` |
| 用法文案 | `GroundZero/lib/infra/i18n/lang.cpp` |
| `list` 参数解析 | `GroundZero/lib/engine/commands/list.cpp` |
| 中间目录根 | `GroundZero/lib/infra/platform/paths.cpp` |
| 物理层规格（目录/硬约束/`gz_dom`；**argv 以本文为准**） | `ai-software-engineering/02-physical/gz-cli/spec.md` |
| 子命令 → 主实现源文件 | `ai-software-engineering/02-physical/gz-cli/mapping.md` |

---

[← `doc/README.md`](../README.md) · [`user-manual.md`](user-manual.md) · [`getting-started.md`](getting-started.md)
