# `doc/` 文档索引

本目录为 **实现侧补充文档**（与 `ai-software-engineering/` 四阶段文档互补）。正文与仓库源码一致采用 **UTF-8 带 BOM**（**`*.py` shebang 脚本**除外，见 **`ai-software-engineering/03-ops/developer-manual.md`**）。**产品方向**：**纯手写** **`package.xml` / `target.xml`**（见 **[zh/package-target-xml-spec.md](zh/package-target-xml-spec.md)** 文首、**`gz spec`**）。

**从源码构建 `gz`**：根目录 **`README.md`** 与 **[zh/user-manual.md](zh/user-manual.md)** / **[en/user-manual.md](en/user-manual.md)**「**0. 快速入口与构建目录约定** / **§0 Quick entry**」等处若出现 **`.\_build\Release\gz.exe`** 等路径，**`_build`** 仅为 **`cmake -B`** 的示例目录名（亦可是 **`_build_gz`** 等），须与本机实际构建目录一致。**[zh/getting-started.md](zh/getting-started.md)** / **[en/getting-started.md](en/getting-started.md)** §0 亦有交叉引用。**子命令、开关、退出码与范例** 的完整清单见 **[zh/cli-reference.md](zh/cli-reference.md)** 或 **[en/cli-reference.md](en/cli-reference.md)**（均为完整正文）。

**推荐阅读顺序**（与根 **`README.md`** 一致）：**getting-started** → **cli-reference** → **user-manual** → 写 XML 时 **package-target-xml-spec** + **`gz spec`**（**`GZ_XML_SPEC_REVISION`** 与节号/三条主线对照见 **package-target-xml-spec** 文内「与内嵌 / Alignment」表；机器摘要以 `gz spec` 与 `GroundZero/lib/engine/commands/spec.cpp` 为准）。

**`doc/zh/` 与 `doc/en/` 文件名一一对应**：上表每一行的 **`zh/*.md`** 与 **`en/*.md`** 均为**完整正文**（中英各一份，结构与职责对齐；若与 **`gz spec`** 或源码冲突，以 **`gz spec` 与源码** 为准）。

| 主题 | 中文 `zh/` | 英文 `en/` |
|------|------------|------------|
| 用户手册（文档分工、鸟瞰、FAQ、gz-gui） | [zh/user-manual.md](zh/user-manual.md) | [en/user-manual.md](en/user-manual.md) |
| **CLI 参数与工作模式** | [zh/cli-reference.md](zh/cli-reference.md) | [en/cli-reference.md](en/cli-reference.md) |
| 分步入门 | [zh/getting-started.md](zh/getting-started.md) | [en/getting-started.md](en/getting-started.md) |
| XML 规范 | [zh/package-target-xml-spec.md](zh/package-target-xml-spec.md) | [en/package-target-xml-spec.md](en/package-target-xml-spec.md) |
| 内置变量与缓存键 | [zh/internal-variables.md](zh/internal-variables.md) | [en/internal-variables.md](en/internal-variables.md) |
| `trigger` 消息表 | [zh/script-messages.md](zh/script-messages.md) | [en/script-messages.md](en/script-messages.md) |
| 脚本与预处理教程 | [zh/script-tutorial.md](zh/script-tutorial.md) | [en/script-tutorial.md](en/script-tutorial.md) |
| **CMake 逆向为 GZ XML**（`gz_reverse_cmake` 功能与用法） | [zh/gz-reverse-cmake.md](zh/gz-reverse-cmake.md) | [en/gz-reverse-cmake.md](en/gz-reverse-cmake.md) |

正文路径均在 **`zh/`** / **`en/`** 上表；仓库根 **`README.md`**、**`DESIGN.md`** 等可链 **`doc/zh/...`** 或 **`doc/en/...`** 任选语言。**不再**在 `doc/` 根目录保留同名跳转 `.md`，避免与 **`doc/README.md`** 重复维护。

仓库总览与命令表见根目录 **`README.md`**；设计与中间目录见 **`DESIGN.md`**。

---

[← 仓库根 `README.md`](../README.md) · [`DESIGN.md`](../DESIGN.md)
