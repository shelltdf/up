# 产品设计：GroundZero（gz）

## 愿景

为 C/C++ 及多生态工程提供一种**以声明式数据（`package.xml` / `target.xml`）驱动**的包扫描、依赖建图、后端生成（CMake / Ninja 等）、构建、测试、运行与打包流程；降低脚本碎片化与后端切换成本。

## 目标用户

- 需要在多包、多目标仓库中统一编排构建的开发者。
- 希望将现有 CMake 工程**迁移**为 `gz` 描述结构的维护者：在包外完成构建/安装后，**手写** `package.xml` / `target.xml`（见 `doc/zh/getting-started.md`）。

## 核心用例（摘要）

1. 在工程 **cwd** 下执行 `configure`：扫描描述文件，生成中间构建树并写入缓存（含 `arch` 标签）。
2. `build`：按缓存中的后端与选项执行构建，将产物安装到约定安装前缀。
3. `run` / `test` / `pack`：基于安装树与元数据运行可执行体、CTest 或打归档。

## 非目标（当前阶段）

- 不替代各语言生态的原生包管理器作为唯一来源。
- 不在工具内硬编码特定第三方库名或专有布局；差异通过 XML 显式表达。

## 相关资产

- 详细工作流与 FAQ：`doc/zh/user-manual.md`（中文）、`doc/en/user-manual.md`（英文）；索引 **`doc/README.md`**。
- 设计背景：`DESIGN.md`、`mindmap.mmd`。
