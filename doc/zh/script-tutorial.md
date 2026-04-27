# 脚本与构建流程范例教程

> **文档索引**（`doc/zh` / `doc/en` 全部入口表）：[`../README.md`](../README.md)

从「最小可运行包」到「条件源文件、自定义变量、复杂预处理/子工程」。**从零按步扩展**见 **`getting-started.md`**。**命令与路径**以仓库 `README.md` / **`user-manual.md`** 为准；**XML 细节**以 **`package-target-xml-spec.md`** 与 **`up spec`** 为准。

---

## 1. 最小示例：一个包 + 一个可执行目标

### 1.1 目录与 `package.xml`

在空目录下建立包根，例如 `hello_pkg/package.xml`：

```xml
<?xml version="1.0" encoding="UTF-8"?>
<package name="hello_pkg" version="0.1.0">
</package>
```

### 1.2 `target.xml`（可执行文件）

`hello_pkg/hello_exe/target.xml`：

```xml
<?xml version="1.0" encoding="UTF-8"?>
<target name="hello" type="executable">
  <sources>
    <file>main.cpp</file>
  </sources>
</target>
```

同目录放 `main.cpp`（普通 `int main()` 即可）。

### 1.3 配置、编译、运行

在**包树根的上一级**（或任意 cwd，用 `--scan` 指到含 `package.xml` 的目录）执行：

```bash
up configure
up build --build-dir-name default
up print-build-dir-name --build-dir-name default
up run --install-dir-name <上一步输出的 arch 目录名> hello
```

要点：

- **`build`** 必须带 **`--build-dir-name`**，与 configure 使用的叶子名一致。  
- **`run` / `test` / `pack`** 必须带 **`--install-dir-name`**，值为 **`.intermediate/install/` 下子目录名**，通常等于 **`up_cache.txt` 的 `arch=`** 或 **`up print-build-dir-name`** 的结果（**不是**构建叶子 `default` 本身）。

---

## 2. 如何判断「系统 / 内置」条件：用 `when`

内置变量见 **`internal-variables.md` §2**，在 **`when`** 与模板 `@KEY@` 中均可使用。

### 2.1 仅 Windows 编译某源文件

```xml
<sources>
  <file from="main.cpp"/>
  <file from="win_only.cpp" when="UP_OS==windows"/>
</sources>
```

### 2.2 用裸变量表示「非空即真」

先在 `<vars>` 里定义开关，再在 `when` 里引用：

```xml
<vars>
  <var name="USE_NET" value="on"/>
</vars>
<sources>
  <file from="net_client.cpp" when="USE_NET"/>
</sources>
```

**`when` 语法子集**（必须整串匹配一种形式）：空串（真）、`true`/`false`、**`KEY == token`** / **`KEY != token`**（`token` 仅 `[A-Za-z0-9_.]+`）、或**单个已存在键**的真值判断。详见 `package-target-xml-spec.md` §3.5。

---

## 3. 添加自定义变量

### 3.1 在 XML 中设默认值

包级或目标级：

```xml
<vars>
  <var name="MY_REV" value="dev"/>
</vars>
```

### 3.2 在 configure 时覆盖

```bash
up configure --opt MY_REV=rc2
```

或在 **`up_cache.txt`** 中增加一行 `MY_REV=rc2` 后再次 **configure**（注意合并顺序：**命令行与缓存后写覆盖**）。

### 3.3 在生成的头 / 源模板里使用

**`<config_files>`** 中：

```xml
<config_files>
  <file in="version.h.in" to="generated/version.h"/>
</config_files>
```

`version.h.in` 内可使用 **`@MY_REV@`** 或 **`${MY_REV}`**；支持 **`#cmakedefine`** 子集。见 `package-target-xml-spec.md` §3.5。

---

## 4. 预处理 / 后处理：复杂编译的第一步

对**某个源条目**可在 `<sources>` 内嵌 **`<preprocess/>` / `<postprocess/>`**（自闭合，`command="..."`），实现会生成 **CMake `add_custom_command`** 或 **Ninja 规则** 调用该命令（**由 shell/cmd 解释**，不是 Lua）。

示例：在编译前生成代码（命令需自行保证可重复、路径正确）：

```xml
<sources>
  <file from="gen_input.txt">
    <preprocess command="python tools/gen_code.py gen_input.txt gen_out.cpp"/>
  </file>
  <file from="gen_out.cpp"/>
</sources>
```

**注意**：命令字符串会进入生成后端；请使用 **可移植** 或 **按 OS 拆分源条目 + `when`** 的方式维护 Windows/Linux 差异。

**头文件 / 安装规则** 侧也有类似的 **`<headers>` … preprocess/postprocess** 与 **资源** 侧规则，见 `simple_xml.cpp` 解析与 `cmake_backend.cpp` / `ninja_backend.cpp` 生成逻辑。

**与「消息 / 元编程挂钩」的对应关系**：合法 **`trigger`**、是否在 **configure** 派发、与 **`<preprocess>`** 优先级、包/目标 **Dom 继承**，见 **`script-messages.md`**（实现见 `src/lib/engine/dom/script_execution.cpp`）。

XML 里写了 **`command="..."`** 时，该字符串即最终交给后端的命令；**未写** `command` 时，configure 会尝试用同 trigger 的 **`<var type="script" … value="..."/>`** 的 **`value` 整段**作为命令（`resolve_script_command`：**有 XML 命令则不再合并脚本 var**）。当前仓库实现**不会**对 `value` 跑 Lua 虚拟机，名称里的 `script_type="lua"` 仅作类型筛选；要接 **moc/uic/rcc** 等外部程序，应把可执行命令行写在 **`preprocess command=`** 或上述 **`value`** 中。

---

## 5. `<var type="script" …>`（Lua 与触发器）

在 `<vars>` 中可声明 **脚本变量**（**自闭合**，命令写在 **`value`**）：

```xml
<var name="hook" type="script" trigger="sources.preprocess" script_type="lua" value="echo prepare-sources"/>
```

支持的 **`trigger`** 取值与 **`manual` 预留**见 **`script-messages.md`** 总表（解析校验在 `simple_xml.cpp` 的 `is_supported_script_trigger`）。**当前实现**对 **`script_type`** 非 `lua` 的脚本在部分路径会跳过；复杂逻辑请优先使用 **§4 的 shell `preprocess`** 或 **独立工具目标**。若希望「同一阶段只跑脚本、不写 XML `command`」，可把 **一整行 shell** 放进 **`value`**。

---

## 6. 遗留：CMake 子工程 `<cmake/>`（不推荐）

> **产品方向**：新工程请**纯手写** `package.xml` / `target.xml`，在包外用上游 CMake 完成构建与 **`install`**，再用 **`imported_installed_*`** 等描述产物（见 **`package-target-xml-spec.md`** 文首与 **`getting-started.md`**）。本节仅保留**实现仍支持的**形状摘要，便于阅读旧仓库或 `test_projects/native_cmake_vendor` 等回归用例。

当需要 **上游完整 CMake 工程**（`ExternalProject` 等）时，在 **`package.xml`** 使用 **`<cmake/>`**（形状与限制见 **`up spec`** 与 `package-target-xml-spec.md`）。

限制摘要：

- **仅 `UP_TARGET_BUILD_SYSTEM=cmake` 时** 支持聚合 `<cmake/>`；**Ninja 顶层模式**下会报错。  
- 若 **`UP_DISABLE_PACKAGE_XML_CMAKE=ON`** 的 `up.exe`，**`<cmake/>` 不解析**。

子工程参数可通过 **`UPSTREAM_*`** 键传入（见 **`internal-variables.md` §6**）。

---

## 7. 调试变量合并结果

- 每次 configure 后在 **`.intermediate/build/<叶子>/packages.md`** 查看包/目标及 **`<vars>` 与选项覆盖** 的摘要。  
- 直接阅读 **`up_cache.txt`** 核对最终写入的 **`UP_*` 与自定义键**。  
- 使用 **`up configure --verbose`**（或 **`UP_VERBOSE=1`**）查看阶段日志。

---

## 8. 外部元编程工具：以 Qt **moc / uic / rcc** 为例

本节的思路是：**不用** Qt CMake 的 `AUTOMOC` / `AUTOUIC` / `AUTORCC`（生成器当前也不会替你调用 `find_package(Qt6)`），而是把 **Qt 自带命令行工具**挂到 **§4** 的 **`preprocess`**（或 **`headers` / `assets` 上的 preprocess**）里，把「输入文件 → 生成 `.cpp` / `.h`」写清楚，再把生成物当作普通 **`<sources>` / `<headers>`** 参与编译或安装。

### 8.1 通用约定

1. **工作目录与路径**：`command` 由 **shell/cmd** 执行；请使用相对 **目标目录**（`target.xml` 所在目录）的可靠路径。**注意**：与 **`<config_files>`** 模板不同，`preprocess` 的 **`command` 字符串在 configure 阶段不会做 `@KEY@` / `${KEY}` 替换**；若要用「可配置 Qt 路径」，可依赖 **构建时 `PATH`**、写 **包装脚本**（脚本内读环境变量），或在 **`<vars>`** 中维护语义后由你在命令里手写可解析片段（例如仅 Unix 下用 `$QTDIR/bin/moc`，由 shell 展开）。
2. **生成物必须显式列出**：例如 `moc` 写出 `gen/moc_widget.cpp`，则目标里要有 **`<file from="gen/moc_widget.cpp"/>`**（并保证 preprocess 先创建该文件）；不要假设「只编译 widget.cpp 就会自动带上 moc 输出」。
3. **包含目录**：`uic` 生成的 `ui_*.h`、`moc` 生成文件若放在子目录，需 **`target.xml` 的 `<includes>`**（若项目支持）或包级约定，使编译器能找到 `#include "ui_mainwindow.h"` 等。
4. **链接 Qt 库**：在 **`target.xml`** 的 **`<dependency name="…"/>`** 指向已安装好的 **导入库目标**（`imported_*`）或本包编译库；**推荐**在包外安装 Qt 再手写依赖，**不**以 **`<cmake/>`** 为默认路径（见 §6）；本节只解决「生成代码」一步。

### 8.2 **moc**（元对象编译器）

典型做法：对含 `Q_OBJECT` 的头文件，在**真正编译该翻译单元之前**生成 `moc_*.cpp`。

- **方式 A（推荐）**：把 **`moc` 输出**单独列为源文件，在**输入头**或**桩 .cpp** 上挂 preprocess（二选一，只要能保证顺序即可）。

```xml
<sources>
  <!-- 在编译 widget.cpp 之前根据 widget.hpp 生成 moc 源 -->
  <file from="widget.cpp">
    <preprocess command="moc widget.hpp -o gen/moc_widget.cpp"/>
  </file>
  <file from="gen/moc_widget.cpp"/>
</sources>
```

（Windows 下请把 `command` 改成你环境可用的形式，例如 `cmd /c` 或 `powershell -Command`，或使用 `when` 拆平台。）

- **方式 B**：对 **`widget.hpp`** 本身建一条 **仅用于触发 moc、不参与编译** 的条目在部分后端上不可行（头文件通常不在 `<sources>`）；更稳妥是 **方式 A** 或把 `widget.hpp` 放进 **`<headers>`** 并在 **`headers.preprocess`** 里生成 `gen/moc_widget.cpp`，再在 `<sources>` 里包含该 cpp。

### 8.3 **uic**（UI 表单 → 头文件）

`.ui` 可放在目标目录，用 **`sources`** 或 **`assets`** 引用（以你希望的安装行为为准），在编译依赖该 UI 的 `.cpp` **之前**生成头文件：

```xml
<sources>
  <file from="mainwindow.cpp">
    <preprocess command="uic mainwindow.ui -o gen/ui_mainwindow.h"/>
  </file>
</sources>
```

`mainwindow.cpp` 内 `#include "gen/ui_mainwindow.h"`（路径与 **`target_include_directories`** / `<includes>` 对齐）。

### 8.4 **rcc**（资源 → 源文件）

把 **`.qrc`** 放在 **`<assets>`** 或 **`<sources>`**（若仅生成中间 cpp、不安装 qrc），在 **`assets.preprocess`** 或 **`sources.preprocess`** 中调用 **`rcc`**：

```xml
<assets>
  <file from="app.qrc">
    <preprocess command="rcc -name qapp app.qrc -o gen/qrc_app.cpp"/>
  </file>
</assets>
```

然后在 **`<sources>`** 中加入 **`<file from="gen/qrc_app.cpp"/>`**。

### 8.5 与 **`<var type="script" trigger="…">`** 组合

- 若某条 **`<preprocess/>` 不写 `command`**，可设 **`<var type="script" trigger="sources.preprocess" script_type="lua" value="…"/>`**，其 **`value` 即整条 shell 命令**（见 §4 表格）。适合把冗长命令集中在包级变量旁维护。
- **不要**期望在已有 **`command="moc …"`** 的同一条目上再自动拼接 Lua：**有 XML 命令时不会读取脚本 var 作为补充**。

### 8.6 后端差异（务必读）

- **Ninja**：对每个带 `preprocess` 的源，在编译该 `.o` 前生成 **stamp**，命令串来自 §4；**可执行目标与库目标行为一致**。
- **CMake**（`src/lib/engine/backends/cmake/cmake_backend.cpp`）：当前仅为 **`static_library` / `shared_library`** 的 `<sources>` 生成 **`add_custom_command` + `add_dependencies`**，以及库上的 **`POST_BUILD`** postprocess；**`executable` 目标不会**为 `<sources>` 插入上述规则。若你使用 **CMake 作为 `UP` 生成后端**且目标是 **exe**，请任选其一：  
  - 把含 moc/uic/rcc 的代码放进 **`static_library` 子目标**，主程序 **`<dependency name="…"/>`** 链它；或  
  - 改用 **Ninja** 顶层构建；或  
  - 在包外维护上游 Qt CMake 工程，**手写**本包 `target.xml` 描述对预安装库的消费（遗留场景下才考虑 **`<cmake/>`**，见 §6）。

---

## 9. 相关链接

| 文档 | 内容 |
|------|------|
| `internal-variables.md` | 内置键、`up_cache` 固定行、`UP_*` 列表 |
| `package-target-xml-spec.md` | XML / `when` / `config_files` |
| `user-manual.md` | 终端用户命令与目录约定 |
