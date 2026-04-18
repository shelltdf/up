#include "spec.hpp"

#include <iostream>

namespace up {

namespace {

// English-only: embedded copy of rules aligned with doc/package-target-xml-spec.md for AI/tools without repo .md.
constexpr const char* kXmlSpecEn = R"SPEC(UP_XML_SPEC_REVISION=8

# up — package.xml and target.xml (machine-oriented summary)

This text is shipped inside `up.exe`. It describes what the **current** `up` implementation expects when you author
`package.xml` and `target.xml`. The parser is a **lightweight regex scanner**, not a full XML validator: write well-formed
XML and follow the shapes below.

After authoring XML, always validate with:

  up configure [--scan <dir>]... [--opt KEY=VALUE]...

`configure` is the source of truth for dependency and path errors.

---

## 1. Layout

| File | Location | Role |
|------|----------|------|
| package.xml | One per **package root** (same tree as targets below) | Package `name`, optional `version`, **package-level** `<dependency/>`, optional `<cmake/>`, optional `<vars>` / `<defines>`. |
| target.xml | **Exactly one** per **target directory** (each target lives in its own subdirectory) | Target `name`, `type`, sources, optional `<headers>`/assets, **target-level** `<dependency/>`. |

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

### Optional native CMake subtree
- `<cmake source_dir="relative/path"/>` — directory (relative to **package.xml parent**) containing upstream
  `CMakeLists.txt`. Used by the CMake backend when generating the aggregate project.

### Optional package variables `<vars>`
- Block: `<vars>...</vars>` with self-closing entries `<var name="KEY" value="VAL"/>` (`value` may be omitted for empty).
- Each pair is a **default** for `KEY` for all targets in the package (for `@KEY@` / `when=`); the same key may be
  **overridden** at configure time (see merge order below).

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

---

## 2b. Variable merge order (builtin / package / target / workspace)

Used for `@KEY@` substitution in `<config_files>` templates and for evaluating `when="..."` on sources and headers.

**Layers (later overrides earlier):**

1. **Builtins** (names from configure context): `UP_OS` (`windows` | `linux` | `darwin`),
   `UP_PACKAGE_NAME`, `UP_PACKAGE_VERSION`, `UP_TARGET_NAME`, `UP_TARGET_BUILD_SYSTEM` (`cmake` | `ninja`),
   `UP_CONFIG` (`debug` | `release`).
2. **Package `<vars>`** from `package.xml` (defaults for the whole package).
3. **Target `<vars>`** from `target.xml` (defaults for that target; same key replaces the package default).
4. **Workspace options**: keys from `--opt` and `up_cache.txt` (same map as `UP_*` switches, plus extra identifiers;
   **overrides** XML defaults for the same key).

**Template syntax:** only **`@NAME@`** is replaced (CMake-style); unknown names are left unchanged.

**`when` semantics:** empty `when` means always. Otherwise: `true` / `false`; `KEY==token` or `KEY!=token` (ASCII
case-insensitive for the comparison); or a bare variable name — must exist in the merged map, then truthiness applies
(`0`, `false`, `off`, `no`, empty => false). Invalid bare expressions (unknown identifier) => **configure error**.

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

### Config files (`<config_files>`) — configure-time templates

- Block: `<config_files>...</config_files>` with **`<file in="template.rel" to="out.rel"/>`** (both required).
- `in` is relative to `target.xml` directory; `to` is relative to **`.intermediate/generated/<package>/<target>/`** and
  must be a safe relative path (no `..` segments, not absolute).
- During **configure**, `up` reads each template, applies **`@NAME@`** substitution using the merged variable map, writes
  the output under the generated directory, and adds that file to the target’s compile sources. The generated directory
  is also added as an **include directory** for `executable` / `static_library` / `shared_library` targets.

**Backends (CMake vs Ninja):** both backends consume the **already generated** file as a normal source path. `up` does
**not** emit CMake `configure_file()` for these entries; both backends treat the output like any other source file. For
  full upstream CMake `configure_file` semantics, keep using a native `<cmake/>` subtree.

### Headers block (`<headers>`) — compile + install layout
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
- Valid dependency target types are library-like (`static_library`, `shared_library`, imported_* library types).
  Configure fails if the reference cannot be resolved or points to a non-library.

CMake backend note (current behavior):
- **Executable** targets also **PRIVATE-link all library targets in the same package** in addition to explicit
  `<dependency/>` entries.
- **Library** targets: `<dependency/>` participates in validation and external-package library wiring; static-to-static
  **transitive** `target_link_libraries` chaining may **not** be generated—ensure the final executable links all needed
  libs if symbols must be pulled from multiple static libs.

---

## 4. Encoding and paths

- Prefer UTF-8 files.
- **ASCII-only paths** for scanned paths are enforced in places (non-ASCII paths may hard-fail configure).

---

## 5. Primary package selection (multi-package scans)

- Prefer the package whose `package.xml` directory equals **cwd**.
- Otherwise the **first** discovered package may be treated as primary (filesystem order). Prefer single-package cwd
  workflows when possible.

---

## 6. Examples

### 6a. Minimal (typical app + dependency)

package.xml:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<package name="my_app" version="0.1.0">
  <dependency name="my_sdk" optional="false"/>
  <cmake source_dir="."/>
</package>
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
  <cmake source_dir="vendor/upstream_cmake"/>
  <vars>
    <var name="MY_DEFAULT" value="from_package_xml"/>
    <var name="FLAG_ONLY"/>
  </vars>
  <defines>
    <define name="DEMO_PKG_MACRO" value="1"/>
    <define name="DEMO_PKG_ONLY"/>
  </defines>
</package>
```

**target.xml — native compile target** (`executable` | `static_library` | `shared_library`): `<vars>`, `<config_files>`,
`<defines>`, `<sources>` (legacy `<file>text</file>`, wrapper block, self-closing `from=` / `when=`, paired tags with
`<preprocess command="..."/>` / `<postprocess command="..."/>`), `<headers>` (`dir` / `file` / `glob`, optional `to`,
optional `when`), `<assets>` (same `from` / `to` / preprocess / postprocess pattern, **no** `when`), `<dependency/>`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<target name="demo_lib" type="static_library">
  <vars>
    <var name="MY_DEFAULT" value="from_target_xml"/>
  </vars>
  <config_files>
    <file in="version.hpp.in" to="generated/version.hpp"/>
  </config_files>
  <defines>
    <define name="DEMO_TARGET" value="1"/>
  </defines>
  <sources>
    <file>legacy_path.cpp</file>
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
  <dependency name="other_pkg:their_lib"/>
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
| Load/parse XML | src/engine/xml/simple_xml.cpp |
| Types | src/engine/xml/simple_xml.hpp (`PackageDesc`, `TargetDesc`, `DefineEntry`) |
| Configure validation / graph | src/engine/commands/configure.cpp |
| Variable merge + `@KEY@` + `when` | src/engine/xml/var_subst.cpp |

End of embedded spec.
)SPEC";

}  // namespace

int cmd_spec(const std::vector<std::string>& args) {
  (void)args;
  std::cout << kXmlSpecEn;
  return 0;
}

}  // namespace up
