# 开发维护说明书（摘要）

## 构建宿主工具

```powershell
cmake -S . -B _build -G "Visual Studio 17 2022" -A x64
cmake --build _build --config Release
```

`-B` 也可选用 **`_build_gz`** 等任意根目录名；仓库 **`.gitignore`** 已忽略 **`/_build*/`** 与 **`_build/`**，避免将 CMake 生成树误提交。

### `gz_reverse_cmake`（根工程内子目录）

- **常规**：与 **`gz` / `gz-gui` 同一次根 CMake 配置**（上节命令），`build.py` 会显式构建三者；**`install.py` 默认的 `gz_runtime`** 在 **`bin/`** 中同时安装 **`gz_reverse_cmake`**（与主程序同发，**选项 B**）。

- **单独调试子目录**（仅维护该工具时，可选）：

```powershell
cmake -S gz_reverse_cmake -B gz_reverse_cmake/build -G Ninja
cmake --build gz_reverse_cmake/build
```

实现为**纯 C++** 解析 `CMakeLists.txt` 子集，**不**拉取第三方库；**不**在工具内执行本机 `cmake`；可选 **`--file-api <path>`** 读入用户预置的 `codemodel` 类 JSON 作 target 名对照（L7，见 `spec.md`）。工程说明见 **`ai-software-engineering/02-physical/gz-reverse-cmake/`**。

## 安装与打包

- `python install.py --prefix dist`：安装 **`gz_runtime`** 分量，包含 **`bin/`** 下 **`gz`**、**`gz-gui`**、**`gz_reverse_cmake`** 与 **`gz_gui.png`**；加 **`--with-dev`** 时额外装 **`gz_dev`**（`gz.lib` 等）。
- `python setup.py -i`：先调用 **`install.py`** 到 **`dist/`**，再把 **`bin/`** 中上述 **`gz_runtime`** 四者复制到本机可执行物目录（Windows 为 `python.exe` 同目录，其它为 **`~/.local/bin`**）；**`setup.py -u`** 会删除同名的这四个文件（若存在）。
- `python package.py`：依赖 `install.py` 产物打 zip / tar.gz。

## 仓库脚本边界

- `build.py` / `install.py` **只**作用于仓库根 CMake 工程，**不**编译 `test_projects/`。
- 示例包在各自目录下用已安装的 `gz` 执行 `configure` / `build` / `test` 等。
- **`test_projects/hello_library_type/`**：演示 **`target.xml`** **`type="library"`** 与 **`GZ_TARGET_DYNAMIC_LIBRARY`**；索引见 **`test_projects/README.md`**。
- **`target.xml` `type=`** 须在实现白名单内；否则 **`gz configure`** 报 **`unknown target type`** 并 **退出码 5**（权威字段表见 **`doc/zh/package-target-xml-spec.md` §3.1**）。

## 源码与文档编码

- **UTF-8 带 BOM（EF BB BF）**：C/C++、CMake、**`ai-software-engineering/`** 下 Markdown、**`doc/`**、**`.cursor/rules/*.mdc`** 等文本，统一为 **UTF-8 BOM**（与 **`.editorconfig`** 中 `charset = utf-8-bom` 一致），减少 Windows 下「被误判 ANSI」导致的乱码。
- **例外**：以 **`#!/`** 开头的 **`*.py` / `*.sh`** 为 **UTF-8 无 BOM**（BOM 在首字节时会导致 Unix 无法识别 shebang）；请用 **`python build.py`** 等方式调用，或勿在首行之前插入任何字节。
- **批量维护**：仓库根执行 **`python tools/normalize_utf8_bom.py`**（跳过 **`.intermediate/`**、**`3rdparty/`**、**`_build*/`** 等；含 shebang 的文件自动跳过写 BOM）。

## 文档维护

- 规则驱动四阶段目录：**`ai-software-engineering/`**（本仓库已建立）。
- 实现侧用户文档：**`doc/zh/`** 与 **`doc/en/`** 同名文件一一对应，**均为完整正文**（中/英各一份；与 **`gz spec`** 或源码冲突时以 **`gz spec` 与源码** 为准）。**索引**：**`doc/README.md`**（**不**再在 `doc/` 根目录维护与 `zh/` 同名的跳转 `.md`）。**`gz spec`** 内嵌英文正文的版本号见 **`GZ_XML_SPEC_REVISION`**（`GroundZero/lib/engine/commands/spec.cpp`），正文按 DOM / 变量 / 脚本 三条主线组织，与 **`doc/*/package-target-xml-spec.md`** 中「与内嵌 / Alignment」表交叉引用。
- 总览：`README.md`、`DESIGN.md`。
