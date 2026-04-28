# Changelog

## 2026-04-28

### Breaking（品牌重命名为 GroundZero / gz）

- **CMake**：`project(GroundZero)`；宿主目标 **`gz`**、**`gz-gui`**、静态库 **`gz-lib`**（`gz.lib`）；安装分量 **`gz_runtime`** / **`gz_dev`**。
- **源码目录**：`GroundZero/`（`lib/` + `exe/`）、`GroundZeroGUI/`。
- **XML / 缓存**：内置键前缀 **`GZ_*`**（替换原 **`UP_*`**）；构建缓存文件名 **`gz_cache.txt`**（元数据键 **`gz.cache.version`**）；GUI 持久化 **`gz_gui_settings.txt`**。旧工作区中的 **`up_cache.txt`** / **`UP_*`** 不会被读取；请删除相关 **`.intermediate`** 或重新执行 **`gz configure`**。
- **CLI 子命令**：入口程序名为 **`gz`**（Windows：`gz.exe`）。
- **DOM 导出 XML**：根元素由 **`<up_dom>`** 改为 **`<gz_dom>`**（`gz list --xml` 等输出）；依赖旧根标签的外部脚本需更新。

### Changed

- **`target.xml` 磁盘预置库类型重命名**：**`imported_static_library` / `imported_shared_library`** 改为 **`prebuilt_static_library` / `prebuilt_shared_library`**（与 **`<prebuilt …/>`** 语义一致）；读入时仍接受旧 **`type`** 并规范化为新名。文档、**`gz spec`**、**`test_projects/prebuilt_*`** 已同步。
- **`target.xml` `type="library"`**：configure 时按 **`GZ_TARGET_DYNAMIC_LIBRARY`**（及 **`GZ_DYNAMIC_LIBRARY`**）解析为 **`static_library`** 或 **`shared_library`** 再生成 CMake/Ninja；**`static_library` / `shared_library`** 仍强制固定链接形态。详见 **`doc/zh/package-target-xml-spec.md`**、**`doc/zh/internal-variables.md`**、**`doc/zh/cli-reference.md`**（**`configure`** 节）、**`doc/zh/user-manual.md`**、**`doc/zh/getting-started.md`**、**`doc/zh/script-tutorial.md`**、**`doc/en/user-manual.md`** 与 **`gz spec`**；示例包 **`test_projects/hello_library_type/`**。
- **`gz reverse`**：子命令及 **`GroundZero/lib/engine/reverse/`**、**`GZ_ENABLE_REVERSE`** 构建开关**已删除**；CLI / **gz-gui** 不再暴露逆向入口。工程迁移请手写 **`package.xml` / `target.xml`**。
- **旧版包级内嵌上游 CMake 与 ExternalProject 聚合路径**：已从解析与 CMake 生成中**完全移除**；**`imported_installed_*`** 仅依赖包外安装与手写 **`target.xml`** 声明。
- **实现侧文档**：用户可读正文统一在 **`doc/zh/`**（中文）与 **`doc/en/`**（英文入口/全文）；**`doc/README.md`** 为双语索引。已移除 `doc/` 根目录下与 `zh/` 重复的同名跳转 `.md`，根 **`README.md`** / **`DESIGN.md`** 等改为直接链接 **`doc/zh/...`**（用户手册另链 **`doc/en/user-manual.md`**）。
- 各 **`doc/zh/*.md`**、**`doc/en/*.md`** 文首增加指向 **`doc/README.md`** 的索引导航（含脚本教程、`trigger` 表、内置变量、XML 规范等）。
- 根 **`README.md`**：标题与命令表、目录树、`package.py`/`install.py` 说明与 **`gz list`** 示例全部对齐 **GroundZero / gz**（去除残留的 `uni-package` / `up` 子命令表述）。
- **`mindmap.mmd`**、**`DESIGN.md`**：导图与索引中的 **`up_cache`** 等字样改为 **`gz_cache`** / **`gz`**。
- **`dev_run_gui.py`**：开发入口改为运行 **`dist/bin/gz-gui.exe`**。
- **gz-gui 内部 API（fork 定制者）**：`gz::gui::shell` 将 **`run_up_command_in_detached_thread`** 重命名为 **`run_gz_command_in_detached_thread`**；`build_configure_args_line` / `try_acquire_run_context` / **`persist::query_print_build_dir_name`** 的 CLI 路径参数统一为 **`gz_exe`**；GTK/Cocoa 状态字段 **`gz_exe`**；macOS **`UpGuiCtrl` → `GzGuiCtrl`**。
- **`.gitignore`**：增加仓库根 **`/_build*/`**（覆盖 `_build_gz`、`_build_agent_rev` 等 CMake 输出目录）；移除已无代码引用的 **`/.up`**。
- **文档**：新增 **`doc/zh/cli-reference.md`**（`gz` 命令行参数、工作模式、退出码与范例）；**`doc/en/cli-reference.md`** 为入口；**`doc/README.md`**、根 **`README.md`**、**`doc/zh/user-manual.md`**、**`doc/zh/getting-started.md`**、**`ai-software-engineering/03-ops/user-manual.md`**、**`ai-software-engineering/02-physical/gz-cli/README.md`** 增加交叉链接。
- **文本编码**：仓库内 **C/C++/CMake/文档/配置等** 已统一为 **UTF-8 带 BOM**（便于 Windows/跨工具识别）；**`#!/...` 脚本**（根目录 **`*.py`** 等）保持 **UTF-8 无 BOM**（BOM 会破坏 Unix shebang）。维护脚本：**`tools/normalize_utf8_bom.py`**（跳过 **`.intermediate/`**、**`3rdparty/`**、**`_build*/`**）；**`.editorconfig`** 已区分 **`*.py` / `*.sh`** 与 **`utf-8-bom`** 规则。
- **文档分工**：**`ai-software-engineering/02-physical/gz-cli/spec.md`** 改为只保留 **物理层**规格与退出码摘要，**argv/子命令详表** 指向 **`doc/zh/cli-reference.md`**；子命令→源码表迁至 **`gz-cli/mapping.md`**。
- **`doc/zh/user-manual.md` / `doc/en/user-manual.md`**：改为**枢纽**（文档分工表、鸟瞰、FAQ、gz-gui），去掉与 **`cli-reference` / `getting-started` / `package-target-xml-spec`** 重复的长命令与 XML 字段段；**`CMAKE_PREFIX_PATH` 聚合**详述迁至 **`doc/zh/internal-variables.md`** §4.1。根 **`README.md`** 推荐阅读顺序改为 **getting-started → cli-reference → user-manual**；**`package-target-xml-spec.md`** 文首增加分工表；**`getting-started` / `script-tutorial` / `cli-reference` / `03-ops/user-manual`** 等交叉引用已对齐。
- **`doc/en/*.md`**：明确 **`doc/en/`** 下各篇均为**英文完整正文**（与 **`doc/zh/`** 同名文件一一对应）；**`doc/README.md`**、根 **`README.md`**、**`doc/en/user-manual.md`** 及 **`cli-reference` / `internal-variables` / `script-*` / `getting-started` / `package-target-xml-spec`** 英文页首已标注「全文 + 可选中文镜像」；**`en/user-manual`** 内链接改为优先指向 **`doc/en/`** 同目录专题页。
- **`doc/en/*.md`**：`cli-reference`、`getting-started`、`package-target-xml-spec`、`internal-variables`、`script-messages`、`script-tutorial` 由入口 stub **改为完整英文版**（与 **`doc/zh/`** 对应文件职责对齐）；**`doc/README.md`**、**`03-ops/developer-manual.md`** 已更新表述。

## 2026-04-25

### Changed

- Refactored source layout into dedicated directories:
  - `GroundZero/exe/` for `gz.exe` entry and command dispatch
  - `GroundZero/lib/` for `gz.lib` implementation (`engine/` and `infra`)
- Updated `CMakeLists.txt` source lists and include directories to `GroundZero/lib/...`.
- Synchronized docs/specs/mappings/UML to the new physical paths and layering constraints.

### Fixed

- Corrected CMake project-source append list to use `GZ_LIB_SOURCES` (instead of stale `UP_CORE_SOURCES`).

### Verified

- Build outputs remain consistent and pass verification:
  - `gz.exe`
  - `gz.lib`
  - `gz-gui.exe`
