# 脚本消息（trigger）总表：`package.xml` / `target.xml` 与 Lua 绑定

> **文档索引**（`doc/zh` / `doc/en` 全部入口表）：[`../README.md`](../README.md)  
> **英文完整版**：[`../en/script-messages.md`](../en/script-messages.md)

本文档列出 **`gz configure`** 阶段可识别的 **消息名**（XML 属性 **`trigger="..."`**），以及 **`<var type="script" …>`** 如何与 **`<preprocess>` / `<postprocess>`** 的 **`command`** 协同。实现入口：**`GroundZero/lib/engine/dom/script_execution.cpp`**（解析与解析命令）、**`GroundZero/lib/engine/commands/configure.cpp`**（`apply_script_command` 调用点）、**`GroundZero/lib/engine/xml/simple_xml.cpp`**（`is_supported_script_trigger`）。

---

## 1. XML 写法（包级与目标级相同）

在 **`package.xml`** 或 **`target.xml`** 的 **`<vars>...</vars>`** 内声明：

```xml
<var name="MY_HOOK" type="script" script_type="lua" trigger="sources.preprocess" value="echo hooked"/>
```

| 属性 | 说明 |
|------|------|
| **`name`** | 脚本变量名（便于阅读/去重）；**不参与**命令字符串拼接。 |
| **`type`** | 必须为 **`script`**。 |
| **`script_type`** | 可选，默认 **`lua`**。当前实现下，**仅** `lua` 或**空**（解析默认成 `lua`）的条目会参与 **`resolve_script_command`**；其它类型会被**跳过**。 |
| **`trigger`** | 消息名，见下表；**大小写敏感**，须与表中字符串**完全一致**。 |
| **`value`** | 在选中作为「脚本回退命令」时，**整段**作为 **shell/cmd 命令行** 交给生成后端（见 §3）。 |

**挂载范围**：包级 `<vars>` 中的脚本变量挂在 **Package** 节点；目标级挂在 **Target** 节点。configure 为某一目标解析某条 `preprocess`/`postprocess` 时，`collect_scripts_for_message` 会从 **当前目标节点沿父链走到包节点**，收集 **同 trigger** 的脚本变量（见 **`script_execution.cpp`**）。**同一节点上**多条同 trigger 时，**按 XML 中出现顺序**；**目标节点先于包节点**遍历，故 **目标级脚本先于包级** 进入列表；**`resolve_script_command` 取第一条非空 `value`**，因此 **目标级可覆盖包级** 的同 trigger 回退命令。

---

## 2. 消息（trigger）一览表

| `trigger` 取值 | 含义（产品语义） | 当前 **configure** 是否派发 | 与哪类 XML 片段配对 |
|----------------|------------------|-----------------------------|----------------------|
| **`sources.preprocess`** | 编译某一源文件**之前**可插入的命令 | **是** | `<sources>` 内 `<file>` / `<glob>` 上的 **`<preprocess command="..."/>`**（或配对块内）；若 `command` 为空则用本消息下的脚本 `value`。 |
| **`sources.postprocess`** | 源级后处理（生成后端语义见 CMake/Ninja 文档） | **是** | 同上 **`<postprocess command="..."/>`**。 |
| **`headers.preprocess`** | 安装/编译用头文件路径处理**之前** | **是** | **`<headers>`** 内 `<dir>` / `<file>` / `<glob>` 的 preprocess。 |
| **`headers.postprocess`** | 头文件条目的后处理 | **是** | **`<headers>`** 内对应 postprocess。 |
| **`assets.preprocess`** | 资源拷贝/处理**之前** | **是** | **`<assets>`** 内各条目的 preprocess。 |
| **`assets.postprocess`** | 资源条目的后处理 | **是** | **`<assets>`** 内 postprocess。 |
| **`manual`** | 预留：由用户或 GUI **手动**触发的脚本槽位 | **否**（解析合法，但 **configure 未调用** `apply_script_command(..., "manual", ...)`） | 无自动配对；未来若 CLI/GUI 提供「执行包/目标脚本」可用此 trigger。 |

**未在表中出现的字符串**：`load_package_xml` / `load_target_xml` 阶段会 **configure 失败**（报错见 `simple_xml.cpp`）。

---

## 3. 「Lua 绑定」与当前实现的真实行为

- **命名**：历史与 **`gz spec`** 使用 **`script_type="lua"`**；仓库内 **尚未接入 Lua 虚拟机** 去执行 `value` 中的语句。
- **实际行为**：对某一 `trigger`，若对应 XML **已写** **`command="..."`**，则 **直接使用该字符串**，**不会**再读取同 trigger 的 **`<var type="script">`**。
- 若 XML **未写** `command`（空串），则 **`resolve_script_command`** 会按 §1 的继承顺序查找 **`type="script"`** 且 **`trigger` 匹配** 且 **`script_type` 为 lua（或默认）** 的变量，取 **第一条** 的 **`value`** 作为 **整条 shell 命令**。
- 因此：今天要把「可执行逻辑」绑在消息上，请把 **`value` 写成一行或多行 shell**（或调用 **`lua -e '...'`** 若本机 PATH 有 `lua`），而不是只写 `print(...)` 指望由宿主解释。

**生成后端**：命令最终进入 **`cmake_backend.cpp`** / **`ninja_backend.cpp`** 生成的规则（与 **`script-tutorial.md` §4** 一致）。

---

## 4. 与 `when`、其它 `<var>` 的关系

- **标量 `<var name="KEY" value="VAL"/>`**：参与 **`when`** 与 **`@KEY@` / `${KEY}`** 替换（见 **`package-target-xml-spec.md` §3.5**）。
- **脚本型 `<var type="script">`**：**不参与** `when` 求值键值合并；仅在上表 **已派发** 的 trigger 路径上作为 **命令回退**。
- **`when` 不作用于** `<var>` 本身（规范已注明）；不能靠 `when` 隐藏整条脚本 var。

---

## 5. 行为提示：全局脚本 var 与「每条源」

对 **`sources.preprocess` / `sources.postprocess`**，`configure` **按每条 `<file>` / `<glob>` 展开后的源**各调用一次 **`apply_script_command`**。若某条源 **未写** XML **`<preprocess command="..."/>`**（即命令为空），则会回落到 §1 的脚本 **`value`**。

因此：在 **包级** 或 **目标级** `<vars>` 里声明 **`trigger="sources.preprocess"`** 且 **`value` 非空** 时，**所有「未写 XML preprocess」的源**都会得到**同一条**回退命令（除非被子节点上更靠前的脚本 var 抢先匹配）。若只想对**单个**输入跑生成器，请在该 **`<file>`** 内写 **`<preprocess command="..."/>`**（可写具体命令，或写空 `command` 再配目标级专用脚本 var——视解析器是否允许空 `command` 属性而定；**推荐**始终写显式 `command`）。详见 **`script-tutorial.md` §4**。

---

## 6. 相关文档

| 文档 | 内容 |
|------|------|
| `script-tutorial.md` | `preprocess` / postprocess、Qt、CMake 后端差异 |
| `internal-variables.md` | 标量变量与脚本 var 的区分 |
| `package-target-xml-spec.md` | §2.7 脚本型 `<var>` 索引 |
| `gz spec` | 英文内嵌规范中的 Script var 行 |

若未来版本为 **`manual`** 或其它 trigger 增加 **真实 Lua 回调** 或 **多脚本合并策略**，以 **`gz spec`** 与 **`script_execution.cpp`** 为准并更新本表。
