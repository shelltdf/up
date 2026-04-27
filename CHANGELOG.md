# Changelog

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
