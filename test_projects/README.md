# test_projects

本目录下**每一个子目录**都是一个独立的测试包：根目录放 **`package.xml`**；**每个 CMake 目标独占一个子目录**，该子目录内**恰好一个** **`target.xml`**（不得在同一目录堆叠多个 target 描述文件）。`<sources>` 中的路径相对于该 `target.xml` 所在目录。`configure` 会为整包生成 `add_library` / `add_executable` 并将同包下的库链接到各可执行目标。

**与 `build.py` / `install.py` 的关系**：仓库根的 `build.py`、`install.py` **只**构建并安装宿主工具 **`up.exe`** 与 **`up-gui.exe`**，**不包含、不编译、不安装** `test_projects/` 下的任何测试包。测试包由你在选好目录后运行 **`up configure` / `up build`** 等命令驱动；产物在当时的 **cwd** 下 **`.intermediate/`** 中生成，与本仓库根 `_build` 无关。

| 子目录 | 说明 |
|--------|------|
| [hello_demo/](hello_demo/) | 多子目录、`target.xml` 与 `foo` 库及单元测试示例 |

在仓库根目录执行（已构建 `up.exe`）：

```powershell
.\_build\Release\up.exe configure --scan test_projects
.\_build\Release\up.exe build
.\_build\Release\up.exe test
.\_build\Release\up.exe run hello_demo
```

`hello_demo` 包内 **`hello_demo_test/`** 子目录含测试可执行体的 **`target.xml`**；`up test` / CTest 会运行 **`hello_demo_test`** 与主程序 **`hello_demo`** 两条用例。

或在某一子包目录内将 `up` 加入 `PATH` 后，直接 `up configure`（默认扫描当前目录，仅包含该包）。
