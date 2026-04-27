# GroundZero / gz — core library

This directory contains the C++17 engine used by the `gz` CLI (`GroundZero/exe/`).

- **XML / DOM / backends / i18n** live under `engine/` and `infra/`.
- **Public include roots** for the static library target are wired in `CMakeLists.txt` (see `target_include_directories`).

`GroundZero/exe/` contains CLI entrypoint sources for `gz.exe`.
