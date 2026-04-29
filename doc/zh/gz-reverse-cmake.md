# CMake 逆向为 GroundZero XML（`gz_reverse_cmake`）



**逆向工程**在此指：在**不运行** `cmake` 配置、**不依赖** [CMake File API](https://cmake.org/cmake/help/latest/manual/cmake-file-api.7.html) 的前提下，对**顶层及 `add_subdirectory` 可达**的 `CMakeLists.txt` 做**命令流级静态分析**，重解释为 GZ 包描述初稿，写出 **`package.xml`** 与各 **`<包名>/<目标名>/target.xml`**，供 `gz configure` 消费或人工精修。  

**与「纯手写 `package.xml` / `target.xml`」**的关系：本路线是**辅助起稿**；**权威字段与行为**仍以 **[`package-target-xml-spec.md`](package-target-xml-spec.md)** 与 **`gz spec`** 为准。复杂工程在逆向后**几乎必然**要手改（宏、`if` 未求值、生成器表达式、外部 `include` 等）。



> **文档索引**（`doc/zh` / `doc/en` 全表）：[`../README.md`](../README.md)



## 1. 功能（能做什么）



| 点 | 说明 |

|----|------|

| **不 configure** | 只读磁盘上的 Listfile 文本，无需生成构建树、不解析 `codemodel-v2` JSON。 |

| **写出位置** | 只读扫描 `--source` 下 Listfile；XML 写在 **`<--out>/<包名>/`**。**未写 `--out` 时** 默认定在 **`<--source>/gz_reverse/`**（与 `CMakeLists.txt` 同根）；要其它根再**显式** `--out`。 |

| **可解释子集** | 将每文件整理为 `identifier(实参…)` 命令序列，在部分命令上**仿真** `project` / `set` / `include_directories` / `add_subdirectory` / `add_executable` / `add_library` / `target_*` 等。 |

| **与 File API 无关** | 不引入 JSON 反序列化、不读 `.cmake/api`。 |



实现细节、解析边界以 **`gz_reverse_cmake` 可执行**及仓库 **`ai-software-engineering/02-physical/gz-reverse-cmake/`** 内 `spec.md` 为**物理层**补充说明（与本文互补）。



## 2. 使用限制（能做什么的反面，摘要）



- **不**做完整 CMake 语义；`function` / `macro` / `foreach` / `while` 块内对可注册目标等命令**不**视同为顶层；**不**求 `if/else/endif` 真值。  

- 实参中 **`$<…>`** 生成器表达式在相关处**忽略**。  

- **跨包**依赖、**`prebuilt_*`**、细粒度 **`<config_files>`** / **`when`** 等通常需后续**手写**或二次整理。



完整清单以 **`ai-software-engineering/02-physical/gz-reverse-cmake/spec.md`** 与 **`../../gz_reverse_cmake/README.md`** 为准。



## 3. 如何获取工具



- 与仓库**根**一体构建：



  ```powershell

  cd <仓库根>

  python build.py

  # 可执行在 _build\Release\gz_reverse_cmake.exe 或对应多配置/生成器输出路径

  ```



- 若已通过 **`python install.py` / `python setup.py -i`** 安装**运行时**分量，一般可在 **`PATH`** 上直接运行 **`gz_reverse_cmake`**（与 **`gz`**、**`gz-gui`** 同批安装，见 `03-ops` 中开发维护说明）。



## 4. 命令行与目录约定

```text
gz_reverse_cmake [ --source <path> ] [ --out <path> ] […]
```

在含顶层 `CMakeLists.txt` 的目录下可**无参**执行：**`--source` 默认 = 当前工作目录**；**`--out` 默认 = `<--source>/gz_reverse/`**。

| 项 | 说明 |
|----|------|
| **`--source`** | 源根，**只读**；**省略 = 当前工作目录**。 |
| **`--out`** | 输出根；**省略 = `<--source>/gz_reverse/`**；实际在 **`<out>/<包名>/`** 写 XML。 |
| **与源树重叠** | 将 `--out` **显式** 指到与 `--source` **完全相同** 的路径时**可能警告**；**默认**的 `…/gz_reverse` 不警告。 |
| 其它 | **`--package-name` / `--package-version`**、**`--help`**。 |

**Windows 建议**：`cmd` 对 UTF-8 的显示可能乱码，工具在 Windows 上会为控制台设 UTF-8；**关键错误**会同时给英文一行，保证可读。

**最简**（在 zlib 根内，默认可写出到 `…\zlib-1.2.13\gz_reverse\<包名>\`）：

```text
cd E:\dev\egg_next\3rdparty\zlib-1.2.13
gz_reverse_cmake
```

**指定工作区下其它输出根**时，**显式**例如：

```text
gz_reverse_cmake --source E:\dev\egg_next\3rdparty\zlib-1.2.13 --out E:\dev\egg_next\gz_reverse
```



## 5. 生成之后做什么



1. 在**扫描根**中纳入含 **`package.xml` 的树**（或把输出树拷到工作区约定位置），按 **[`user-manual.md`](user-manual.md)** 与 [**`package-target-xml-spec.md`**](package-target-xml-spec.md) 补全/修正**包依赖、预置库、头文件 `to`、宏** 等。  

2. 运行 **`gz configure` / `gz build`** 等按既有 CLI 流程验证；差异大时以**手写规范**与 **`gz` 实现**为准。



## 6. 相关链接



| 内容 | 路径 |

|------|------|

| XML 规范（字段、块合并、`prebuilt`、类型） | [`package-target-xml-spec.md`](package-target-xml-spec.md) |

| 实现侧 README（构建、与工具条一致） | [`../../gz_reverse_cmake/README.md`](../../gz_reverse_cmake/README.md) |

| 物理层规格/边界（工程文档，AI 维护） | [`../../ai-software-engineering/02-physical/gz-reverse-cmake/spec.md`](../../ai-software-engineering/02-physical/gz-reverse-cmake/spec.md) |

| 逻辑层简要数据流 | [`../../ai-software-engineering/01-logic/detailed-design-gz-reverse-cmake.md`](../../ai-software-engineering/01-logic/detailed-design-gz-reverse-cmake.md) |

| 用户手册总入口 | [`user-manual.md`](user-manual.md) |



---



[← `doc` 文档索引 `README.md`](../README.md) · [英文本页 `../en/gz-reverse-cmake.md`](../en/gz-reverse-cmake.md)

