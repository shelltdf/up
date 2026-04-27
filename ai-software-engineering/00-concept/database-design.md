# 数据与存储设计（广义）

本工具不依赖服务端数据库；**状态以文件为主**。

## `up_cache.txt`

- **位置**：`.intermediate/build/<leaf>/up_cache.txt`（`<leaf>` 与 `configure --build-dir-name` 对应，默认 `default`）。
- **用途**：记录 `cwd`、`arch`、主包名、生成文件路径、`scan_roots` 以及以 `UP_` / `UPSTREAM_` 前缀为主的选项键值，供 `build` / `print-build-dir-name` 等读取。
- **版本行**：`up.cache.version=1`（实现写入，用于未来演进）。

## GUI 设置

- **文件**：`up_gui_settings.txt`（由 `up-gui` 维护，具体路径与字段以实现为准）。
- **用途**：持久化编译环境与路径类选项；在发起 `configure` 时转为 `--opt` 传给 `up`。

## 其它

包描述 **`package.xml` / `target.xml`** 为源码树内的人类可编辑数据，不归档为「数据库表」，字段级约定见 **`doc/zh/package-target-xml-spec.md`** 与 `02-physical` 各目标规格中的引用（索引导航见 **`doc/README.md`**）。
