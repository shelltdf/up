# `package.xml` and `target.xml` specification

> **Documentation index** (full `doc/zh` / `doc/en` table): [`../README.md`](../README.md)  
> This file is the **full English** specification prose. Optional **Simplified Chinese** edition: [`../zh/package-target-xml-spec.md`](../zh/package-target-xml-spec.md).

This document describes **gz (GroundZero)**’s current **parsing rules** for the two descriptor files and **configure-time semantic constraints**. The implementation uses a **lightweight regex scanner** (see `GroundZero/lib/engine/xml/simple_xml.cpp`), **not** a full XML validator: write well-formed XML and follow the recognizable shapes below.

**Authoritative machine-oriented English** is the text printed by **`gz spec`** (stdout), versioned by **`GZ_XML_SPEC_REVISION`** inside `GroundZero/lib/engine/commands/spec.cpp` (currently **33**). That embedded text is organized for tooling around three pillars: **DOM-style trees** (`package` / `target`), **variables** (builtins, merge, `when`, templates), and **scripts / triggers** (with `script-messages.md` detail). **This file** keeps narrative prose, field tables, examples, and cross-links; if anything disagrees with **`gz spec`** or the implementation, **`gz spec` and source code** win.

### How this doc fits with others

| Need | Document |
|------|----------|
| **Subcommands, argv, exit codes, intermediate dirs, `--build-dir-name` / `--install-dir-name`** | [`cli-reference.md`](cli-reference.md) |
| **Step-by-step tutorial and sample XML** | [`getting-started.md`](getting-started.md) |
| **Doc map, FAQ, gz-gui, high-level overview without XML fields** | [`user-manual.md`](user-manual.md) |
| **Built-ins, `gz_cache.txt`, `GZ_*`, `CMAKE_PREFIX_PATH` aggregation** | [`internal-variables.md`](internal-variables.md) |
| **This file**: `package.xml` / `target.xml` **fields, repeated blocks, `when`, deps, type rules** | (this page) + **`gz spec`** |

### Alignment with embedded `gz spec` (revision 33)

| Topic in `gz spec` | Where to read in this document and repo |
|--------------------|----------------------------------------|
| §1 Validation (`gz configure` / `gz list`) | Subcommand details: [`cli-reference.md`](cli-reference.md); list stdout / export quirks are also summarized in `gz spec` §1. |
| §2 DOM-style trees (`package` / `target` allowed children) | **§1** (file roles, merge), **§2–3** (field-by-field). |
| §3 Variables (builtins, merge order, `when`, `config_files` templates) | **§2.4–2.6**, **§3.5**, and [`internal-variables.md`](internal-variables.md). |
| §4 Scripts / `trigger` / preprocess | **§2.7** and [`script-messages.md`](script-messages.md). |
| §5–6 `package.xml` / `target.xml` field reference | **§2** and **§3** below. |
| §7 Encoding / primary package | **§4–5**. |
| §8 Examples | **§6** and [`../../test_projects/README.md`](../../test_projects/README.md). |
| §9–10 Implementation / `gz pack` / redistribution | **§7–8**; `redist_emit.cpp` / `pack.cpp` as listed in `gz spec` §9–10. |

### Product direction (recommended workflow)

- **Recommended**: **hand-write** **`package.xml`** and **`target.xml`**, explicitly listing sources, `<headers>`, `<defines>`, `<dependency>`, **`<prebuilt …/>`**, and **`prebuilt_static_library` / `prebuilt_shared_library`**; walkthrough: **[getting-started.md](getting-started.md)**.
- **Upstream CMake libraries**: build or install **outside** the gz package, place **`.lib` / `.a` / `.dll` / `.so`** on a reachable path (or fixed SDK dir), then in **`target.xml`** use **`prebuilt_static_library` / `prebuilt_shared_library`** + **`<prebuilt …/>`** pointing at those **disk paths** (relative to **`target.xml`** directory or absolute).

---

## 1. File roles and placement

| File | Location | Role |
|------|----------|------|
| **`package.xml`** | Root of each **standalone package** (same tree level as inner `target.xml` dirs) | Declares package name, version, **package-level** dependencies (other package names); optional **`<vars>`**, **`<defines>`**, **`<config_files>`**, etc. |
| **`target.xml`** | **One subdirectory per build target**, **exactly one** `target.xml` per dir | Declares target name, type, sources, optional header layout, **target-level** dependencies (other targets, usually libraries). |

- **Attachment**: each `target.xml` must sit under some `package.xml` directory tree; `configure` assigns the target to the **nearest** package root (`nearest_package_parent` in `configure.cpp`).
- **Scan**: **`gz configure`** / **`gz list`** **recursively** find all `package.xml` and `target.xml` under scan roots (default **cwd**, or **`--scan`** directories). Recursion **never descends** into a directory named **`.intermediate`**. In addition, any **`--scan`** root that resolves **under `<cwd>/.intermediate/`** is **dropped** with a warning so generated trees (e.g. **`gz-redist/`**) are not mistaken for source packages.

### 1.1 Repeated balanced blocks (merge order)

Inside **`<package>`** in **`package.xml`** and **`<target>`** in **`target.xml`**, these **paired** sections may appear **any number of times** in document order; each body is parsed and results are **appended** to the same logical list (intra-block order rules still apply):

- **`package.xml`**: **`<vars>`**, **`<defines>`**, **`<config_files>`**
- **`target.xml`**: **`<sources>`**, **`<headers>`**, **`<assets>`**, **`<vars>`**, **`<defines>`**, **`<config_files>`**

**Void / self-closing tags** (e.g. **`<prebuilt …/>`**, **`<dependency …/>`**) may repeat: **`<prebuilt/>`** — the **last** non-empty parse wins for the prebuilt slot; **`<dependency/>`** is still collected by global regex; order is list order.

**Bare `<file>…</file>`** (not inside `<sources>`): only scanned **when there is no** **`<sources>…</sources>`** block in the file; if **any** `<sources>` block exists, sources come **only** from those block bodies; **outer** bare `<file>` is ignored.

---

## 2. `package.xml`

### 2.1 Root element

- The parser takes the **first** substring **`<package`** and the opening tag up to the **first `>`** as the package header (no full XML tree validation required).
- **Required attributes**
  - **`name`**: package name; must be **globally unique** within one configure scan; must match cross-package references `PackageName:TargetName`.
- **Optional attributes**
  - **`version`**: defaults to **`0.0.0`** if omitted.

Example:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<package name="hello_demo" version="0.1.0">
  <dependency name="hello_lib"/>
</package>
```

### 2.2 Package dependency `<dependency ... />`

- Shape: self-closing **`<dependency ... />`** containing **`name="..."`** (double-quoted).
- **`name`**: another **package** name; that package must appear in the **same configure scan**, otherwise:
  - If **`optional="true"`** (or value **`1` / `yes`** per implementation parsing): optional only;
  - Else **configure fails** with a missing-package error.
- **`optional`**: if present, only **`true` / `1` / `yes`** mean optional; other values ⇒ **not** optional.

Implementation matches all `<dependency` … `/>` fragments with regex; nesting under `<package>` is **not** required (but recommended for readability).

### 2.3 Relationship to `target.xml`

If a `target.xml` uses **`OtherPkg:TheirLib`**, **`OtherPkg`** must appear in this `package.xml` as **`<dependency name="OtherPkg"/>`** (except optional-deps rules above), or **configure fails**.

---

## 3. `target.xml`

### 3.1 Root element

- A **`<target`** opening tag must exist; same parsing as `package` up to first **`>`**.
- **Required attributes**
  - **`name`**: CMake target name (executable name for `gz run`, etc.).
- **Optional attributes**
  - **`type`**: defaults to **`executable`**. Recognized values (case as consumed by backends—use lowercase):
    - **`executable`**
    - **`library`**: static vs shared from workspace **`GZ_TARGET_DYNAMIC_LIBRARY`** (and alias **`GZ_DYNAMIC_LIBRARY`**): when true, dynamic; when false, static—regular “policy-following” library target.
    - **`static_library`**: **forces** static; **not** overridden by the global preference.
    - **`shared_library`**: **forces** shared; **not** overridden by the global preference.
    - **`asset_bundle`**: no compile/link; install content from at least one of **`sources` / `<assets>` / `<headers>`** (see table); typical use is **`<assets>`**.
    - **`prebuilt_static_library`**: **`<prebuilt …/>`** points at a prebuilt static **`.lib` / `.a`**, etc.; path relative to **`target.xml`** dir or absolute.
    - **`prebuilt_shared_library`**: **`<prebuilt …/>`** for a prebuilt shared library; on Windows resolve **`.dll`** (**`dll=`** / **`location=`**) and import **`.lib`** (**`import_lib=`**).
  - Any **`type=`** not listed above is **rejected at `configure` time** as an **unknown** target type: stderr **`configure: unknown target type "…" for target "…"`**; **exit code 5** (**[`cli-reference.md`](cli-reference.md) §3 / §5**; see `configure.cpp`).

Type vs key child tags (current implementation):

| type | `<sources>` | `<prebuilt/>` | Notes |
|------|-------------|---------------|-------|
| `executable` / `library` / `static_library` / `shared_library` | required (≥1 source) | no | Normal compile targets; `library` resolved at configure per `GZ_TARGET_DYNAMIC_LIBRARY` |
| `asset_bundle` | may be empty | no | Need at least one of `sources` / `assets` / `<headers>` |
| `prebuilt_static_library` | not required | **required** | Paths relative to `target.xml` (or absolute) |
| `prebuilt_shared_library` | not required | **required** | Windows: dll + import lib must resolve |

#### 3.1.1 **`<prebuilt/>`** (binary **layout** metadata)

- The install root segment name for the current `gz` build comes from **configure** / the **`.intermediate/` cache** (**`arch=`** and **`compose_arch_tag`**); it is **not** duplicated on **`<prebuilt/>`**.
- **Authoring:** besides **`import_lib` / `location` / `dll`**, use the optional **split** attributes
  that mirror **`compose_arch_tag`** in **`GroundZero/lib/infra/platform/paths.cpp`**: **`os`**, **`cpu`**, **`build_system`**, **`toolchain`**, **`link`**, **`config`**, **`crt`**. Do not use a single long **`arch=…`** for new files.
- **`gz build` redistribution** **`target.xml` / schema 3 `gz_redist_manifest.json`** **emit** those split fields; the JSON has no
  extra “install directory leaf” key—only the same split fields as **`<prebuilt/>`**.
- **Read compatibility (deprecated monolithic `arch` only):** on **`<prebuilt/>`**, legacy monolithic **`arch=…`** is read only to run
  **`try_decompose_compose_arch_tag`**
  in-memory; split fields are set when recognized, and the monolith is not written back. For manifests, **schema 1** **`arch`** is handled the same way. **New files** should use split fields.

### 3.2 Source files `<file> ... </file>`

- One pair **`<file>`** … **`</file>`** per source (whitespace in tag names must match implementation regex; prefer canonical `<file>...</file>`).
- Inner text (trimmed) is a **relative path** from **`target.xml`** directory.
- Multiple `<file>` pairs append in order.

```xml
<sources>
  <file>main.cpp</file>
</sources>
```

If there is **no** **`<sources>…</sources>`** block, bare **`<file>…</file>`** anywhere in the file is collected. If **one or more** `<sources>` blocks exist, only **inside** those blocks are parsed; **outer** bare `<file>` is ignored. Multiple `<sources>` blocks merge in order (§1.1).

### 3.3 Headers input and install layout `<headers>`

**`<headers>…</headers>`** may repeat (§1.1). Entries use **`from` / `to`** self-closing tags:

- **`<dir from="..." to="..."/>`**
- **`<file from="..." to="..."/>`**
- **`<glob from="..." to="..."/>`**

Where:

- **`from`**: required, relative to `target.xml` directory.
- **`to`**: optional install subdir under install prefix **`include/`**; empty/omitted ⇒ `include/` root.
- **`when`**: optional; if false, entry is omitted from compile include inference and install rules (§3.5).

**At configure / CMake generation**:

- `dir.from` ⇒ include directory.
- `file.from` ⇒ parent directory as include dir.
- `glob.from` ⇒ glob base directory as include dir.

**Install phase (CMake)**:

- `dir` ⇒ `install(DIRECTORY … DESTINATION include/<to>)`.
- `file` ⇒ `install(FILES … DESTINATION include/<to>)`.
- `glob` ⇒ expand at configure, install files to `include/<to>` (no matches ⇒ warning).

> **Naming**: **`<headers>`** means “header sources + layout under `include/`”, not generic compiler **`-I`**. **`<includes>`** is **not** supported. Legacy nested **`<dir>path</dir>`** under `<headers>` is **not** supported—use **`<dir from="..." to="..."/>`**.

### 3.4 Compile macros `<defines>`

- Optional **`<defines>…</defines>`** (may repeat, §1.1) with self-closing **`<define name="..." value="..."/>`**.
- **`name`**: required C identifier **`[A-Za-z_][A-Za-z0-9_]*`**.
- **`value`**: optional; omitted ⇒ define name only; present ⇒ **`NAME=value`**.
- **`value` charset** (current impl): alnum and **`._+-/`** only (no spaces—for stable Ninja/MSVC command lines).

**Emission**:

- **CMake**: `target_compile_definitions(<target> PRIVATE …)` for native compile targets (`imported` / `asset_bundle` skip; **`library`** resolved first).
- **Ninja**: append **`/D…`** (MSVC `cl`) or **`-D…`** (Unix-like).

### 2.4 Package-level `<vars>` (optional)

- Block **`<vars>…</vars>`** (may repeat, §1.1) with self-closing **`<var name="KEY" value="VAL"/>`** (`value` may be omitted for empty).
- Each **KEY** is a **default** for all `target.xml` in the package (`@KEY@`, `when`); same keys may be set in **`gz configure --opt`** or **`gz_cache.txt`** (allowed key shapes: all `GZ_*` plus C identifiers except reserved cache keys—**configure applies these last**, overriding XML defaults) (§3.5 merge).

### 2.5 Package-level `<defines>` (optional)

- Same shape as target **`<defines>`** (may repeat, §1.1).
- **Scope**: every **`executable` / `library` / `static_library` / `shared_library`** in the package; applied **before** per-target `<defines>` (target macros later on the command line can override same name at the toolchain level; **`library`** resolved first).

### 2.6 Package-level `<config_files>` (optional)

- Same block shape as target (may repeat, §1.1): **`<file in="rel" to="rel"/>`** (both required).
- **`in`**: relative to **`package.xml`** directory (package root).
- **`to`**: relative to **`.intermediate/generated/<package>/_package/`** (reserved **`_package`**—avoid a compile target directory with the same name in the same package).
- **`@NAME@` / `${NAME}`**: built-ins + package `<vars>` + **all `target.xml` `<vars>` in this package** (in configure target collection order; **later** `<var>` / **later** target wins on duplicate keys) + **`--opt` / `gz_cache.txt`**. Built-in **`GZ_TARGET_NAME`** is **empty** in package templates unless a target `<var>` sets it.
- **When**: `configure` generates once per package with entries; outputs are added to **every** native compile target’s sources and **`generated/<package>/_package/`** is added to those targets’ **compile include paths** (after **`library`** resolution).

### 2.7 Script-shaped `<var type="script">` (messages / trigger)

- **Where**: **`package.xml`** and **`target.xml`** inside **`<vars>…</vars>`**, mixed with scalar **`<var name="KEY" value="VAL"/>`**.
- **Shape**: **`<var name="…" type="script" script_type="lua" trigger="…" value="…"/>`** (`script_type` optional, default `lua`; `trigger` optional, default **`manual`**).
- **Legal `trigger`**, configure dispatch points, precedence vs **`<preprocess command>`**, DOM parent walk: **`script-messages.md`** (aligned with **`script_execution.cpp`** / **`configure.cpp`**).
- **Important**: **`value`** is used as a **shell command string** today; **no** Lua bytecode execution in-process; “Lua” is type filtering / future hook.

### 3.5 Variable merge, `@KEY@`, `config_files`, and `when`

#### Variable layers (later overrides earlier)

Used for **target-level** `<config_files>` **`@NAME@` / `${NAME}`** (full stack). **Package-level** `<config_files>` uses layers **1–2**, then concatenates **each target’s layer 3** `<vars>` in configure target order into one layer before **layer 4** (workspace); **`GZ_TARGET_NAME`** is still built-in (empty at package level unless a target `<var>` sets it). Also used to evaluate **`when="..."`** on sources and headers:

1. **Built-ins**: `GZ_OS` (`windows`|`linux`|`darwin`), `GZ_PACKAGE_NAME`, `GZ_PACKAGE_VERSION`, `GZ_TARGET_NAME`, `GZ_TARGET_BUILD_SYSTEM` (`cmake`|`ninja`), `GZ_CONFIG` (`debug`|`release`).
2. **`package.xml` `<vars>`** (package defaults).
3. **`target.xml` `<vars>`** (target defaults; override package on same key).
4. **Workspace**: **`--opt`** and **`gz_cache.txt`** lines (**last wins**, same map as `GZ_*` options).

Supports **`@NAME@`** and **`${NAME}`** (`NAME` C identifier; CMake `configure_file`-like subset). **`<config_files>` templates**: before substitution, any **`@NAME@` / `${NAME}`** with `NAME` a C identifier **not yet** in the merge map is **added with empty value** and the placeholder is **removed** (no literal `${NAME}` left); set real values in **`<vars>`** or **`--opt`**. **`$<…>`** generator expressions are **not** parsed. **`when="..."`** uses only the merge map, **not** keys discovered from templates. **`packages.md`** under the build leaf summarizes `<vars>` defaults and whether a key appears in the option map for this configure.

#### Tags where `when` is evaluated (today)

- **Implemented**: **`<sources>`** `<file>`/`<glob>` with `from=` (void or paired opening tag `when`); **`<headers>`** `<dir>`/`<file>`/`<glob>` `when`.
- **Not implemented (do not rely)**: **`<assets>`**, **`<define>`**, **`<dependency/>`**, **`<config_files>`** inner `<file>`, **`<var>`**, package-level rows, **`<prebuilt/>`**, preprocess/postprocess child tags, **`<package>` / `<target>`** root attributes.
- More row-level `when` may be added product-by-product with defined semantics.

#### `when` expression grammar (matches `eval_when`, `var_subst.cpp`)

Trim ASCII whitespace, then match **exactly one** of the following (**top to bottom**; whole string must be one form):

1. **Empty** after trim ⇒ **true** (always include).
2. **Boolean literal**: whole string **`true`** or **`false`** (ASCII, case-insensitive).
3. **Comparison**: whole string **`KEY == RHS`** or **`KEY != RHS`** (spaces around `==` / `!=` allowed):
   - **`KEY`**: single C identifier; looked up in §3.5 merge map.
   - **`RHS`**: single token **`[A-Za-z0-9_.]+` only** (no spaces, quotes, `-`, `/`, etc.; for values like `dynamic-md`, use a separate key + bare truthiness, not a literal RHS).
   - Compare **ASCII-lowercased** table value for `KEY` to **ASCII-lowercased** literal `RHS`.
   - Missing **`KEY`** ⇒ treat value as **empty string** for comparison.
4. **Bare identifier**: whole string is one identifier **`[A-Za-z_][A-Za-z0-9_]*`** and **must exist** in the map; truthiness from value:
   - **False**: empty, or (case-insensitive) **`0`**, **`false`**, **`off`**, **`no`**;
   - **True**: any other value.
   - **Key missing** ⇒ **configure fails** (unknown `when`).

**Not supported**: `&&` / `||`, parentheses, function calls, substring ops, quoted free text, or anything outside the above.

When **false** (and `when` is implemented for that tag): skip that **`<sources>` / `<headers>`** row.

#### `<config_files>` (minimal `configure_file`-like subset)

- **Target-level** (`target.xml`): **`<config_files>…</config_files>`** (may repeat) with **`<file in="template.rel" to="out.rel"/>`** (both required). `in` relative to **`target.xml`** dir; `to` relative to **`.intermediate/generated/<package>/<target>/`**, safe relative path (no `..`, not absolute). **`@NAME@`** uses **full §3.5 stack**. Generated file is added to **that target’s** sources and **`generated/<package>/<target>/`** to its **compile includes**.
- **Package-level** (`package.xml`): same shape (may repeat); `in` relative to **package root**; `to` relative to **`.intermediate/generated/<package>/_package/`** (reserved **`_package`**). **`@NAME@` / `${NAME}`** use **package `<vars>` + all targets’ `<vars>`** (stack rules above) + workspace; **`GZ_TARGET_NAME`** default empty. Outputs go to **all** native compile targets’ sources and **`generated/<package>/_package/`** on their include paths.
- **`${NAME}`** (CMake `configure_file` style): same merged map; **`${` … `}`** may have inner whitespace. **Placeholder harvest**: names in template not in map are added **empty**, then alternating `@` / `${}` passes (bounded rounds), then **`#cmakedefine`**. No **`$<…>`**.
- **`#cmakedefine` / `#cmakedefine01`**: after placeholder expansion, CMake-like **subset** (`apply_cmakedefine_directives` in `var_subst.cpp`): false if missing/empty/`0`/`false`/`off`/`no`; `#cmakedefine01` ⇒ `#define NAME 0|1`; etc. For zlib-style **`zconf.h.in`**; **not** full CMake codegen.

**CMake and Ninja**: both consume the **already generated** file like any other source; the implementation does **not** emit CMake **`configure_file()``** for these rows. For full upstream `configure_file` semantics, run that **outside** gz.

#### `when` on `<sources>`

- Void **`<file from="..." when="..."/>`**, **`<glob from="..." when="..."/>`** (`from=` required).
- Or **`when`** on paired opening tag.
- If false: skip that source (glob not expanded).

### 3.6 Target dependency `<dependency name="..." visibility="..."/>`

- Self-closing **`<dependency …/>`**, may repeat.
- **`name`** forms:
  1. **`TargetName`**: intra-package library target with that name.
  2. **`PackageName:TargetName`**: cross-package; package must be declared and target must exist in the scan set.
- **`visibility`** (optional, default **`private`**, case-insensitive): **`private` / `public` / `interface`** ⇒ CMake `target_link_libraries` **`PRIVATE` / `PUBLIC` / `INTERFACE`**. **Executable** consumers **must not** use **`interface`** on a dependency (configure fails).
- **Constraint**: dependency must resolve to a library-like target (`static_library` / `shared_library` / `library` / `prebuilt_*`) or **`asset_bundle`** (graph validation only); otherwise **configure fails**.

**CMake backend (current)**:

- **Executable**: with **no** explicit `<dependency>` rows, **PRIVATE**-link **all library targets in the same package**; with explicit rows, use declared **`visibility`** (later duplicate names: later wins; sorted/deduped).
- **Library targets**: `<dependency>` participates in validation and external-package wiring; **static-to-static transitive** `target_link_libraries` chains may **not** be emitted—ensure the final exe links all needed libs (see `test_projects` examples).

---

## 4. Encoding and paths

- Prefer **UTF-8** file content (aligned with **DESIGN.md**).
- **configure** enforces **ASCII paths** where checked; non-ASCII may hard-fail—avoid.

---

## 5. Configure “primary package” and extra rules

- **Primary package**: prefer the package whose `package.xml` directory **equals cwd**; else the **first** discovered package (filesystem order—prefer single-package cwd workflows).
- Primary package needs **at least one `target.xml`** (exe/lib/import ok); **library-only** packages (e.g. only **`prebuilt_*` + `<prebuilt>`**) are allowed.
- Duplicate **`PackageName:TargetName`** in the scan set is not allowed.

---

## 6. Tests and examples

Under **`test_projects/`**—see **[test_projects/README.md](../../test_projects/README.md)**.

---

## 7. Implementation map

| Topic | Source |
|-------|--------|
| Parse `package.xml` / `target.xml` | `GroundZero/lib/engine/xml/simple_xml.cpp` (`load_package_xml` / `load_target_xml`) |
| Dependency validation, backend gen | `GroundZero/lib/engine/commands/configure.cpp` |
| Merge map, `@KEY@`, `when` | `GroundZero/lib/engine/xml/var_subst.cpp` |
| Data structures | `GroundZero/lib/engine/xml/simple_xml.hpp` (`PackageDesc` / `TargetDesc`) |

If XSD/JSON Schema is added later, document version and changelog at the top of this file.

---

## 8. Install trees, **`gz pack`**, and optional “redistribution XML” (`gz build`)

- **`gz pack` (archive only)**: archives the **install root** from **`--install-dir-name`** (**.intermediate/install/<arch>/`**, including **`bin/`**, **`lib/`**, **`include/`**, …) via **CPack** when available, else **archive** backends. It **does not** generate or rewrite **`package.xml` / `target.xml`**. If **`gz-redist/`** already exists under that install tree (see below), it is **included** in the archive. Code: **`pack.cpp`**, **`run_pack_backend`** in **`backend_dispatch.cpp`**.

- **`gz build` (redistribution XML on by default)**: after a successful **build + install**, **`gz`** reads **`.intermediate/build/<leaf>/gz_redist_manifest.json`** (written by **`gz configure`** when applicable). When the manifest exists and lists **`targets`**, it writes under the **install root** (the **`--install-dir-name`** tree): **`gz-redist/package.xml`** and **`gz-redist/<emit-name>/target.xml`**. Emitted library-like targets use **`<prebuilt …/>`** with **split layout** fields from §3.1.1. **Schema 1** manifests with only a legacy **`arch`** string are **upgraded** on read when possible. **`executable`** and **`asset_bundle`** targets are **not** included in the manifest (MVP). Disable with **`--no-emit-redistribution-xml`** or **`GZ_EMIT_REDIST_XML=0` / `false` / `off` / `no`**. Code: **`build.cpp`**, **`redist_emit.cpp`**; manifest authoring: **`configure.cpp`** (`try_write_gz_redist_manifest_json`).

- **Consumption hint**: ship **`gz-redist/`** next to **`bin/`**, **`lib/`**, **`include/`** under one install prefix; when **`gz-redist`** is used as the **package root** for scanning, its parent directory should match the **install prefix** for rebased **`<prebuilt/>`** library paths in those **`target.xml`** files.
