# 内部变量与可覆盖键总览

> **文档索引**（`doc/zh` / `doc/en` 全部入口表）：[`../README.md`](../README.md)

本文汇总 **gz（GroundZero）** 在 **configure / build / 模板替换 / 缓存** 中出现的变量与键名，便于区分：**实现自动写入**、**用户在命令行或 GUI 传入**、**项目在 XML 中声明**。  
**权威细节**仍以 `gz spec`（内嵌规范）、`package-target-xml-spec.md` 与源码为准；本表侧重检索与分工说明。

---

## 1. 变量出现在哪里

| 载体 | 作用 |
|------|------|
| **内置映射（configure 上下文）** | 用于 `@KEY@` / `${KEY}`、`when="..."` 求值；由实现按当前包/目标/OS 等填入（见 `GroundZero/lib/engine/xml/var_subst.cpp`）。 |
| **`package.xml` / `target.xml` 的 `<vars>`** | 包级/目标级**默认值**；可被下层覆盖。 |
| **`gz configure --opt KEY=VALUE`** | 工作区覆盖；与缓存合并时**后出现者优先**（同 `gz_cache.txt` 内多行同键时通常以后者为准，具体以合并实现为准）。 |
| **`.intermediate/build/<叶子>/gz_cache.txt`** | configure 结束时写入；含固定元数据行 + **整份合并后的选项映射**（键值对）。 |

**合并顺序（`@` / `when` / 包级 `config_files`）**：内置 → 包 `<vars>` → 目标 `<vars>` → `--opt` / `gz_cache.txt`（后者覆盖前者）。详见 `package-target-xml-spec.md` §3.5。

---

## 2. 内置变量（configure 为每个目标/包填入）

在 **`when`**、**`<config_files>`** 等合并表中由实现提供（名称固定）：

| 变量名 | 含义（典型取值） |
|--------|------------------|
| `GZ_OS` | 主机 OS：`windows` / `linux` / `darwin` |
| `GZ_PACKAGE_NAME` | 当前包名（`package.xml` 的 `name`） |
| `GZ_PACKAGE_VERSION` | 包版本（缺省 `0.0.0`） |
| `GZ_TARGET_NAME` | 当前目标名；**包级模板中默认为空**，可被目标 `<var name="GZ_TARGET_NAME" …/>` 覆盖 |
| `GZ_TARGET_BUILD_SYSTEM` | `cmake` 或 `ninja` |
| `GZ_CONFIG` | `debug` 或 `release`（由 Debug/Release 类选项推导） |

---

## 3. `gz_cache.txt` 固定行（实现自动生成）

路径：`.intermediate/build/<叶子>/gz_cache.txt`（`<叶子>` 对应 `configure --build-dir-name`，省略时常为 `default`）。

| 键 | 说明 |
|----|------|
| `gz.cache.version` | 缓存格式版本（当前为 `1`） |
| `cwd` | configure 时工作目录（绝对路径，POSIX 斜杠风格由实现写出） |
| `arch` | **安装目录名**用的组合标签（如 `windows_x86_64_cmake_msvc_dynamic_release`）；与 **构建叶子名** 不必相同 |
| `package` | 主包名（实现记录用） |
| `generated_file` | 生成入口文件绝对路径（如根 `CMakeLists.txt` 或 `out/build.ninja`） |
| `scan_roots` | 分号分隔的扫描根绝对路径列表 |

其后为 **任意数量** `KEY=VALUE` 行：来自合并后的 **configure 选项表**（含下文「常用 `GZ_*`」及你在 `--opt` / 旧缓存中写入的键）。

---

## 4. 常用 `GZ_*` 选项（用户 / GUI / 缓存可写）

下列键名在 **`arch` 组合**、**build 并行度**、**CMake 行为** 等路径中被读取；**兼容别名**在括号中列出（实现侧 `option_or_compat` 一类逻辑，见 `commands_common.cpp` / `configure.cpp` / `build.cpp`）。

| 键（主名） | 别名或备注 | 用途摘要 |
|------------|------------|----------|
| `GZ_TARGET_CPU_ARCH` | `GZ_CPU_ARCH` | 目标 CPU 标签（参与 `arch` 组合） |
| `GZ_TARGET_SYSTEM` | `GZ_SYSTEM` | 目标系统标签 |
| `GZ_TARGET_DYNAMIC_LIBRARY` | `GZ_DYNAMIC_LIBRARY` | `ON`/`OFF` 等，动态/静态链接倾向 |
| `GZ_TARGET_DEBUG` | `GZ_DEBUG` | `ON`/`OFF` 等，Debug/Release |
| `GZ_TARGET_BUILD_SYSTEM` | `GZ_BUILD_SYSTEM` | `cmake` 或 `ninja` |
| `GZ_TARGET_CRT` | `GZ_CRT` | Windows CRT 模式，如 `dynamic_md` |
| `GZ_CMAKE_GENERATOR` | — | 传给 CMake 的 `-G`（可影响工具链标签推断） |
| `GZ_CMAKE_PREFIX_PATH` | — | `CMAKE_PREFIX_PATH` 类前缀（configure 写入缓存等） |
| `GZ_BUILD_PARALLEL` | `GZ_BUILD_JOBS` | 并行编译线程数（二者为别名；缺省由 configure 按 CPU 数写入） |

**还可写入**：所有 **`GZ_*`**，以及符合 C 标识符规则的 **自定义键**（用于 XML `<vars>` 默认值覆盖、模板占位等）；禁止使用的保留键以实现校验为准（参见 `gz spec` 内说明）。

---

## 5. 项目 XML 中的「变量」

| 来源 | 形式 | 说明 |
|------|------|------|
| `package.xml` `<vars>` | `<var name="KEY" value="VAL"/>` | 包内共享默认值 |
| `target.xml` `<vars>` | 同上 | 目标级覆盖包级 |
| `<define name="..." value="..."/>` | 预处理器宏 | **不是**合并表中的 `KEY`；进入编译定义，不参与 `when` 键查找（除非另有同名 `<var>`） |

**脚本型 `<var>`**（`type="script"`）：带 `trigger`（如 `sources.preprocess`），`script_type` 默认 `lua`；与 **`<sources>` 内 `<preprocess command="..."/>`** 等 **shell 命令** 是不同机制——前者走脚本管线定义，后者直接进生成后端规则。**全部合法 `trigger`、派发阶段与绑定规则**见 **`script-messages.md`**；流程与 Qt 见 `script-tutorial.md`。

---

## 6. 环境变量（与 CLI 并列，非 `gz_cache` 键）

| 变量 | 作用 |
|------|------|
| `GZ_VERBOSE` / `gz --verbose` | 阶段日志等到 stderr（见 CLI 帮助） |

（其它与 **编译器/SDK 路径探测** 相关的环境变量由 **gz-gui Win32** 或宿主环境提供，用于生成 `--opt`；它们**不**自动进入 `gz_cache.txt`，除非通过 configure 写入。）

---

## 7. gz-gui 写入的 `gz_gui_settings.txt`（UTF-8）

与 **`gz configure --opt`** 对齐的持久化键（节选，完整见 `GroundZeroGUI/core/gui_persist.hpp`）：

| 键 | 含义 |
|----|------|
| `local.build_system` | `cmake` / `ninja` |
| `local.compiler` | 如 `msvc` |
| `android.sdk` / `android.ndk` | 路径 |
| `emsdk.path` | EMSDK 根 |
| `local.vcvars` / `local.vcvars64` / `local.vcvars32` | vcvars 路径 |
| `browse.*` | 各对话框上次路径 |
| `gui.ui_lang` | `zh` / `en` |

GUI 会将上述项转成 **`--opt GZ_*=...`** 片段参与 configure（与命令行一致）。

---

## 8. 自动占位（模板中有、表中无）

对 **`<config_files>`** 模板：若出现 **`@NAME@` / `${NAME}`** 且合并表中尚无 `NAME`，实现会 **以空串加入表** 并删掉占位符（不保留字面 `${NAME}`）。需要在 XML `<vars>` 或 `--opt` 中显式赋值才能得到非空展开。见 `package-target-xml-spec.md` §3.5。

---

## 9. 相关文档

- `package-target-xml-spec.md` — XML、`when`、合并顺序
- `user-manual.md` — `configure` / `build` / `run` 与 `arch`、`--install-dir-name`
- `DESIGN.md` — 中间目录与命令总览  

若本表与 **`gz spec`** 或源码不一致，以 **`gz spec` 与源码** 为准，并欢迎提 PR 更新本页。
