# 详细设计：CLI 子命令与路径语义

## 构建目录叶子 `<leaf>`

- 由 **`configure --build-dir-name <leaf>`** 指定；若省略则为 **`default`**。
- 物理路径：**`<cwd>/.intermediate/build/<leaf>/`**。
- **`build` 必填**：`up build --build-dir-name <leaf>`，且该目录下须已有 **`up_cache.txt`**（先成功 `configure`）。

## 安装目录名 `<installLeaf>`

- **`run` / `test` / `pack` 使用**：`--install-dir-name <installLeaf>`。
- **语义**：`<cwd>/.intermediate/install/<installLeaf>/`。
- **与 `<leaf>` 的关系**：通常 **`<installLeaf>` 应等于 `up_cache.txt` 中的 `arch` 字段**，而不是构建叶子名 `default`。`build` 实现将安装前缀设为 `default_install_root(cwd) / arch`（见 `src/lib/engine/commands/build.cpp`）。
- **获取方式**：在同一 cwd 下执行 **`up print-build-dir-name`**（默认读取 `.intermediate/build/default/up_cache.txt`，亦可通过 `--build-dir-name` / `--opt` 与 configure 对齐）。

## 典型脚本序列（PowerShell）

```powershell
.\path\to\up.exe configure --scan test_projects
$ARCH = .\path\to\up.exe print-build-dir-name
.\path\to\up.exe build --build-dir-name default
.\path\to\up.exe test --install-dir-name $ARCH
.\path\to\up.exe run --install-dir-name $ARCH hello_demo
```

## `spec` 子命令

向标准输出打印内嵌的规范文本，便于无仓库 `doc/` 副本时由工具消费；不改变工作区文件。

## `project` 子命令

- 预处理/探测外部工程并生成 `package.xml` 与 `.targets/` 等；实现位于 `src/lib/engine/project/*`（条件编译）。
- **若二进制未带 `UP_ENABLE_PROJECT=1`**：`up project` 固定失败并提示使用 `-DUP_ENABLE_PROJECT=ON` 重编译。
