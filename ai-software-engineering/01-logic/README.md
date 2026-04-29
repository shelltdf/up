# 逻辑阶段（01-logic）

| 文档 | 职责 |
|------|------|
| [system-design.md](system-design.md) | 子系统划分、configure→build→install 数据流 |
| [detailed-design-cli.md](detailed-design-cli.md) | CLI 子命令参数、目录叶子名与 `arch` 的关系 |
| [detailed-design-gz-reverse-cmake.md](detailed-design-gz-reverse-cmake.md) | 独立工具 `gz_reverse_cmake`：L1–L6 静态解析/解释子集（不子进程 `cmake`）；L7 可选 `--file-api` 摄入预置 JSON 对照；→ `package.xml` / `target.xml` 初稿 |

物理层字段级规格与源码映射见 `02-physical/`。
