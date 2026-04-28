# Getting started: Hello World through third-party libraries and migration

This tutorial grows a `gz` project **step by step**; where it matches **`test_projects/`**, pointers are given. Field-level semantics: **`package-target-xml-spec.md`** and **`gz spec`**; **argv / exit codes / examples**: **`cli-reference.md`** and repo-root **`README.md`**; **doc map, FAQ, gz-gui**: **`user-manual.md`**. **`doc/` index**: [`../README.md`](../README.md). This file is the **full English** edition; optional **Simplified Chinese** mirror: [`../zh/getting-started.md`](../zh/getting-started.md).

---

## 0. Conventions and doc map

- **Package**: directory containing **`package.xml`** is the package root; each build target lives in **its own subdirectory** with **`target.xml`**.
- **Scan**: run **`gz configure`** from the **parent** of a tree that contains `package.xml`, or from any cwd with **`--scan`** pointing at such a tree.
- **Build / run**: **`gz build`** requires **`--build-dir-name`**; **`gz run`** requires **`--install-dir-name`** (value is the **architecture subdirectory name** under `.intermediate/install/`, see **`script-tutorial.md` §1**).
- **`gz` path**: this tutorial assumes **`gz` is on PATH**. If you build from repo **`README.md` / `user-manual.md`** and see paths like **`.\_build\Release\gz.exe`**, **`_build`** is only an example **`cmake -B`** directory (could be **`_build_gz`**, etc.); see **`user-manual.md`** **§0 Quick entry and build-directory note**.
- **Product direction**: **hand-write** **`package.xml` / `target.xml`** (see **`package-target-xml-spec.md`** intro). **`gz reverse`** is removed.
- **Further reading**: [cli-reference.md](cli-reference.md), [internal-variables.md](internal-variables.md), [script-messages.md](script-messages.md), [script-tutorial.md](script-tutorial.md), [package-target-xml-spec.md](package-target-xml-spec.md) (all full English under **`doc/en/`**).

---

## Step 1: Hello World (single package, single executable)

Layout:

```text
hello_pkg/
  package.xml
  app/
    target.xml
    main.cpp
```

**`package.xml`**

```xml
<?xml version="1.0" encoding="UTF-8"?>
<package name="hello_pkg" version="0.1.0">
</package>
```

**`app/target.xml`**

```xml
<?xml version="1.0" encoding="UTF-8"?>
<target name="hello" type="executable">
  <sources>
    <file>main.cpp</file>
  </sources>
</target>
```

**`app/main.cpp`**: `int main() { return 0; }` (or print a line).

Run: `gz configure` → `gz build --build-dir-name default` → `gz print-build-dir-name --build-dir-name default` → `gz run --install-dir-name <arch-name> hello`.

---

## Step 2: Add a static library and link it from the executable

Add subdirectory **`mylib/`** in the same package, type **`static_library`** (for **global** static vs shared switching, use **`type="library"`**, see **`package-target-xml-spec.md` §3.1**). The executable links via **`<dependency name="libTargetName"/>`** or default behavior (below).

**CMake generation semantics (today)**:

- If the executable has **no** `<dependency>` at all, configure **auto-links qualifying libraries in the same package** per **`GZ_TARGET_DYNAMIC_LIBRARY`**, etc.; if **any** explicit `<dependency>` exists, **only** those links are used (see **`package-target-xml-spec.md` §3.6**).
- Library **`<headers>`** and **target-level `<config_files>`** output dirs join that library’s **include path**; for **`static_library` / `shared_library`** (and **`library`** resolved to one of them), CMake uses **`target_include_directories(... PUBLIC ...)`**, so executables that **link** the library can usually **`#include`** public headers and that library’s `config_files` outputs (see **`test_projects/hello_simple_lib/`**).

Reference: `test_projects/hello_simple_lib/` (package `defines` / `config_files` + static lib + tool exe + tests).

**`mylib/target.xml` skeleton**

```xml
<?xml version="1.0" encoding="UTF-8"?>
<target name="mylib" type="static_library">
  <sources>
    <file>mylib.cpp</file>
  </sources>
  <headers>
    <dir from="."/>
  </headers>
</target>
```

**`app/target.xml` add dependency**

```xml
<target name="hello" type="executable">
  <sources>
    <file>main.cpp</file>
  </sources>
  <dependency name="mylib"/>
</target>
```

---

## Step 3: Add compile macros `<defines>`

- **Package-level** `<defines>...</defines>` in `package.xml`: applies to all **native compile** targets in the package (merged before target-level; target can override same name).
- **Target-level** `<defines>` in `target.xml`: that target only; **`value`** charset: alnum and **`._+-/`** (no spaces), see spec §3.4.

Example:

```xml
<defines>
  <define name="USE_FEATURE"/>
  <define name="APP_VERSION" value="1.0.0"/>
</defines>
```

---

## Step 4: Add `config_files` (generated headers)

Two placement rules (**do not mix**):

| Location | `in` relative to | Output under `.intermediate/generated/` | Who receives output |
|----------|------------------|-------------------------------------------|------------------------|
| **Package** `package.xml` | Package root | `<packageName>/_package/` | **Every** exe/static/shared **library** target in the package (**sources + include**) |
| **Target** `target.xml` | That `target.xml` directory | `<packageName>/<targetName>/` | **That target only**; others need **`<dependency>`** to a library that **PUBLIC**-exports includes, etc. |

Templates use **`@NAME@` / `${NAME}`** and optional **`#cmakedefine`**; merge stack: spec §3.5.

Examples: `test_projects/hello_simple_lib/pkg_gen.hpp.in` (package), `hello_simple_lib/hello_simple_lib_gen.hpp.in` (target).

---

## Step 5: OS / config conditions (`when` + built-ins)

- **`when`** only on **implemented** tags (**`<sources>`** `<file>`/`<glob>`, **`<headers>`** dir/file/glob); **do not** rely on `<defines>`, `<assets>`, etc. (see spec §3.5 “not implemented”).
- Common built-ins: **`GZ_OS`**, **`GZ_CONFIG`**, **`GZ_TARGET_BUILD_SYSTEM`**, etc.: **`internal-variables.md`**.
- **Syntax**: no `&&` / `||`; use **`KEY == token`**, **`KEY != token`**, or **single-key** truthiness.

Example: compile a file only on Windows:

```xml
<file from="win_only.cpp" when="GZ_OS==windows"/>
```

---

## Step 6: Cross-package dependencies (multi-package scan)

1. Child package directory has its own **`package.xml`** (different **`name`**).
2. Parent **`package.xml`** lists **`<dependency name="childPackageName"/>`** (optional **`optional="true"`**).
3. Library target **`target name="bar"`** in child; parent executable uses **`<dependency name="childPackageName:bar"/>`**.

Example: **`test_projects/hello_parent_child/`**.

---

## Step 7: Third-party — prebuilt binaries (`prebuilt_*` + `<prebuilt>`)

When the vendor ships **`.lib` / `.a` / `.dll`+.lib / `.so`** and you do not compile from source.

1. New target **`type="prebuilt_static_library"`** or **`prebuilt_shared_library`**.
2. Fill **`<prebuilt .../>`**: static often **`import_lib=`** or **`location=`**; Windows **shared** needs **`dll=`** (or **`location=`** to `.dll`) and **`import_lib=`** (`.lib`). See **`package-target-xml-spec.md`** type table.
3. **`<headers>`** for third-party include dirs.
4. Executable **`<dependency name="thatImportTarget"/>`**.

Example: **`test_projects/prebuilt_import_demo/`**.

**Note**: paths resolve relative to **`target.xml`** directory; check **architecture** and **license** for checked-in binaries.

---

## Step 8: Third-party — fold an out-of-tree `install` into the package (**`prebuilt_*`**)

When upstream was **`cmake --install`** (or an official SDK tree) and you want a **reproducible** layout in this repo: **copy or symlink** the needed **`lib/`**, **`include/`**, **`.dll`**, etc. into the package (e.g. under **`vendor/…`**) or place them on a **stable absolute path**, then use **`prebuilt_static_library` / `prebuilt_shared_library`** (paths **relative to `target.xml` or absolute**; same as Step 7).

1. In CI or a local script, run upstream **`cmake --install`** (or unpack the SDK) and take the subtree you need.
2. Vendor it into the package; in **`target.xml`** use **`<prebuilt …/>` and `<headers>`**; see **`package-target-xml-spec.md` §3.1** and **`test_projects/prebuilt_import_demo/`** (Step 7).
3. If the **aggregate CMake** still needs extra **`find_package` roots**, add them via **`--opt GZ_CMAKE_PREFIX_PATH=...`**; see **`internal-variables.md` §4.1**.

Cross-check: **`test_projects/smoke_minimal_exe/`** is a minimal executable smoke sample; third-party packaging uses **`prebuilt_import_demo/`** and Step 7.

---

## Migration topic A: existing **CMake** project → gz (hand-write)

**Recommended path**:

1. **Read upstream**: list sources, public headers, macros, `link_libraries`, install rules; in gz create one **`target.xml`** per compile unit with **`<sources>` / `<headers>` / `<defines>` / `<dependency>`** rewriting the graph.
2. **Third-party / subtrees**: after folding install output into the repo (or using absolute paths), use **`prebuilt_*` + `<prebuilt>`**; for extra **`find_package` prefixes** use **`GZ_CMAKE_PREFIX_PATH`** (Steps 7–8, **`internal-variables.md`**).
3. **Incremental**: start with **`prebuilt_*`** wrapping existing binaries, then move sources into **`static_library`** targets.
4. **History**: removed subcommands / embedded upstream CMake paths are gone; migrate per **`gz spec`** and **`package-target-xml-spec.md`** to pure hand-written XML.

---

## Migration topic B: **Qt** app → gz

**Idea**: Qt comes from **official install or out-of-tree CMake install**; gz **only hand-writes** sources, macros, **`<dependency>`** to **`prebuilt_*`** (vendored **`.lib`** / headers) or in-repo libraries; **moc / uic / rcc** via **`<preprocess>`** or a **`static_library`** sub-target to avoid CMake-backend limits on **exe**-level source preprocess.

| CMake/Qt concept | In `gz` |
|------------------|---------|
| `target_sources` / `add_executable` | **`<sources><file>…`** |
| `target_include_directories` | **`<headers>`** (install + include inference) |
| `target_compile_definitions` | **`<defines>`** |
| `find_package(Qt…)` + link | **`<dependency>`** to **`prebuilt_*`** or in-package **`static_library`/`shared_library`**; **hand-write** vendored libs/headers or set **`GZ_CMAKE_PREFIX_PATH`** to Qt’s prefix |
| `AUTOMOC` / `AUTOUIC` / `AUTORCC` | **Hand-write** **`moc`/`uic`/`rcc`** command lines, see **`script-tutorial.md` §8** |

---

## Migration topic C: third-party shapes (choice table)

| Shape | Suggested approach |
|-------|-------------------|
| **Header-only** | No link target: **`<headers>`** only, or **`asset_bundle`** / install as needed. |
| **Official CMake + install** | **`cmake --install`** outside gz, then **vendor** needed files and use **`prebuilt_*` + `<prebuilt>`**, or point **`GZ_CMAKE_PREFIX_PATH`** at that prefix. |
| **Prebuilt .lib/.dll/.so only** | **`prebuilt_static_library` / `prebuilt_shared_library` + `<prebuilt>` + `<headers>`**. |
| **Patches / multi-step configure** | Keep upstream build **outside** gz; use **`preprocess`** or vendored source targets, **`script-tutorial.md`**. |
| **Another gz package in this repo** | Package **`<dependency name="PkgName"/>`** + **`PkgName:TargetName`**. |

---

## Pre-flight checklist

- [ ] **`gz configure`** succeeds; inspect **`.intermediate/build/<leaf>/packages.md`** for vars/deps.  
- [ ] **`when`** only on supported tags; complex conditions → multiple keys + **`--opt`**.  
- [ ] Cross-package refs declared in **`package.xml`**.  
- [ ] **`gz run`** uses correct **`--install-dir-name`** (matches **`arch=`** in **`gz_cache.txt`**).  
- [ ] Third-party license/binary redistribution OK.  

More commands and dirs: **`cli-reference.md`**; doc split and FAQ: **`user-manual.md`**.
