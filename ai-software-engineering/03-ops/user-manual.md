# 用户说明书（运维摘要）

## 环境前提

- CMake 3.20+、C++17 工具链；Windows 推荐 VS 2022 + MSVC。
- 将仓库构建产物 `up` / `up-gui` 加入 `PATH` 或使用显式路径调用。

## CLI：易错参数（与实现对齐）

1. **`up build`** 必须带 **`--build-dir-name <leaf>`**（与此前 `configure` 使用的叶子名一致，默认 **`default`**）。
2. **`up run` / `up test` / `up pack`** 必须带 **`--install-dir-name <name>`**，其中 **`<name>` 是 `.intermediate/install/` 下的子目录名**，通常等于 **`up_cache.txt` 里的 `arch=`**，而不是构建叶子名。
3. 获取 `<name>`：在相同 **cwd** 下执行 **`up print-build-dir-name`**（必要时传与 configure 一致的 `--build-dir-name` / `--opt`）。

## 一键示例（PowerShell）

```powershell
.\_build\Release\up.exe configure --scan test_projects
$ARCH = .\_build\Release\up.exe print-build-dir-name
.\_build\Release\up.exe build --build-dir-name default
.\_build\Release\up.exe test --install-dir-name $ARCH
.\_build\Release\up.exe run --install-dir-name $ARCH hello_demo
```

## 延伸阅读

- 图文版手册：**正文** `doc/zh/user-manual.md`（中文）、`doc/en/user-manual.md`（英文）；索引 **`doc/README.md`**。
- 设计背景：`DESIGN.md`。
