# up-cli：模型/行为 → 源码映射（摘录）

| 概念 | 路径 |
|------|------|
| 入口 / argv 分发 | `src/cli/main.cpp` |
| 用法与 i18n 文案 | `src/infra/i18n/lang.cpp` |
| 全局 verbose | `src/engine/commands/cli_verbose.hpp`、`commands_common.cpp` |
| 默认中间目录 | `src/infra/platform/paths.cpp`、`paths.hpp` |
| 路径 ASCII 校验 | `src/infra/platform/path_check.cpp` |
| configure 缓存写入 | `src/engine/commands/configure.cpp`（`write_up_cache`） |
| 后端选择 | `src/engine/backends/core/backend_dispatch.cpp` |
