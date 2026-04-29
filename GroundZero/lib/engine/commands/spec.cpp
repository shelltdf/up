#include "spec.hpp"

#include <iostream>

namespace gz {

namespace {

// English-only: embedded spec for `gz spec` stdout. Aligned with doc/en/package-target-xml-spec.md; AI-oriented layout.
// Split into chunks: MSVC ~16kB string literal limit.
constexpr const char* kXmlSpecEnPart1 =
  R"SPEC(GZ_XML_SPEC_REVISION=33

# gz — package.xml and target.xml (machine-oriented summary)

This text is shipped inside `gz.exe`. It describes what the **current** `gz` implementation expects when you author
`package.xml` and `target.xml`. The parser is a **lightweight regex scanner**, not a full XML validator or DOM with
XSD: write well-formed XML and follow the shapes below.

---

## Table of contents (read in order for full behavior)

1. [Validation commands](#1-validation-commands) — `gz configure` / `gz list` (quick check after editing XML)
2. [Document model: DOM-style trees](#2-document-model-dom-style-trees) — what may appear under `<package>` / `<target>`
3. [Variables, builtins, merge, `when`, templates](#3-variables-builtins-merge-when-templates) — one place for substitution rules
4. [Scripts, triggers, preprocess / postprocess](#4-scripts-triggers-preprocess--postprocess) — message names and command fallback
5. [package.xml — field reference](#5-packagexml--field-reference)
6. [target.xml — field reference](#6-targetxml--field-reference)
7. [Encoding, paths, primary package](#7-encoding-paths-primary-package)
8. [Examples](#8-examples)
9. [Implementation pointers](#9-implementation-pointers)
10. [Install trees, `gz pack`, redistribution XML](#10-install-trees-gz-pack-redistribution-xml)

---

## 1. Validation commands

After authoring XML, always validate with:

  gz configure [--scan <dir>]... [--opt KEY=VALUE]...
  gz list [--format tree|json|xml] [--xml <path>] [--json <path>] [--quiet]

`configure` is the source of truth for dependency and path errors.

`gz list` output behavior:
- stdout payload is selected by `--format` (`tree` default, or `json`/`xml`).
- `--xml <path>` / `--json <path>` are file exports and can be combined with any stdout format.
- `--quiet` suppresses tree text and export hint lines; it does not suppress payload for `--format json|xml`.
- warning-only combinations (do not fail execution):
  - `--format xml` + `--json <path>`: warns stdout is XML while JSON is file-only.
  - `--format json` + `--xml <path>`: warns stdout is JSON while XML is file-only.

**Scan / discovery (configure and `gz list`).** The engine searches recursively for `package.xml` and `target.xml` from **cwd** and
each `--scan` root. Recursion **does not** descend into directories named **`.intermediate`**. Any `--scan` path that resolves
under **`<cwd>/.intermediate/`** (after normalization) is **dropped** with a warning, so e.g. **`gz-redist/`** under a build
tree is not treated as a source package root.

---

## 2. Document model: DOM-style trees

The loader builds in-memory **package** and **target** descriptions (see **§9** source files). **Logical** structure (order of
**paired** child blocks in the file is **document order**; repeated block bodies **merge** per rules in each section):

### 2.1. `package.xml` (one file per **package root**)

```
package  (root; attributes: name= required, version= optional; default version 0.0.0 if omitted)
+-- dependency*     (self-closing: <dependency name="..." optional="true|false"/>; regex-collected, order preserved)
+-- vars*            (block; may repeat; body entries merge: <var name= value=/>; script vars: type=script, see §4)
+-- defines*         (block; may repeat: <define name= value=/>)
+-- config_files*   (block; may repeat: <file in= to=/> — paths relative to package root for `in`)
```

- Every `target.xml` under this tree is assigned the **nearest** parent `package.xml` (`nearest_package_parent`).
- `gz configure` scans from cwd and each `--scan` root for `package.xml` and `target.xml` recursively.

### 2.2. `target.xml` (**exactly one** per **target directory**)

**Native compile** (`type` = `executable` | `library` | `static_library` | `shared_library`) — typical layout:

```
target  (root; name= required, type= optional, default `executable`)
+-- vars*
+-- config_files*
+-- defines*
+-- sources*        (or bare <file> only when zero <sources> block exists, see §6)
+-- headers*
+-- assets*
+-- dependency*     (self-closing)
```

**Prebuilt** (`type` = `prebuilt_static_library` | `prebuilt_shared_library`):

```
target
+-- prebuilt        (void: last non-empty parse wins if multiple; layout attrs, see §6)
```

**Asset bundle** (`type` = `asset_bundle`): at least one of `sources` / `headers` / `assets` per type rules in §6.

**Repeated balanced blocks (merge order)** under `<package>` / `<target>`:
- **package.xml:** `<vars>`, `<defines>`, `<config_files>` — each may repeat; bodies **append** in file order to one logical list.
- **target.xml:** `<sources>`, `<headers>`, `<assets>`, `<vars>`, `<defines>`, `<config_files>` — same merge rule.
- **Self-closing / void tags:** multiple **`<prebuilt …/>`** — the **last** non-empty parse wins for the prebuilt slot.
- **`<dependency/>`** is global-regex collected; order is list order.
- **Bare `<file>…</file>`** (no `<sources>` wrapper): only scanned when there is **zero** `<sources>…</sources>` in the file;
  if any `<sources>` exists, only inner block bodies are used; outer bare `<file>` is ignored.
- For **full-tag sketches** of every child shape, see **§8** (6b).

---

## 3. Variables, builtins, merge, `when`, templates

### 3.1. Who sets what (for AI / automation)

| Category | Set by | Examples / notes |
|----------|--------|------------------|
| **Builtins (per configure context)** | `gz configure` | `GZ_OS` (`windows`|`linux`|`darwin`), `GZ_PACKAGE_NAME`, `GZ_PACKAGE_VERSION`, `GZ_TARGET_NAME`, `GZ_TARGET_BUILD_SYSTEM` (`cmake`|`ninja`), `GZ_CONFIG` (`debug`|`release`) — see `var_subst.cpp` `builtin_host_os` and similar. |
| **User / project defaults** | `package.xml` and `target.xml` **`<vars>`** | Scalar `<var name="K" value="V"/>` (and optional empty value). Merged in layers (below). **Script** vars are a separate sub-syntax (`type="script"`, §4) and **not** in the `when` scalar map. |
| **User overrides at configure** | `gz configure --opt KEY=value` and `gz_cache.txt` | Same key namespace as `GZ_*` and extra C identifiers; **wins** over XML defaults for the same name (see layers). **Reserved** keys: `cwd`, `arch`, `package`, `generated_file`, `scan_roots`, `gz.cache.version` (and similar metadata) — not used as free-form opt keys. |
| **Compile macros** | `package.xml` / `target.xml` **`<defines>`** | C identifiers; applied in package-then-target order (target can add/override for compile flags). |
| **Config file templates** | `package.xml` / `target.xml` **`<config_files>`** | `in` / `to` files; `to` under `.intermediate/generated/...` — see §5/§6. **Placeholder rule:** in template text only, missing `@NAME@` / `${NAME}` get **empty** entries added to the map so placeholders strip (see §3.3). |

### 3.2. Merge order (later overrides earlier)

Used for **`@KEY@`** and **`${KEY}`** in **target-level** `<config_files>` (full per-target map), **package-level** `<config_files>`
(builtins + package `<vars>` + **all targets'** `<vars>` in **build** order, then workspace; `GZ_TARGET_NAME` empty at package level unless set by context), and for **`when="..."`** on **`<sources>`** and **`<headers>`** lines.

**Layers:**

1. **Builtins** — as in §3.1 table.
2. **Package `<vars>`** — defaults for all targets in the package.
3. **Target `<vars>`** — overrides package for the same key.
4. **Workspace options** — from `--opt` and `gz_cache.txt` (same map as `GZ_*` build switches, plus C identifiers) — **overrides** XML for same key.

**`when` and templates:** in **`when="..."`**, only the merged map applies; `NAME` must be a C identifier. **`$<...>`** generator expressions are **not** interpreted by `gz`.

### 3.3. `config_files` template-only behavior

Before substitution, every **`@NAME@`** and **`${NAME}`** in the template where `NAME` is a C identifier and **missing** from the
merged map is **added with empty value** so the placeholder is removed. Set real values in **`<vars>`** or **`--opt`** if empty
is wrong (e.g. `typedef ${ZIP_INT8_T} ...`).

Substitution alternates both passes (max 64 rounds) for **target** templates. After replacement, `#cmakedefine` / `#cmakedefine01`
are expanded (subset like CMake). See **§6** for full `config_files` path rules and backends note.

### 3.4. `when` — where it is supported

- **Implemented:** optional `when="..."` on **`<sources>`** entries (`<file …/>`, `<glob …/>`, paired with `from=`) and on **`<headers>`** entries (`<dir/>`, `<file/>`, `<glob/>`).
- **Not implemented (ignored; do not rely on):** `<assets>`, `<define>`, `<dependency/>`, `<config_files>`, `<var>`, package-level
  rows, `<prebuilt/>`, preprocess/postprocess tags, `<package>`/`<target>` roots. **`<install …/>`** is not a supported void tag; use
  **`<prebuilt …/>`**.

**Product direction (not all implemented).** Extending `when` uniformly to every repeatable child would need a defined
meaning (e.g. skip vs fail for `<dependency>`) and more configure work; the forms above are what the code evaluates today.

### 3.5. `when` expression grammar (matches `eval_when` in `GroundZero/lib/engine/xml/var_subst.cpp`)

String is **trimmed**; then **exactly one** of (in order):

1. **Empty** — **true**.
2. **Boolean literal** — whole string `true` or `false` (case-insensitive).
3. **Comparison** — `KEY == RHS` or `KEY != RHS` (optional spaces). `KEY` is one identifier; `RHS` is a **single** token
   `[A-Za-z0-9_.]+` only. The left-hand value is the merge-map entry for `KEY` or, if `KEY` is **missing**, the **empty string**;
   compared **lowerASCII** to `RHS`.
4. **Bare identifier** — must **exist** in the merged map; truthy unless empty/0/false/off/no. **Missing key** = configure error
   (unlike `==` / `!=` forms).

**Not supported:** `&&` `||`, parentheses, functions, substring, arbitrary text.

---

)SPEC";

constexpr const char* kXmlSpecEnPart2 =
  R"SPEC(## 4. Scripts, triggers, preprocess / postprocess

**`<var type="script" .../>`** in **`<vars>`** (package or target) declares a **message slot** (attribute **`trigger`**).
**`script_type`** defaults to **`lua`**. The repo does **not** yet execute a **Lua VM** on `value`; the attribute name is
historical. **Actual behavior:** for a given `trigger` on a `<preprocess>`/`<postprocess>` node, if **`<preprocess command="..."/>`**
or **`<postprocess command="..."/>`** is **non-empty**, that string is used. If the command is **empty**, **`resolve_script_command`**
walks from **current target** up the **parent chain to package**, collecting `<var type="script">` with the **same** `trigger`
(see `script_execution.cpp`); **target-level before package-level**; **first non-empty `value`** wins as the **entire** shell
line. Put **real shell** (or `lua -e '...'` if on PATH) in `value` if you need executable behavior today.

| trigger | Meaning | Dispatched at configure? | Paired XML |
|---------|---------|--------------------------|------------|
| sources.preprocess | Before each source compiles | Yes | `<preprocess command="..."/>` under `<sources>` `<file>`/`<glob>`; empty command uses script `value` |
| sources.postprocess | After each source (backend-specific) | Yes | `<postprocess command="..."/>` |
| headers.preprocess | Before header entry for compile+install | Yes | Under `<headers>` entries |
| headers.postprocess | After header entry | Yes | Under `<headers>` |
| assets.preprocess | Before assets | Yes | Under `<assets>` |
| assets.postprocess | After assets | Yes | Under `<assets>` |
| manual | Reserved; not auto-dispatched | **No** | Future CLI/GUI |

**Case-sensitive** `trigger` string; must match the table **exactly**. Any other string = **load error** (`is_supported_script_trigger` in `simple_xml.cpp`).

- **Scalar `<var>`** participates in **`when`** and **`@` / `${}`**; **script `<var>`** does **not** participate in the scalar **merge
  map** for `when` — it is only **command fallback** for paired preprocess/postprocess entries.

---

)SPEC"
  R"SPEC(## 5. package.xml — field reference

### Root
- First `<package` … up to first `>` = header.
- **Required attribute:** `name` — **globally unique** among all packages in one scan.
- **Optional:** `version` — default `0.0.0` if omitted.

### `<dependency/>`
- Self-closing. `name` = another **package** in the same scan (unless `optional` = true/1/yes).
- If any `target.xml` uses `OtherPkg:TargetName`, that **OtherPkg** must appear here (non-optional) or configure fails.

### `<vars>` (see also §3, §4)
- Repeating blocks; `<var name= value=/>` merge in order. Script vars: `type=script` `script_type=lua` `trigger=...` `value=...`.

### Workspace / `--opt` (see §3.1)
- Keys: any `GZ_*` plus C identifiers, except reserved cache lines listed in §3.1. Applied **after** package/target vars.

### `<defines>` (package)
- Same element shape as target. Applied to **every** native compile target in the package **before** that target's own
  `<defines>` (so target can override for compile flags).

### `<config_files>` (package)
- `<file in="rel" to="out"/>` both required. `in` relative to **package root** (parent of `package.xml`). `out` under
  **`.intermediate/generated/<package>/_package/`**; reserved name **`_package`**. Avoid naming a **compile** target
  **`_package`** in the same package (clashes with the reserved output dir). Substitution map: builtins + package
  **`<vars>`** + **all** this package's `target.xml` **`<vars>`** in **build target order** (later wins on duplicate) +
  workspace. **`GZ_TARGET_NAME` empty** unless a target var sets it when used in package config map. Output written
  **once per configure**; appended to **every** native compile target as extra source; include path **`.intermediate/generated/<package>/_package/`**
  added to all when any package `config_files` exist.

---

## 6. target.xml — field reference

### Root
- First `<target` … to first `>`. **name** required. **type** optional, default `executable` (lowercase in table below).

### Supported `type` values

| type | sources | prebuilt | Notes |
|------|---------|----------|------|
| executable | required 1+ | no | |
| library | required | no | Resolves to static/shared via `GZ_TARGET_DYNAMIC_LIBRARY` / `GZ_DYNAMIC_LIBRARY` |
| static_library | required | no | Always static |
| shared_library | required | no | Always shared |
| asset_bundle | may be empty | no | Need at least one of sources / headers / assets |
| prebuilt_static_library | not required | **required** | Paths relative to `target.xml` unless abs |
| prebuilt_shared_library | not required | **required** | Win: dll + import .lib resolvable |

Unknown `type` = configure error (`unknown target type`).

### `<prebuilt/>`
- Layout: implied install dir segment from `gz_cache.txt` `arch=` and `compose_arch_tag`; on `<prebuilt/>` use split fields
  `os` `cpu` `build_system` `toolchain` `link` `config` `crt` — see `paths.cpp` `compose_arch_tag` / `try_decompose_compose_arch_tag`.
  `gz build` redistribution and schema 3 `gz_redist_manifest.json` use split fields. Legacy `arch="..."` read then decomposed; not re-emitted.
  Schema 1 legacy `arch`; 2/3 from JSON.

### `<sources>`, bare `<file>`
- As §2.2. Self-closing `<file from= when=/>` `<glob from= when=/>` or paired with inner preprocess/postprocess. **`<vars>`** in
  target block merges like package. Script vars: §4.

### `<config_files>` (target)
- `in` relative to `target.xml` directory; `to` under **`.intermediate/generated/<package>/<target>/`**, no `..`, not absolute.
  Merged var map: full target stack per §3.2; `#cmakedefine` after `@` substitution; 64 pass limit. Generated dir added to **include
  paths** for that target. **Backends (CMake and Ninja)**: no CMake `configure_file()` emission — generated file is a normal source. For full upstream
  `configure_file`, run outside `gz` and check in the file.

)SPEC"
  R"SPEC(### `<headers>` — compile + install layout
- Repeating blocks. Self-closing: `<dir from= to=/>` `<file from= to=/>` `<glob from= to=/>` (`<includes>` not supported).
- `from` required, relative to `target.xml`. `to` optional under install prefix `include/`. `when` per §3.4/3.5 on each row.

### `<assets>`
- Repeating; `from` / `to` and preprocess/postprocess. **`when` on assets not implemented.**

### `<defines>` (target)
- C identifier `name`, optional `value`. If `value` is present, it may use only alnum and **`._+-/`** (no spaces), matching the
  current implementation. Applied `PRIVATE` for native compiles; CMake: `target_compile_definitions`; Ninja: `/D` or `-D` flags.

### `<dependency/>`
- Same package: `name` only. Cross: `Package:Target`. **visibility** `private|public|interface` (default private). `interface` **rejected** when consumer is **executable**. **Valid** ref targets: library-like (`static_library`, `shared_library`, `library`, `prebuilt_*`); `asset_bundle` in graph **validation**; configure **fails** if unresolved or non-library. CMake notes: `library` rewritten to static or shared before emission; exes in same package **implicit private-link** all in-package library targets if **no** explicit `<dependency/>` lines; with explicit lines, use visibility. Static-to-static **transitive** `target_link_libraries` may **not** be fully chained — final exe may need explicit deps.

---

## 7. Encoding, paths, primary package

- Prefer **UTF-8** files. **ASCII-only** paths for scanned files are enforced in places; non-ASCII may hard-fail.

### Multi-package scans (primary)
- Prefer the package whose `package.xml` directory **equals cwd**.
- Otherwise the **first** discovered package may be treated as primary (implementation / filesystem order). Single-package
  cwd workflows are the least surprising.

---

)SPEC";

constexpr const char* kXmlSpecEnPart3 =
  R"SPEC(## 8. Examples

### 8.1. Minimal (typical app + dependency) — also see full shapes in 8.2

package.xml:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<package name="my_app" version="0.1.0">
  <dependency name="my_sdk" optional="false"/>
)SPEC"
  R"SPEC(</package>
```

target.xml (executable next to sources):
```xml
<?xml version="1.0" encoding="UTF-8"?>
<target name="my_app" type="executable">
  <sources>
    <file>main.cpp</file>
  </sources>
  <headers>
    <dir from="."/>
  </headers>
  <dependency name="my_sdk:my_sdk_static"/>
</target>
```

### 8.2. Full-tag reference sketches (syntax only; not one runnable tree)

Omit blocks you do not need. Some combinations are mutually exclusive by `type` (e.g. `prebuilt_*` vs full `<sources>`). **Full logical DOM**
for all tags is in **§2** and the tables in **§5–6**.

**package.xml — all major top-level constructs (duplicate dependency rows not all shown):**

```xml
<?xml version="1.0" encoding="UTF-8"?>
<package name="demo_pkg" version="1.0.0">
  <dependency name="other_pkg" optional="false"/>
  <dependency name="maybe_pkg" optional="true"/>
)SPEC"
  R"SPEC(  <vars>
    <var name="MY_DEFAULT" value="from_package_xml"/>
    <var name="FLAG_ONLY"/>
    <var name="PACKAGE_SCRIPT" type="script" script_type="lua" trigger="manual" value="print('pkg script')"/>
  </vars>
  <defines>
    <define name="DEMO_PKG_MACRO" value="1"/>
    <define name="DEMO_PKG_ONLY"/>
  </defines>
  <config_files>
    <file in="templates/pkg_header.hpp.in" to="generated/pkg_header.hpp"/>
  </config_files>
</package>
```

**target.xml — native compile** (`executable` | `library` | `static_library` | `shared_library`):

```xml
<?xml version="1.0" encoding="UTF-8"?>
<target name="demo_lib" type="static_library">
  <vars>
    <var name="MY_DEFAULT" value="from_target_xml"/>
    <var name="TARGET_SCRIPT" type="script" script_type="lua" trigger="sources.preprocess" value="print('src pre')"/>
  </vars>
  <config_files>
    <file in="version.hpp.in" to="generated/version.hpp"/>
  </config_files>
  <defines>
    <define name="DEMO_TARGET" value="1"/>
  </defines>
  <sources>
    <file>source_path.cpp</file>
    <file from="single.cpp" when="GZ_OS==windows"/>
    <glob from="src/*.cpp" when="true"/>
    <file from="gen.cpp">
      <preprocess command="touch gen.cpp.stamp || exit 0"/>
      <postprocess command=""/>
    </file>
  </sources>
  <headers>
    <dir from="include" to="demo"/>
    <file from="include/demo/single.hpp" to="demo"/>
    <glob from="include/demo/*.hpp" to="demo" when="GZ_OS==linux"/>
  </headers>
  <assets>
    <dir from="assets" to="share/demo"/>
    <file from="assets/readme.txt" to="share/demo"/>
    <glob from="assets/*.md" to="share/demo"/>
  </assets>
  <dependency name="other_pkg:their_lib" visibility="public"/>
</target>
```

**target.xml — prebuilt:**

```xml
<?xml version="1.0" encoding="UTF-8"?>
<target name="sdk_stub" type="prebuilt_static_library">
  <prebuilt import_lib="lib/third_party.lib" os="windows" cpu="x86_64" build_system="cmake" toolchain="msvc" link="static"
    config="release" crt="dynamic_md"/>
</target>
```

**asset_bundle:** at least one of sources / headers / assets per type rules in §6.

---

## 9. Implementation pointers (for humans and AI)

| Topic | Source |
|-------|--------|
| DOM model, script context | `GroundZero/lib/engine/dom/dom_model.hpp`, `dom_nodes.hpp`, `dom_node_visitor.{hpp,cpp}`, `dom_document.{hpp,cpp}`, `script_execution.{hpp,cpp}` |
| Load/parse XML | `GroundZero/lib/engine/xml/simple_xml.cpp` |
| Types | `GroundZero/lib/engine/xml/simple_xml.hpp` (`PackageDesc`, `TargetDesc`, `ConfigFileEntry`, `DefineEntry`, …) |
| Configure / graph | `GroundZero/lib/engine/commands/configure.cpp` |
| Variable merge, `when`, `#cmakedefine` | `GroundZero/lib/engine/xml/var_subst.cpp` |
| `gz list` / export | `list.cpp` and related |
| `gz pack` / redist | `redist_emit.cpp`, `build.cpp`, `pack.cpp`, `backend_dispatch.cpp` (`run_pack_backend`) |

Longer **human** topic docs: `script-messages` (triggers, shell fallback detail), `script-tutorial`, `internal-variables` (repo `doc/en/` or `doc/zh/`), and **`package-target-xml-spec`** (narrative + `gz spec` as embedded master for English).

---

)SPEC"
  R"SPEC(## 10. Install trees, `gz pack`, redistribution XML (`gz build`)

- **`gz pack`** only **archives** the install root(s) for `--install-dir-name` (under `.intermediate/install/<arch>/`, e.g. `bin/`, `lib/`, `include/`). It does **not** create new `package.xml` / `target.xml` at source; it does not rewrite your hand-authored targets from the scan set.

- **Redistribution XML (default on)**: after **`gz build`**, the engine reads **`.intermediate/build/<leaf>/gz_redist_manifest.json`**
  (from **`gz configure`**, **schema 3** with split layout fields). If it lists **targets**, writes **`<install>/gz-redist/package.xml`**
  and one **`<install>/gz-redist/<emit-name>/target.xml`** per entry. Disable with **`--no-emit-redistribution-xml`** or
  **`GZ_EMIT_REDIST_XML=0`** / `false` / `off` / `no`. Library targets emit as **`prebuilt_*`** with **`<prebuilt …/>`** rebased
  relative to each `target.xml` and layout from manifest. Schema 1 may use legacy **`arch`**. **Executable** targets omitted
  (MVP). Code: **`redist_emit.cpp`**, **`configure.cpp`**, **`build.cpp`**.

- **`gz pack`** includes **`gz-redist/`** when it exists under the packaged install tree.

End of embedded spec.
)SPEC";

}  // namespace

int cmd_spec(const std::vector<std::string>& args) {
  (void)args;
  std::cout << kXmlSpecEnPart1 << kXmlSpecEnPart2 << kXmlSpecEnPart3;
  return 0;
}

}  // namespace gz
