# 软件设计：形态与边界

## 交付物

| 产物 | 说明 |
|------|------|
| `gz`（`gz.exe`） | C++17 CLI；MSVC 下静态 CRT、`/utf-8` 与仓库根 `CMakeLists.txt` 一致。 |
| `gz-gui` | 薄壳：仅使用 `GroundZeroGUI/` 下源码，通过进程调用同目录或可解析路径下的 `gz`；各平台 UI（Win32 / GTK3 / Cocoa）。 |

## 源码布局（概念映射）

- `GroundZero/exe/`：入口、全局参数（如 `--verbose`）、子命令分发。
- `GroundZero/lib/engine/commands/`：各子命令实现（configure、build、run、test、pack、spec、list 等）。
- `GroundZero/lib/engine/backends/`：CMake / Ninja / CTest / 归档等后端适配。
- `GroundZero/lib/infra/`：路径、i18n、控制台 UTF-8 等横切能力。
- `GroundZeroGUI/`：GUI 与设置持久化，调用 CLI。

## 可选编译能力

## 中间产物目录（cwd 相对）

固定使用 **`.intermediate/`**，其下：

- **`build/<leaf>/`**：`configure` 写入生成文件与 **`gz_cache.txt`**；`<leaf>` 由 `--build-dir-name` 指定，省略时为 `default`。**不等于**安装目录下的 `<arch>` 名。
- **`install/<arch>/`**：安装前缀；`<arch>` 为组合配置标签，写入 `gz_cache.txt` 的 `arch=` 字段。
- **`pack/<arch>/`**：打包输出。

## GUI 设计要点（概念层）

编译环境设置（本地 / Android / emsdk）、CWD、扫描目录、构建目录叶子名、安装目录名、`--opt` 列表等；细节见 `01-logic/detailed-design-cli.md` 与 `02-physical/gz-gui/spec.md`。
