# 开发维护说明书（摘要）

## 构建宿主工具

```powershell
cmake -S . -B _build -G "Visual Studio 17 2022" -A x64
cmake --build _build --config Release
```

## 安装与打包

- `python install.py --prefix dist`：仅安装 **`up_runtime`** 分量（`up` + `up-gui`）。
- `python package.py`：依赖 `install.py` 产物打 zip / tar.gz。

## 仓库脚本边界

- `build.py` / `install.py` **只**作用于仓库根 CMake 工程，**不**编译 `test_projects/`。
- 示例包在各自目录下用已安装的 `up` 执行 `configure` / `build` / `test` 等。

## 文档维护

- 规则驱动四阶段目录：**`ai-software-engineering/`**（本仓库已建立）。
- 实现侧用户文档：**正文**在 **`doc/zh/`**（中文）；**`doc/en/`** 与 **`doc/zh/`** **同名文件一一对应**：其中 **`doc/en/user-manual.md`** 为完整英文稿，其余 **`doc/en/*.md`** 当前为**入口页**（指向 **`../zh/…`** 正文或 **`up spec`**）。**索引与全部链出**：**`doc/README.md`**（**不**再在 `doc/` 根目录维护与 `zh/` 同名的跳转 `.md`）。
- 总览：`README.md`、`DESIGN.md`。
