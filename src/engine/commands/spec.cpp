#include "spec.hpp"

#include <iostream>

namespace up {

namespace {

// English-only: embedded copy of rules aligned with doc/package-target-xml-spec.md for AI/tools without repo .md.
constexpr const char* kXmlSpecEn = R"SPEC(UP_XML_SPEC_REVISION=4

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
| package.xml | One per **package root** (same tree as targets below) | Package `name`, optional `version`, **package-level** `<dependency/>`, optional `<cmake/>`. |
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

### Headers block (`<headers>`) — compile + install layout
- Self-closing entries under `<headers>` (`<includes>` is not supported):
  - `<dir from="rel/path" to="optional_include_subdir"/>`
  - `<file from="rel/path.hpp" to="optional_subdir"/>`
  - `<glob from="rel/*.hpp" to="optional_subdir"/>`
- `from` is required, relative to `target.xml` directory. `to` is optional (under install prefix `include/`).

### Compile definitions (`<defines>`)
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

## 6. Minimal examples

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

---

## 7. Implementation pointers (for humans maintaining `up`)

| Topic | Source |
|-------|--------|
| Load/parse XML | src/engine/xml/simple_xml.cpp |
| Types | src/engine/xml/simple_xml.hpp (`PackageDesc`, `TargetDesc`; `TargetDesc::defines`) |
| Configure validation / graph | src/engine/commands/configure.cpp |

End of embedded spec.
)SPEC";

}  // namespace

int cmd_spec(const std::vector<std::string>& args) {
  (void)args;
  std::cout << kXmlSpecEn;
  return 0;
}

}  // namespace up
