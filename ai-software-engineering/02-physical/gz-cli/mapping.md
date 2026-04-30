# gz-cli：模型/行为 → 源码映射（摘录）

## 子命令 → 主实现源文件

| 子命令 | 主实现（业务逻辑；argv 解析多在 `cli_dispatch.cpp`） |
|--------|------|
| `configure` | `GroundZero/lib/engine/commands/configure.cpp` |
| `build` | `GroundZero/lib/engine/commands/build.cpp`；二次分发 XML 落盘见 `redist_emit.cpp`（由 `configure` 写 `gz_redist_manifest.json`） |
| `run` | `GroundZero/lib/engine/commands/run.cpp` |
| `test` | `GroundZero/lib/engine/commands/test.cpp` |
| `pack` | `GroundZero/lib/engine/commands/pack.cpp` |
| `spec` | `GroundZero/lib/engine/commands/spec.cpp` |
| `list` | `GroundZero/lib/engine/commands/list.cpp`、`list.hpp` |
| `print-build-dir-name` | `GroundZero/exe/cli_dispatch.cpp`（与 `main.cpp` 入口同链路） |

## 其它映射

| 概念 | 路径 |
|------|------|
| 入口 / argv 分发 | `GroundZero/exe/main.cpp`、`cli_dispatch.cpp` |
| 用法与 i18n 文案 | `GroundZero/lib/infra/i18n/lang.cpp` |
| 全局 verbose | `GroundZero/lib/engine/commands/cli_verbose.hpp`、`commands_common.cpp` |
| 默认中间目录 | `GroundZero/lib/infra/platform/paths.cpp`、`paths.hpp` |
| 路径 ASCII 校验 | `GroundZero/lib/infra/platform/path_check.cpp` |
| configure 缓存写入 | `GroundZero/lib/engine/commands/configure.cpp`（`write_gz_cache`） |
| DOM 模型 / script 上下文 | `GroundZero/lib/engine/dom/dom_model.hpp`、`GroundZero/lib/engine/dom/dom_model.cpp` |
| 后端选择 | `GroundZero/lib/engine/backends/core/backend_dispatch.cpp` |
| 内嵌 Lua 5.5 静态库 | 根 `3rdparty/lua-5.5.0/CMakeLists.txt` 目标 **`lz_embed`**；`target_link_libraries(gz-lib PRIVATE lz_embed)` 见 `GroundZero/lib/CMakeLists.txt` |
| `trigger=configure` 脚本、**`gz.file`** | `GroundZero/lib/engine/lua/gz_embedded_lua.cpp`、**`gz_embedded_lua.hpp`**；调度 **`run_dom_embedded_configure_lua`** 在 **`configure.cpp`**（写后端之前；DOM 含嵌套 `Package`） |
| 合法 **`configure` trigger** | `GroundZero/lib/engine/xml/simple_xml.cpp` 中 **`is_supported_script_trigger`**；内嵌 spec §4 表 |
