# 用户说明书（运维摘要）

## 环境前提

- CMake 3.20+、C++17 工具链；Windows 推荐 VS 2022 + MSVC。
- 将仓库构建产物 `gz` / `gz-gui` 加入 `PATH` 或使用显式路径调用。

## CLI：易错参数（与实现对齐）

1. **`gz build`** 必须带 **`--build-dir-name <leaf>`**（与此前 `configure` 使用的叶子名一致，默认 **`default`**）。
2. **`gz run` / `gz test` / `gz pack`** 必须带 **`--install-dir-name <name>`**，其中 **`<name>` 是 `.intermediate/install/` 下的子目录名**，通常等于 **`gz_cache.txt` 里的 `arch=`**，而不是构建叶子名。
3. 获取 `<name>`：在相同 **cwd** 下执行 **`gz print-build-dir-name`**（必要时传与 configure 一致的 `--build-dir-name` / `--opt`）。

下文 **`.\_build\Release\`** 中的 **`_build`** 表示 **`cmake -B`** 所选构建目录（示例名；亦可是 **`_build_gz`** 等），请按本机实际路径替换。

## 一键示例（PowerShell）

完整可复制片段与更多组合见 **`doc/zh/cli-reference.md`** 与 **`doc/zh/getting-started.md`**；运维摘要仅保留最短链：

```powershell
.\_build\Release\gz.exe configure --scan test_projects
$ARCH = .\_build\Release\gz.exe print-build-dir-name
.\_build\Release\gz.exe build --build-dir-name default
.\_build\Release\gz.exe test --install-dir-name $ARCH
.\_build\Release\gz.exe run --install-dir-name $ARCH hello_demo
```

## 延伸阅读

- **`gz` 命令行逐项说明与范例**：`doc/zh/cli-reference.md` 与 **`doc/en/cli-reference.md`**（均为完整正文，任选语言）。
- 分步入门与 XML 范例：`doc/zh/getting-started.md`。
- 图文版手册（分工、FAQ、gz-gui）：`doc/zh/user-manual.md`（中文）、`doc/en/user-manual.md`（英文）；索引 **`doc/README.md`**。
- 设计背景：`DESIGN.md`。
