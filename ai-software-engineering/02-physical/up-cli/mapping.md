# up-cli：模型/行为 → 源码映射（摘录）

| 概念 | 路径 |
|------|------|
| 入口 / argv 分发 | `src/exe/main.cpp` |
| 用法与 i18n 文案 | `src/lib/infra/i18n/lang.cpp` |
| 全局 verbose | `src/lib/engine/commands/cli_verbose.hpp`、`commands_common.cpp` |
| 默认中间目录 | `src/lib/infra/platform/paths.cpp`、`paths.hpp` |
| 路径 ASCII 校验 | `src/lib/infra/platform/path_check.cpp` |
| configure 缓存写入 | `src/lib/engine/commands/configure.cpp`（`write_up_cache`） |
| DOM 模型 / script 上下文 | `src/lib/engine/dom/dom_model.hpp`、`src/lib/engine/dom/dom_model.cpp` |
| list 命令 | `src/lib/engine/commands/list.cpp`、`src/lib/engine/commands/list.hpp` |
| 后端选择 | `src/lib/engine/backends/core/backend_dispatch.cpp` |
