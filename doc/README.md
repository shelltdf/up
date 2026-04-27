# `doc/` 文档索引

本目录为 **实现侧补充文档**（与 `ai-software-engineering/` 四阶段文档互补）。**产品方向**：**纯手写** **`package.xml` / `target.xml`**（见 **[zh/package-target-xml-spec.md](zh/package-target-xml-spec.md)** 文首、**`up spec`**）。

**`doc/zh/` 与 `doc/en/` 文件名一一对应**：英文侧除 **`en/user-manual.md`** 为完整稿外，其余 **`en/*.md`** 当前为**入口页**（链到中文正文或 `up spec`），便于双语目录对齐与后续补译。

| 主题 | 中文正文 `zh/` | 英文 `en/` |
|------|------------------|------------|
| 用户手册 | [zh/user-manual.md](zh/user-manual.md) | [en/user-manual.md](en/user-manual.md)（完整英文） |
| 分步入门 | [zh/getting-started.md](zh/getting-started.md) | [en/getting-started.md](en/getting-started.md)（入口 → `zh`） |
| XML 规范 | [zh/package-target-xml-spec.md](zh/package-target-xml-spec.md) | [en/package-target-xml-spec.md](en/package-target-xml-spec.md)（入口 → `zh` / `up spec`） |
| 内置变量与缓存键 | [zh/internal-variables.md](zh/internal-variables.md) | [en/internal-variables.md](en/internal-variables.md)（入口 → `zh`） |
| `trigger` 消息表 | [zh/script-messages.md](zh/script-messages.md) | [en/script-messages.md](en/script-messages.md)（入口 → `zh`） |
| 脚本与预处理教程 | [zh/script-tutorial.md](zh/script-tutorial.md) | [en/script-tutorial.md](en/script-tutorial.md)（入口 → `zh`） |

正文路径均在 **`zh/`** / **`en/`** 上表；仓库根 **`README.md`**、**`DESIGN.md`** 等已直接链到 **`doc/zh/...`**（用户手册另链 **`doc/en/user-manual.md`**）。**不再**在 `doc/` 根目录保留同名跳转 `.md`，避免与 **`doc/README.md`** 重复维护。

仓库总览与命令表见根目录 **`README.md`**；设计与中间目录见 **`DESIGN.md`**。

---

[← 仓库根 `README.md`](../README.md) · [`DESIGN.md`](../DESIGN.md)
