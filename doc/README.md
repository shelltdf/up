# `doc/` 文档索引

本目录为 **实现侧补充文档**（与 `ai-software-engineering/` 四阶段文档互补）。正文与仓库源码一致采用 **UTF-8 带 BOM**（**`*.py` shebang 脚本**除外，见 **`ai-software-engineering/03-ops/developer-manual.md`**）。**产品方向**：**纯手写** **`package.xml` / `target.xml`**（见 **[zh/package-target-xml-spec.md](zh/package-target-xml-spec.md)** 文首、**`gz spec`**）。

**从源码构建 `gz`**：根目录 **`README.md`** 与 **[zh/user-manual.md](zh/user-manual.md)** 中「**0. 10 分钟快速上手**」等处若出现 **`.\_build\Release\gz.exe`** 等路径，**`_build`** 仅为 **`cmake -B`** 的示例目录名（亦可是 **`_build_gz`** 等），须与本机实际构建目录一致；细则见用户手册该节中的 **构建目录** 说明。**[zh/getting-started.md](zh/getting-started.md)** §0 亦有交叉引用。**子命令、开关、退出码与范例** 的完整清单见 **[zh/cli-reference.md](zh/cli-reference.md)**。

**`doc/zh/` 与 `doc/en/` 文件名一一对应**：英文侧除 **`en/user-manual.md`** 为完整稿外，其余 **`en/*.md`** 当前为**入口页**（链到中文正文或 `gz spec`），便于双语目录对齐与后续补译。

| 主题 | 中文正文 `zh/` | 英文 `en/` |
|------|------------------|------------|
| 用户手册 | [zh/user-manual.md](zh/user-manual.md) | [en/user-manual.md](en/user-manual.md)（完整英文） |
| **CLI 参数与工作模式** | [zh/cli-reference.md](zh/cli-reference.md) | [en/cli-reference.md](en/cli-reference.md)（入口 → `zh`） |
| 分步入门 | [zh/getting-started.md](zh/getting-started.md) | [en/getting-started.md](en/getting-started.md)（入口 → `zh`） |
| XML 规范 | [zh/package-target-xml-spec.md](zh/package-target-xml-spec.md) | [en/package-target-xml-spec.md](en/package-target-xml-spec.md)（入口 → `zh` / `gz spec`） |
| 内置变量与缓存键 | [zh/internal-variables.md](zh/internal-variables.md) | [en/internal-variables.md](en/internal-variables.md)（入口 → `zh`） |
| `trigger` 消息表 | [zh/script-messages.md](zh/script-messages.md) | [en/script-messages.md](en/script-messages.md)（入口 → `zh`） |
| 脚本与预处理教程 | [zh/script-tutorial.md](zh/script-tutorial.md) | [en/script-tutorial.md](en/script-tutorial.md)（入口 → `zh`） |

正文路径均在 **`zh/`** / **`en/`** 上表；仓库根 **`README.md`**、**`DESIGN.md`** 等已直接链到 **`doc/zh/...`**（用户手册另链 **`doc/en/user-manual.md`**）。**不再**在 `doc/` 根目录保留同名跳转 `.md`，避免与 **`doc/README.md`** 重复维护。

仓库总览与命令表见根目录 **`README.md`**；设计与中间目录见 **`DESIGN.md`**。

---

[← 仓库根 `README.md`](../README.md) · [`DESIGN.md`](../DESIGN.md)
