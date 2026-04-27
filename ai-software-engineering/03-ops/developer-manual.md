# 开发维护说明书（摘要）

## 构建宿主工具

```powershell
cmake -S . -B _build -G "Visual Studio 17 2022" -A x64
cmake --build _build --config Release
```

`-B` 也可选用 **`_build_gz`** 等任意根目录名；仓库 **`.gitignore`** 已忽略 **`/_build*/`** 与 **`_build/`**，避免将 CMake 生成树误提交。

## 安装与打包

- `python install.py --prefix dist`：仅安装 **`gz_runtime`** 分量（`gz` + `gz-gui`）。
- `python package.py`：依赖 `install.py` 产物打 zip / tar.gz。

## 仓库脚本边界

- `build.py` / `install.py` **只**作用于仓库根 CMake 工程，**不**编译 `test_projects/`。
- 示例包在各自目录下用已安装的 `gz` 执行 `configure` / `build` / `test` 等。
- **`test_projects/hello_library_type/`**：演示 **`target.xml`** **`type="library"`** 与 **`GZ_TARGET_DYNAMIC_LIBRARY`**；索引见 **`test_projects/README.md`**。

## 源码与文档编码

- **UTF-8 带 BOM（EF BB BF）**：C/C++、CMake、**`ai-software-engineering/`** 下 Markdown、**`doc/`**、**`.cursor/rules/*.mdc`** 等文本，统一为 **UTF-8 BOM**（与 **`.editorconfig`** 中 `charset = utf-8-bom` 一致），减少 Windows 下「被误判 ANSI」导致的乱码。
- **例外**：以 **`#!/`** 开头的 **`*.py` / `*.sh`** 为 **UTF-8 无 BOM**（BOM 在首字节时会导致 Unix 无法识别 shebang）；请用 **`python build.py`** 等方式调用，或勿在首行之前插入任何字节。
- **批量维护**：仓库根执行 **`python tools/normalize_utf8_bom.py`**（跳过 **`.intermediate/`**、**`3rdparty/`**、**`_build*/`** 等；含 shebang 的文件自动跳过写 BOM）。

## 文档维护

- 规则驱动四阶段目录：**`ai-software-engineering/`**（本仓库已建立）。
- 实现侧用户文档：**正文**在 **`doc/zh/`**（中文）；**`doc/en/`** 与 **`doc/zh/`** **同名文件一一对应**：其中 **`doc/en/user-manual.md`** 为完整英文稿，其余 **`doc/en/*.md`** 当前为**入口页**（指向 **`../zh/…`** 正文或 **`gz spec`**）。**索引与全部链出**：**`doc/README.md`**（**不**再在 `doc/` 根目录维护与 `zh/` 同名的跳转 `.md`）。
- 总览：`README.md`、`DESIGN.md`。
