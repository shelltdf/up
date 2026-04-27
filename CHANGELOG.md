# Changelog

## 2026-04-28

### Changed

- **实现侧文档**：用户可读正文统一在 **`doc/zh/`**（中文）与 **`doc/en/`**（英文入口/全文）；**`doc/README.md`** 为双语索引。已移除 `doc/` 根目录下与 `zh/` 重复的同名跳转 `.md`，根 **`README.md`** / **`DESIGN.md`** 等改为直接链接 **`doc/zh/...`**（用户手册另链 **`doc/en/user-manual.md`**）。
- 各 **`doc/zh/*.md`**、**`doc/en/*.md`** 文首增加指向 **`doc/README.md`** 的索引导航（含脚本教程、`trigger` 表、内置变量、XML 规范等）。

## 2026-04-25

### Changed

- Refactored source layout into dedicated directories:
  - `src/exe/` for `up.exe` entry and command dispatch
  - `src/lib/` for `up.lib` implementation (`engine/` and `infra`)
- Updated `CMakeLists.txt` source lists and include directories to `src/lib/...`.
- Synchronized docs/specs/mappings/UML to the new physical paths and layering constraints.

### Fixed

- Corrected CMake project-source append list to use `UP_LIB_SOURCES` (instead of stale `UP_CORE_SOURCES`) when `UP_ENABLE_REVERSE=ON`.

### Verified

- Build outputs remain consistent and pass verification:
  - `up.exe`
  - `up.lib`
  - `up-gui.exe`
