# Built-in variables and overridable keys

> **Documentation index** (full `doc/zh` / `doc/en` table): [`../README.md`](../README.md)

This page summarizes variable and key names that appear in **configure / build / template substitution / cache** for **gz (GroundZero)**, and how they differ: **written by the implementation**, **passed on the CLI or GUI**, or **declared in project XML**.  
**Full English** edition; optional **Simplified Chinese**: [`../zh/internal-variables.md`](../zh/internal-variables.md). Authoritative detail remains **`gz spec`**, **`package-target-xml-spec.md`**, and source; this file is for quick lookup and scope notes.

---

## 1. Where variables live

| Carrier | Role |
|---------|------|
| **Built-in map (configure context)** | Used for `@KEY@` / `${KEY}` and **`when="..."`**; filled by the implementation from package/target/OS (see `GroundZero/lib/engine/xml/var_subst.cpp`). |
| **`<vars>` in `package.xml` / `target.xml`** | Package/target **defaults**; overridden downstream. |
| **`gz configure --opt KEY=VALUE`** | Workspace overrides; when merged with cache, **later wins** (same key repeated in `gz_cache.txt`: typically last line wins; see merge implementation). |
| **`.intermediate/build/<leaf>/gz_cache.txt`** | Written at end of configure; fixed metadata lines + **full merged option map** (key/value lines). |

**Merge order (`@` / `when` / package-level `config_files`)**: built-ins → package `<vars>` → target `<vars>` → `--opt` / `gz_cache.txt` (later overrides earlier). See **`package-target-xml-spec.md` §3.5**.

---

## 2. Built-in variables (configure fills per target/package)

Available in **`when`**, **`<config_files>`**, etc.:

| Name | Meaning (typical) |
|------|-------------------|
| `GZ_OS` | Host OS: `windows` / `linux` / `darwin` |
| `GZ_PACKAGE_NAME` | Current package (`package.xml` `name`) |
| `GZ_PACKAGE_VERSION` | Package version (default `0.0.0`) |
| `GZ_TARGET_NAME` | Current target name; **empty in package-level templates** unless a target `<var name="GZ_TARGET_NAME" …/>` overrides |
| `GZ_TARGET_BUILD_SYSTEM` | `cmake` or `ninja` |
| `GZ_CONFIG` | `debug` or `release` (from Debug/Release-style options) |

---

## 3. Fixed lines in `gz_cache.txt` (implementation-generated)

Path: `.intermediate/build/<leaf>/gz_cache.txt` (`<leaf>` = `configure --build-dir-name`, often `default` when omitted).

| Key | Meaning |
|-----|---------|
| `gz.cache.version` | Cache format version (currently `1`) |
| `cwd` | Working directory at configure time (absolute, POSIX slashes as written) |
| `arch` | **Install directory name** composite tag (e.g. `windows_x86_64_cmake_msvc_dynamic_release`); **not** necessarily the **build leaf** name |
| `package` | Primary package name (implementation bookkeeping) |
| `generated_file` | Absolute path to generated entry (root `CMakeLists.txt` or `out/build.ninja`, etc.) |
| `scan_roots` | Semicolon-separated list of scan root absolute paths |

After that: any number of **`KEY=VALUE`** lines from the merged **configure option map** (including common **`GZ_*`** below and custom keys from `--opt` / old cache).

---

## 4. Common `GZ_*` options (user / GUI / cache)

These keys are read when composing **`arch`**, **build parallelism**, **CMake behavior**, etc. **Compat aliases** in parentheses (see `option_or_compat` in `commands_common.cpp` / `configure.cpp` / `build.cpp`).

| Key (canonical) | Alias / note | Summary |
|-----------------|--------------|---------|
| `GZ_TARGET_CPU_ARCH` | `GZ_CPU_ARCH` | Target CPU tag (part of `arch`) |
| `GZ_TARGET_SYSTEM` | `GZ_SYSTEM` | Target system tag |
| `GZ_TARGET_DYNAMIC_LIBRARY` | `GZ_DYNAMIC_LIBRARY` | `ON`/`OFF`, etc.: participates in **static/dynamic** segment of **`arch`**, and resolves **`target.xml` `type="library"`** to **`static_library`** or **`shared_library`** at configure time (**`static_library` / `shared_library` are not overridden** by this). See [`package-target-xml-spec.md`](package-target-xml-spec.md) §3.1. |
| `GZ_TARGET_DEBUG` | `GZ_DEBUG` | `ON`/`OFF`, etc., Debug/Release |
| `GZ_TARGET_BUILD_SYSTEM` | `GZ_BUILD_SYSTEM` | `cmake` or `ninja` |
| `GZ_TARGET_CRT` | `GZ_CRT` | Windows CRT mode, e.g. `dynamic_md` |
| `GZ_CMAKE_GENERATOR` | — | CMake `-G` (may affect toolchain tag inference) |
| `GZ_CMAKE_PREFIX_PATH` | — | `CMAKE_PREFIX_PATH`-style prefix list (configure/cache paths) |
| `GZ_BUILD_PARALLEL` | `GZ_BUILD_JOBS` | Parallel compile jobs (aliases; default often filled from CPU count at configure) |

You may also store any **`GZ_*`** and any **C identifier** custom keys (for XML `<vars>` overrides, template placeholders, etc.); reserved keys that must not be used are enforced by validation (see **`gz spec`**).

### 4.1 Aggregate CMake and `CMAKE_PREFIX_PATH`

When configure generates an **aggregate** top-level CMake project for multiple package roots, it sets **`CMAKE_PREFIX_PATH`** as the **deduplicated** concatenation (semicolon-separated CMake list) of:

1. This configure’s install prefix under **cwd**: **`.intermediate/install/<arch>/`** (always **first**, so workspace-installed packages are found first).
2. Extra directories from **`--opt GZ_CMAKE_PREFIX_PATH=dir1;dir2`** (e.g. third-party SDK CMake roots); relative paths resolve against **cwd**.

Additionally, when the primary package depends on another package that exposes **`imported_installed_*`** targets, configure tries to map those install artifacts/includes to common **`find_package`** cache variables for the aggregate CMake (e.g. `<PKG>_LIBRARY`, `<PKG>_LIBRARY_DEBUG`, `<PKG>_INCLUDE_DIR`) to reduce flakiness when **`CMAKE_PREFIX_PATH`** alone is insufficient. On Windows, **`implib`** or **`.lib`** is preferred so **`.dll`** is not passed into linker-library variables.

---

## 5. “Variables” in project XML

| Source | Shape | Note |
|--------|-------|------|
| `package.xml` `<vars>` | `<var name="KEY" value="VAL"/>` | Shared defaults in package |
| `target.xml` `<vars>` | same | Target overrides package |
| `<define name="..." value="..."/>` | Preprocessor macro | **Not** a merge-table `KEY` for `when`; goes to compile definitions (unless a same-named `<var>` exists) |

**Script `<var>`** (`type="script"`): has `trigger` (e.g. `sources.preprocess`), `script_type` defaults to `lua`; different mechanism from **`<preprocess command="..."/>`** shell lines in `<sources>`—script pipeline vs direct backend rules. Full **`trigger`** list and dispatch: **`script-messages.md`**; Qt/preprocess flows: **`script-tutorial.md`**.

---

## 6. Environment variables (alongside CLI, not `gz_cache` keys)

| Variable | Role |
|----------|------|
| `GZ_VERBOSE` / `gz --verbose` | Progress-style logs to stderr (see CLI help) |

(Other env vars used for **compiler/SDK path probing** come from **gz-gui Win32** or the host when building `--opt` fragments; they do **not** automatically enter `gz_cache.txt` unless configure writes them.)

---

## 7. Keys written by gz-gui: `gz_gui_settings.txt` (UTF-8)

Persisted keys aligned with **`gz configure --opt`** (excerpt; full list in `GroundZeroGUI/core/gui_persist.hpp`):

| Key | Meaning |
|-----|---------|
| `local.build_system` | `cmake` / `ninja` |
| `local.compiler` | e.g. `msvc` |
| `android.sdk` / `android.ndk` | Paths |
| `emsdk.path` | EMSDK root |
| `local.vcvars` / `local.vcvars64` / `local.vcvars32` | vcvars paths |
| `browse.*` | Last paths in dialogs |
| `gui.ui_lang` | `zh` / `en` |

The GUI turns these into **`--opt GZ_*=...`** fragments for configure (same as CLI).

---

## 8. Auto placeholders (in template but not in map)

For **`<config_files>`** templates: if **`@NAME@` / `${NAME}`** appears and **`NAME`** is not yet in the merge map, the implementation **adds `NAME` with empty value** and removes the placeholder (no literal `${NAME}` left). Set real values in XML `<vars>` or **`--opt`** for non-empty expansion. See **`package-target-xml-spec.md` §3.5**.

---

## 9. Related docs

- **`package-target-xml-spec.md`** — XML, `when`, merge order
- **`cli-reference.md`** — argv, exit codes, intermediate dirs, examples
- **`user-manual.md`** — doc split table, bird’s-eye, FAQ, gz-gui
- **`DESIGN.md`** — intermediate dirs and command overview  

If this page disagrees with **`gz spec`** or source, **`gz spec` and source** win; PRs to update this page are welcome.
