#include "spec.hpp"

#include <iostream>

namespace up {

namespace {

// English-only: embedded copy of rules aligned with doc/package-target-xml-spec.md for AI/tools without repo .md.
// Split into named chunks: MSVC ~16kB string literal limit; edit the slice you need.
constexpr const char* kXmlSpecEnThroughSection3 =
  R"SPEC(UP_XML_SPEC_REVISION=16

# up — package.xml and target.xml (machine-oriented summary)

This text is shipped inside `up.exe`. It describes what the **current** `up` implementation expects when you author
`package.xml` and `target.xml`. The parser is a **lightweight regex scanner**, not a full XML validator: write well-formed
XML and follow the shapes below.

After authoring XML, always validate with:

  up configure [--scan <dir>]... [--opt KEY=VALUE]...
  up list [--format tree|json|xml] [--xml <path>] [--json <path>] [--quiet]

`configure` is the source of truth for dependency and path errors.

`up list` output behavior:
- stdout payload is selected by `--format` (`tree` default, or `json`/`xml`).
- `--xml <path>` / `--json <path>` are file exports and can be combined with any stdout format.
- `--quiet` suppresses tree text and export hint lines; it does not suppress payload for `--format json|xml`.
- warning-only combinations (do not fail execution):
  - `--format xml` + `--json <path>`: warns stdout is XML while JSON is file-only.
  - `--format json` + `--xml <path>`: warns stdout is JSON while XML is file-only.

---

## 1. Layout

| File | Location | Role |
|------|----------|------|
)SPEC"
#if !UP_DISABLE_PACKAGE_XML_CMAKE
  R"SPEC(| package.xml | One per **package root** (same tree as targets below) | Package `name`, optional `version`, **package-level** `<dependency/>`, optional `<cmake/>`, optional `<vars>` / `<defines>`. |
)SPEC"
#else
  R"SPEC(| package.xml | One per **package root** (same tree as targets below) | Package `name`, optional `version`, **package-level** `<dependency/>`, optional `<vars>` / `<defines>`. (Package `<cmake/>` is **disabled** in this `up.exe` — **UP_DISABLE_PACKAGE_XML_CMAKE**.) |
)SPEC"
#endif
  R"SPEC(| target.xml | **Exactly one** per **target directory** (each target lives in its own subdirectory) | Target `name`, `type`, sources, optional `<headers>`/assets, **target-level** `<dependency/>`. |

Rules:
- Every `target.xml` must sit **under** some `package.xml` directory tree. `configure` assigns each target to the
  nearest parent `package.xml` ("nearest_package_parent" behavior).
- `up configure` recursively scans from cwd and/or each `--scan` root for `package.xml` and `target.xml`.

---

## 2. package.xml

### Root element
- First occurrence of `<package` … up to the first `>` is treated as the header.
- **Required attribute:** `name` — globally unique among all packages in one configure scan.
- **Optional attribute:** `version` — if omitted, implementation defaults to `0.0.0`.

### Package dependencies
- Self-closing tags: `<dependency name="other_pkg" optional="true|false"/>`
- `name` is another **package** name that must appear in the same scan (unless `optional` is true / 1 / yes).
- If a `target.xml` uses `OtherPkg:SomeLib`, that `OtherPkg` **must** be listed here (non-optional), or configure fails.

)SPEC"
#if !UP_DISABLE_PACKAGE_XML_CMAKE
  R"SPEC(

### Optional native CMake subtree
- `<cmake source_dir="relative/path"/>` — directory (relative to **package.xml parent**) containing upstream
  `CMakeLists.txt`. Used by the CMake backend when generating the aggregate project.

)SPEC"
#else
  R"SPEC(

### Package.xml `<cmake/>` (compile-time disabled in this binary)

- **UP_DISABLE_PACKAGE_XML_CMAKE** is **ON**: `package.xml` **`<cmake/>` is not parsed**; configure does **not** wire
  ExternalProject / upstream CMake from that tag. Rebuild `up` with **`-DUP_DISABLE_PACKAGE_XML_CMAKE=OFF`** to enable.

)SPEC"
#endif
  R"SPEC(### Optional package variables `<vars>`
- Block: `<vars>...</vars>` with self-closing entries `<var name="KEY" value="VAL"/>` (`value` may be omitted for empty).
- Each pair is a **default** for `KEY` for all targets in the package (for `@KEY@` / `when=`); the same key may be
  **overridden** at configure time (see merge order below).
- Script var (special var type): `<var name="SCRIPT_NAME" type="script" script_type="lua" trigger="manual|sources.preprocess|sources.postprocess|headers.preprocess|headers.postprocess|assets.preprocess|assets.postprocess" value="..."/>`
  - `script_type` defaults to `lua`; `trigger` defaults to `manual`.
  - Unsupported `trigger` is a parse error at configure/list load time.

### Workspace overrides (`--opt` / `up_cache.txt`)

- `up configure --opt KEY=value` (or `KEY=value` lines in `up_cache.txt`) supplies entries merged into the same option
  map as `UP_*` build switches.
- Keys accepted into that map: any **`UP_*`** or **`UPSTREAM_*`**, plus any **C identifier** (`[A-Za-z_][A-Za-z0-9_]*`)
  except reserved cache metadata (`cwd`, `arch`, `package`, `generated_file`, `scan_roots`, `up.cache.version`).
- These entries apply **after** package and target `<vars>` in the template/`when` merge, so they **override** XML
  defaults for the same name.

### Optional package compile definitions `<defines>`
- Same shape as target **`<defines>`** (see below): `<defines>...</defines>` with `<define name="IDENT" value="..."/>`.
- Applied to **every** `executable` / `static_library` / `shared_library` target in this package **before** that target’s
  own `<defines>` entries (so target-level definitions follow and may override at the toolchain level).

### Optional package config files `<config_files>`
- Same block shape as target **`<config_files>`**: `<config_files>...</config_files>` with **`<file in="rel" to="out"/>`**
  (both required).
- `in` is relative to the **`package.xml` parent directory** (package root).
- `to` is relative to **`.intermediate/generated/<package>/_package/`** (reserved directory name **`_package`**; avoid
  naming a compile target `_package` in the same package). Same safe-path rules as target config: no `..`, not absolute.
- **`@NAME@` / `${NAME}` substitution map:** builtins + package `<vars>` + **every `target.xml` `<vars>` in this package**
  (flattened in `configure` **build_targets** order; **later** `<var>` / **later** target wins on duplicate keys) +
  workspace options. Builtin **`UP_TARGET_NAME` is empty** unless a target `<var>` sets it.
- Generated files are written **once per configure** per package and appended to **every** native compile target in that
  package as extra sources. **`.intermediate/generated/<package>/_package/`** is added to compile **include directories**
  for every `executable` / `static_library` / `shared_library` in the package whenever any package-level `<config_files>`
  entry exists.

---

## 2b. Variable merge order (builtin / package / target / workspace)

Used for **`@KEY@`** and **`${KEY}`** substitution in **target-level** `<config_files>` templates (full stack below),
**package-level** `<config_files>` (builtins + package `<vars>` + **all targets' `<vars>` in the package** in build order,
then workspace; builtin `UP_TARGET_NAME` empty unless overlaid), and for evaluating `when="..."` on sources and headers.

**Layers (later overrides earlier):**

1. **Builtins** (names from configure context): `UP_OS` (`windows` | `linux` | `darwin`),
   `UP_PACKAGE_NAME`, `UP_PACKAGE_VERSION`, `UP_TARGET_NAME`, `UP_TARGET_BUILD_SYSTEM` (`cmake` | `ninja`),
   `UP_CONFIG` (`debug` | `release`).
2. **Package `<vars>`** from `package.xml` (defaults for the whole package).
3. **Target `<vars>`** from `target.xml` (defaults for that target; same key replaces the package default).
4. **Workspace options**: keys from `--opt` and `up_cache.txt` (same map as `UP_*` switches, plus extra identifiers;
   **overrides** XML defaults for the same key).

**Template syntax:** **`@NAME@`** and **`${NAME}`** in **`when="..."`** use the merged map only; `NAME` must be a C
identifier. **`$<...>`** generator expressions are not interpreted.

For **`<config_files>`** templates only: before substitution, every **`@NAME@`** and **`${NAME}`** in the template text
where `NAME` is a C identifier and **`NAME` is missing** from the merged map is **added to the map with an empty value**,
so the placeholder is **removed** (replaced by nothing). Set real values in **`<vars>`** or **`--opt`** when empty is
wrong (e.g. `typedef ${ZIP_INT8_T} …`).

### `when` attribute — where it is supported today

- **Implemented:** optional `when="..."` on **`<sources>`** entries (`<file …/>`, `<glob …/>`, and paired opening tags with
  `from=`) and on **`<headers>`** entries (`<dir/>`, `<file/>`, `<glob/>`).
- **Not implemented (ignored if present; do not rely on it):** `<assets>`, `<define>`, `<dependency/>`, `<config_files>`,
  `<var>`, package-level rows, `<prebuilt/>`, `<install/>`, preprocess/postprocess tags, and the `<target>` / `<package>`
  roots themselves.

Uniform `when` on every repeatable row is a reasonable **product direction**, but each construct needs a defined meaning
(e.g. skipping a `<dependency>` vs failing resolution; skipping `<define>` vs compile flags) and extra configure work.

### `when` expression grammar (matches `eval_when` in `src/lib/engine/xml/var_subst.cpp`)

The string is **trimmed** of leading/trailing ASCII whitespace, then evaluated as **exactly one** of the following forms
(in this order):

1. **Empty** after trim — treated as **true** (always include the item).
2. **Boolean literal:** whole string is `true` or `false` (ASCII letters; compared case-insensitively) — **true** or
   **false**.
3. **Comparison:** must match the **entire** string (after trim), pattern  
   `KEY == RHS` or `KEY != RHS` with optional spaces around `==` / `!=`, where:
   - **`KEY`** is a single C-like identifier: `[A-Za-z_][A-Za-z0-9_]*` (matches keys in the merged map: builtins,
     package `<vars>`, target `<vars>`, `--opt` / `up_cache.txt`).
   - **`RHS`** is a **single token** `[A-Za-z0-9_.]+` only (no spaces, no quotes, no `/`, no `-` unless you encode them
     outside this grammar — use another `KEY` or a different expression form).
   - The variable’s value and `RHS` are compared as **ASCII lowercased** strings for `==` / `!=`.
   - If `KEY` is missing from the map, its value is treated as **empty** for the comparison.
4. **Bare identifier:** the whole string is **one** identifier `[A-Za-z_][A-Za-z0-9_]*` that **must exist** as a key in the
   merged map; the result is **truthy** or **falsy** on the stored value:
   - **Falsy:** empty, or (case-insensitive) `0`, `false`, `off`, `no`.
   - **Truthy:** any other value.
   - If the key is **not** in the map — **configure error** (unknown `when`).

**Not supported:** logical combinators (`&&`, `||`), parentheses, function calls, substring match, or arbitrary text
outside the forms above.

---

## 3. target.xml

### Root element
- First `<target` … up to first `>`.
- **Required attribute:** `name` — target id (executable name for `up run`, etc.).
- **Optional attribute:** `type` — if omitted, defaults to `executable`.

### Supported `type` values (use lowercase)

| type | `<sources>` / `<file>` | `<prebuilt/>` | `<install …/>` (artifact) | Notes |
|------|------------------------|---------------|----------------------------|-------|
| executable | required (>=1 source) | no | no | Normal compile target. |
| static_library | required | no | no | |
| shared_library | required | no | no | |
| asset_bundle | may be empty | no | no | Need at least one of sources / assets / `<headers>`. |
| imported_static_library | not required | **required** | no | Paths relative to `target.xml` dir unless absolute. |
| imported_shared_library | not required | **required** | no | On Windows, dll + import `.lib` must be resolvable. |
| imported_installed_static_library | not required | no | **required** | `artifact` relative to `CMAKE_INSTALL_PREFIX` after upstream install. |
| imported_installed_shared_library | not required | no | **required** | Windows: need `implib` for the import library. |

### Sources
- Repeat: `<file>relative/path.cpp</file>` — path is **relative to the directory that contains this target.xml**.
- Optional wrapper: `<sources>…</sources>` is for readability only; the parser matches `<file>…</file>` anywhere.
- Under `<sources>`, you may also use self-closing **`<file from="rel.cpp" when="..."/>`** or **`<glob from="*.cpp" when="..."/>`**
  (requires `from=`). Paired `<file from="...">…</file>` / `<glob …>` still support optional **`when="..."`** on the
  opening tag.
- Optional **`<vars>...</vars>`** (same shape as package vars): **defaults** for that target; same key overrides the
  package default, and may still be overridden last by `--opt` / `up_cache.txt`.
- Script vars are also allowed at target level with the same `type="script"` + `script_type` + `trigger` + `value` shape.

### Config files (`<config_files>`) — target-level configure-time templates

- Block: `<config_files>...</config_files>` with **`<file in="template.rel" to="out.rel"/>`** (both required).
- `in` is relative to **`target.xml` directory**; `to` is relative to **`.intermediate/generated/<package>/<target>/`** and
  must be a safe relative path (no `..` segments, not absolute).
- During **configure**, `up` reads each template, **extends** the merged variable map with **default-empty** entries for
  every **`@NAME@`** / **`${NAME}`** placeholder in the file that is not already in the map (see §2b template syntax),
  then applies **`@NAME@`** and **`${NAME}`** substitution, alternating both passes until nothing changes (max 64 rounds),
  using the **full** merged variable map for target-level templates, then
  writes the output under the generated directory, and adds that file to the target’s compile sources. The target’s
  generated directory is also added as an **include directory** for `executable` / `static_library` / `shared_library`
  targets.
- After `@` / `${}` replacement, lines **`#cmakedefine NAME ...`** and **`#cmakedefine01 NAME`** are expanded like CMake
  `configure_file` (subset): unknown / empty / `0` / `false` / `off` / `no` treat as false; `#cmakedefine01` becomes
  **`#define NAME 0|1`**. This helps zlib-style `zconf.h` templates; it is **not** a full CMake `configure_file` engine.

)SPEC"
#if !UP_DISABLE_PACKAGE_XML_CMAKE
  R"SPEC(**Backends (CMake vs Ninja):** both backends consume the **already generated** file as a normal source path. `up` does
**not** emit CMake `configure_file()` for these entries; both backends treat the output like any other source file. For
  full upstream CMake `configure_file` semantics, keep using a native `<cmake/>` subtree.

)SPEC"
#else
  R"SPEC(**Backends (CMake vs Ninja):** both backends consume the **already generated** file as a normal source path. `up` does
**not** emit CMake `configure_file()` for these entries; both backends treat the output like any other source file.

)SPEC"
#endif
  R"SPEC(### Headers block (`<headers>`) — compile + install layout
- Self-closing entries under `<headers>` (`<includes>` is not supported):
  - `<dir from="rel/path" to="optional_include_subdir"/>`
  - `<file from="rel/path.hpp" to="optional_subdir"/>`
  - `<glob from="rel/*.hpp" to="optional_subdir"/>`
- `from` is required, relative to `target.xml` directory. `to` is optional (under install prefix `include/`).
- Optional **`when="..."`** on each entry: if false, the entry is omitted from compile include paths and from install rules.

### Compile definitions (`<defines>`) — target
- Optional block: `<defines>...</defines>` containing self-closing `<define name="IDENT" value="..."/>`.
- `name` is required (C identifier: `[A-Za-z_][A-Za-z0-9_]*`). `value` is optional; omitted means define the macro name only.
- `value` (if present) may only contain letters, digits, and `._+-/` (no spaces).
- Applied to native compile targets (`executable`, `static_library`, `shared_library`): CMake uses
  `target_compile_definitions(... PRIVATE ...)`, Ninja appends `/D` or `-D` flags to compile commands.

### Target dependencies
- `<dependency name="LocalLib"/>` — depends on another **library** target in the **same** package (same `name`).
- `<dependency name="OtherPkg:TheirLib"/>` — cross-package; `OtherPkg` must be in `package.xml` `<dependency/>` and
  `TheirLib` must exist in the scan set.
- **Optional attribute:** `visibility="private|public|interface"` (default **`private`**, ASCII case-insensitive on
  parse). Maps to CMake `target_link_libraries` link keywords for **executable** consumers when explicit `<dependency/>`
  rows are used: **`PRIVATE`** / **`PUBLIC`** / **`INTERFACE`**. **`interface` is rejected** when the consumer target is
  an **`executable`** (an executable must actually link the library; use `private` or `public`).
- Valid dependency target types are library-like (`static_library`, `shared_library`, imported_* library types).
  Configure fails if the reference cannot be resolved or points to a non-library.

CMake backend note (current behavior):
- **Executable** targets also **PRIVATE-link all library targets in the same package** when no explicit `<dependency/>`
  link rows exist; otherwise explicit rows (with their `visibility`) are used for `target_link_libraries`.
- **Library** targets: `<dependency/>` participates in validation and external-package library wiring; static-to-static
  **transitive** `target_link_libraries` chaining may **not** be generated—ensure the final executable links all needed
  libs if symbols must be pulled from multiple static libs.

---
)SPEC";

constexpr const char* kXmlSpecEnSections4And5 = R"SPEC(

## 4. Encoding and paths

- Prefer UTF-8 files.
- **ASCII-only paths** for scanned paths are enforced in places (non-ASCII paths may hard-fail configure).

---

## 5. Primary package selection (multi-package scans)

- Prefer the package whose `package.xml` directory equals **cwd**.
- Otherwise the **first** discovered package may be treated as primary (filesystem order). Prefer single-package cwd
  workflows when possible.

---
)SPEC";

constexpr const char* kXmlSpecEnSections6Through7 =
  R"SPEC(

## 6. Examples

### 6a. Minimal (typical app + dependency)

package.xml:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<package name="my_app" version="0.1.0">
  <dependency name="my_sdk" optional="false"/>
)SPEC"
#if !UP_DISABLE_PACKAGE_XML_CMAKE
  R"SPEC(  <cmake source_dir="."/>
)SPEC"
#endif
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

### 6b. Full-tag reference sketches (syntax only; trim for real trees)

These snippets are **not** one runnable layout: they show **every major child shape** the scanner recognizes. Omit
blocks you do not need. Some combinations are **mutually exclusive by `type`** (e.g. `imported_*` vs normal `<sources>`).

**package.xml — all supported top-level constructs (except duplicate `<dependency/>` rows shown as one each):**

```xml
<?xml version="1.0" encoding="UTF-8"?>
<package name="demo_pkg" version="1.0.0">
  <dependency name="other_pkg" optional="false"/>
  <dependency name="maybe_pkg" optional="true"/>
)SPEC"
#if !UP_DISABLE_PACKAGE_XML_CMAKE
  R"SPEC(  <cmake source_dir="vendor/upstream_cmake"/>
)SPEC"
#endif
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

**target.xml — native compile target** (`executable` | `static_library` | `shared_library`): `<vars>`, `<config_files>`,
`<defines>`, `<sources>` (`<file>text</file>`, wrapper block, self-closing `from=` / `when=`, paired tags with
`<preprocess command="..."/>` / `<postprocess command="..."/>`), `<headers>` (`dir` / `file` / `glob`, optional `to`,
optional `when`), `<assets>` (same `from` / `to` / preprocess / postprocess pattern; **`when` not implemented**),
`<dependency/>`:

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
    <file from="single.cpp" when="UP_OS==windows"/>
    <glob from="src/*.cpp" when="true"/>
    <file from="gen.cpp">
      <preprocess command="touch gen.cpp.stamp || exit 0"/>
      <postprocess command=""/>
    </file>
  </sources>
  <headers>
    <dir from="include" to="demo"/>
    <file from="include/demo/single.hpp" to="demo"/>
    <glob from="include/demo/*.hpp" to="demo" when="UP_OS==linux"/>
  </headers>
  <assets>
    <dir from="assets" to="share/demo"/>
    <file from="assets/readme.txt" to="share/demo"/>
    <glob from="assets/*.md" to="share/demo"/>
  </assets>
  <dependency name="other_pkg:their_lib" visibility="public"/>
</target>
```

**target.xml — `imported_static_library` / `imported_shared_library`** (uses `<prebuilt/>`, no compile `<sources>`):

```xml
<?xml version="1.0" encoding="UTF-8"?>
<target name="sdk_stub" type="imported_static_library">
  <prebuilt import_lib="lib/third_party.lib"/>
</target>
```

**target.xml — `imported_installed_*`** (uses `<install …/>` and optional `<interface_include/>`):

```xml
<?xml version="1.0" encoding="UTF-8"?>
<target name="from_cmake" type="imported_installed_static_library">
  <install artifact="lib/foo.lib"/>
  <interface_include dir="include"/>
</target>
```

**target.xml — `asset_bundle`** (may omit compile sources; needs sources and/or `<headers>` and/or `<assets>` per rules above.)

---

## 7. Implementation pointers (for humans maintaining `up`)

| Topic | Source |
|-------|--------|
| DOM model / script execution context | src/lib/engine/dom/dom_model.hpp / src/lib/engine/dom/dom_model.cpp |
| Load/parse XML | src/lib/engine/xml/simple_xml.cpp |
| Types | src/lib/engine/xml/simple_xml.hpp (`PackageDesc`, `TargetDesc`, `ConfigFileEntry`, `DefineEntry`) |
| Configure validation / graph | src/lib/engine/commands/configure.cpp |
| Variable merge + `@` / `${}` + `when` + `#cmakedefine` (config_files) | src/lib/engine/xml/var_subst.cpp |

End of embedded spec.
)SPEC";

}  // namespace

int cmd_spec(const std::vector<std::string>& args) {
  (void)args;
  std::cout << kXmlSpecEnThroughSection3 << kXmlSpecEnSections4And5 << kXmlSpecEnSections6Through7;
  return 0;
}

}  // namespace up
