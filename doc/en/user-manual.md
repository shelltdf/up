# gz User Manual (GroundZero)

This manual is for first-time users of `gz` (GroundZero). It helps you understand and run `gz` from scratch with both CLI and `gz-gui`.

> **Documentation index** (full `doc/zh` / `doc/en` table): [`../README.md`](../README.md)

- Step-by-step tutorial: [`../zh/getting-started.md`](../zh/getting-started.md) (English entry: [`getting-started.md`](getting-started.md))
- Product direction (**hand-written** `package.xml` / `target.xml`): intro of [`../zh/package-target-xml-spec.md`](../zh/package-target-xml-spec.md)
- Design document: [`DESIGN.md`](../DESIGN.md)
- XML spec: [`../zh/package-target-xml-spec.md`](../zh/package-target-xml-spec.md) (English entry: [`package-target-xml-spec.md`](package-target-xml-spec.md))
- Example projects: [`test_projects/README.md`](../test_projects/README.md)

---

## 0. 10-minute Quick Start

If you only need a fast path, follow these steps.

### Step 1: Build `gz.exe`

Run in repository root:

```powershell
cmake -S . -B _build -G "Visual Studio 17 2022" -A x64
cmake --build _build --config Release
```

> **Build directory:** **`_build`** is only an example. Use the same path you passed to **`cmake -B ...`** (for example **`_build_gz`**). Update the **`.\_build\Release\`** prefix in the next commands accordingly.

### Step 2: Run sample projects

```powershell
.\_build\Release\gz.exe configure --scan test_projects
$ARCH = .\_build\Release\gz.exe print-build-dir-name
.\_build\Release\gz.exe build --build-dir-name default
.\_build\Release\gz.exe test --install-dir-name $ARCH
.\_build\Release\gz.exe run --install-dir-name $ARCH hello_demo
```

**Note:** `build` requires **`--build-dir-name`** (same leaf as `configure`, usually **`default`** if omitted). `run` / `test` / `pack` require **`--install-dir-name`**, which must name a **child directory under `.intermediate/install/`** — typically the **`arch`** string from **`gz print-build-dir-name`** (or the `arch=` line in `gz_cache.txt`), **not** the build leaf `default`.

### Step 3: Check outputs

- Generated/build tree: `.intermediate/build/<leaf>/` (example **`default/`**)
- Install tree: `.intermediate/install/<arch>/` (**`<arch>`** is `$ARCH` above)
- You should see `hello_demo` logs including calls into `hello_foo`, `hello_simple_lib`, and `rock_stack`.

---

## 1. Overview: What it is and why it exists

`gz` (GroundZero) is a data-driven package/build orchestrator. You describe package and target relationships with `package.xml` and `target.xml`, and `gz` handles:

1. Scanning descriptors and building a package/target graph
2. Generating backend build files (mainly CMake now, also Ninja)
3. Build, test, run, and pack workflows

### Why this software

In C/C++ projects, common pain points are:

- Growing dependency complexity between libraries and executables
- Scattered build/relationship definitions
- High switching cost across build backends

`gz` follows a simple principle: data defines behavior.

### Boundary with repo scripts

`build.py` and `install.py` only build/install host tools (`gz.exe`, `gz-gui.exe`, and optional `gz.lib` as dev component). They do not build packages under `test_projects`.

Source layout is split as:

- `GroundZero/exe/`: CLI entry and command dispatch (`gz.exe`)
- `GroundZero/lib/`: core implementation (`gz.lib`, including `engine` and `infra`)

---

## 2. Internal workflow details

Main flow of `gz`:

1. **Scan**: find `package.xml` and `target.xml` under `cwd` (or `--scan` roots)
2. **Attach**: each `target.xml` is attached to the nearest package root
3. **Validate**:
   - package dependencies exist in the scan set
   - target dependencies resolve to library targets
4. **Generate**: create backend files under **`.intermediate/build/<leaf>/`** (from **`configure --build-dir-name`**, default **`default`**) and write **`gz_cache.txt`** (includes **`arch=`**)
5. **Build/install**: `build` installs to **`.intermediate/install/<arch>/`** where **`<arch>`** comes from the cache (usually **not** equal to **`<leaf>`**)
6. **Follow-up**: `run` / `test` / `pack` use install trees; CLI requires **`--install-dir-name <arch>`**

Working directories (relative to command `cwd`):

- `.intermediate/build/<leaf>/`
- `.intermediate/install/<arch>/`
- `.intermediate/pack/<arch>/`

Backend behavior:

- **CMake mode**: generates `CMakeLists.txt` then calls CMake
- **Ninja mode**: directly generates `out/build.ninja`

---

## 3. Core concepts

### 3.1 Package and target

- **package**: defined by `package.xml`
- **target**: defined by `target.xml`
  - `executable`
  - `library` (at configure time becomes **`static_library`** or **`shared_library`** from **`GZ_TARGET_DYNAMIC_LIBRARY`** / **`GZ_DYNAMIC_LIBRARY`**)
  - `static_library` / `shared_library` (each **forces** STATIC or SHARED; not overridden by the global preference above)
  - `asset_bundle` (install-only resources, no compile units)
  - `prebuilt_static_library` / `prebuilt_shared_library` (prebuilt SDKs; use `<prebuilt .../>`, paths relative to `target.xml` unless absolute). See [`test_projects/prebuilt_static_stub/`](../test_projects/prebuilt_static_stub/README.md) for an end-to-end sample (ships an MSVC `stub_import.lib`; the stub CMake writes to `lib/import/` so it is not ignored by the repo-root `dist/` `.gitignore` rule).
  - `imported_installed_static_library` / `imported_installed_shared_library`: declare libraries **already installed** under this package’s install prefix (`CMAKE_INSTALL_PREFIX`). **Recommended:** run **`cmake --install`** (or vendor SDK) **outside** `gz`, then hand-write `<install artifact="..."/>` (and `implib` on Windows for shared).

Rule boundary: `gz` / `gz-gui` must stay generic and do not embed per-project special cases. Dependency wiring, install artifacts, and include directories must be expressed explicitly via `package.xml` / `target.xml`.

### 3.1b `CMAKE_PREFIX_PATH` merging

The generated super-build receives **`CMAKE_PREFIX_PATH`** built as:

1. This configure’s install prefix (`.intermediate/install/<arch>/` for the current `cwd`), **always first**.
2. Extra directories from **`--opt GZ_CMAKE_PREFIX_PATH=dir1;dir2`** (semicolon-separated). Relative entries are resolved against **`cwd`**.

Duplicates are removed after canonicalization.

In addition, when the primary package declares dependencies that expose `imported_installed_*` targets, `configure` tries to map those installed artifacts/includes to common `find_package` cache variables and pass them to the aggregate CMake super-build (for example `<PKG>_LIBRARY`, `<PKG>_LIBRARY_DEBUG`, `<PKG>_INCLUDE_DIR`). On Windows, `implib` or `.lib` is preferred so `.dll` is not passed into linker-library variables.

### 3.2 Dependency levels

- Package-level dependency: `<dependency name="..."/>` in `package.xml`
- Target-level dependency: `<dependency name="..."/>` in `target.xml`
  - Intra-package: `myLib`
  - Cross-package: `otherPkg:otherLib`
  - **Visibility (CMake backend)**: optional **`visibility="private|public|interface"`** (default **`private`**, case-insensitive), mapping to **`PRIVATE` / `PUBLIC` / `INTERFACE`** in `target_link_libraries`. **Executable** targets must not use **`interface`** on a dependency (configure fails). Library-to-library chained `target_link_libraries` is not generated today; ensure consumers (for example the final exe) link all needed libraries.
  - **`when`** is **not** evaluated on `<dependency/>` yet; which tags support conditions and the expression grammar are defined by **`gz spec`** and **[`../zh/package-target-xml-spec.md`](../zh/package-target-xml-spec.md)**.

### 3.3 `<headers>` block (`from/to`)

Use self-closing entries inside `<headers>`:

- `<dir from="..." to="..."/>`
- `<file from="..." to="..."/>`
- `<glob from="..." to="..."/>`

Meaning:

- `from`: path relative to `target.xml` directory
- `to`: install subdirectory under `include/` (optional)

Legacy `<dir>...</dir>` syntax is no longer supported.

### 3.4 Arch tag

`<arch>` is a composite build tag (system, CPU, backend, toolchain, debug/release, CRT, etc.), not just CPU architecture.

---

## 4. Main usage

### 4.1 Prerequisites

- CMake 3.20+
- C++17 compiler
- On Windows, Visual Studio 2022 + MSVC recommended

### 4.2 Build `gz` (repo root)

Option A: manual CMake

```powershell
cmake -S . -B _build -G "Visual Studio 17 2022" -A x64
cmake --build _build --config Release
```

(See **Section 0** for the **build directory** note: **`_build`** is only an example; keep it consistent with your **`cmake -B ...`** path.)

The legacy **`project`** subcommand and **`--project-dir`** flag are removed with no alias.

Option B: Python scripts

```powershell
python build.py
python install.py --prefix dist
python package.py
```

### 4.3 CLI quick flow

```powershell
.\_build\Release\gz.exe configure --scan test_projects
$ARCH = .\_build\Release\gz.exe print-build-dir-name
.\_build\Release\gz.exe build --build-dir-name default
.\_build\Release\gz.exe test --install-dir-name $ARCH
.\_build\Release\gz.exe run --install-dir-name $ARCH hello_demo
```

Common commands (see also `gz --help`):

- `gz configure [--build-dir-name <leaf>] [--scan <dir>]... [--opt KEY=VALUE]...`
- `gz build --build-dir-name <leaf>`
- `gz run --install-dir-name <name> <target-name>`
- `gz test --install-dir-name <name> [test-target-name]`
- `gz pack --install-dir-name <name>...`
- `gz list [--format tree|json|xml] [--xml <path>] [--json <path>] [--quiet]`
- `gz spec` / `gz print-build-dir-name`

`list` option behavior summary:

- stdout payload is selected by `--format`: `tree` (default), `json`, or `xml`.
- `--xml <path>` and `--json <path>` are file exports and can be combined with any stdout format.
- `--quiet` suppresses tree text and export hint lines, but does not suppress payload for `--format json|xml`.
- Some option combinations are warning-only (no failure), such as `--format json + --xml <path>`.

### 4.4 Work in a single package directory

If `gz` is on PATH:

```powershell
gz configure
$ARCH = gz print-build-dir-name
gz build --build-dir-name default
gz test --install-dir-name $ARCH
gz run --install-dir-name $ARCH rock_app_one
```

### 4.5 `gz-gui` quick flow

`gz-gui` is a thin GUI shell (Win32 on Windows, GTK3 on Linux, Cocoa on macOS) that runs `gz` / `gz.exe` from the same install directory.

Suggested flow:

1. Open `gz-gui`
2. In Environment Settings, select:
   - build system
   - compiler
   - optional Android/emsdk paths
3. Pick working directory
4. Run `Configure`
5. Run `Build`
6. Optionally run `Test` / `Run`

The GUI passes selected settings to `gz.exe` via `--opt`.

### 4.6 Common workflow templates

#### Template A: Add a library target

1. Create a subdirectory (for example `myLib/`)
2. Add `target.xml` and source files
3. Reference it from an executable target with `<dependency name="myLib"/>`
4. Run `configure`, then `print-build-dir-name`, then `build --build-dir-name`, then `test`/`run` with **`--install-dir-name`** (see §4.3)

#### Template B: Add cross-package dependency

1. Declare package dependency in `package.xml`
2. Reference with `otherPkg:otherTarget` in `target.xml`
3. Ensure both packages are included in configure scan roots

#### Template C: Verify `<headers>` install layout

1. Use `from/to` entries in `<headers>` (`dir/file/glob`)
2. Run `gz configure`, then `gz build --build-dir-name default` (or the same leaf you used for configure)
3. Check `.intermediate/install/<arch>/include/` (**`<arch>`** from `gz print-build-dir-name` or `gz_cache.txt`)

#### Template D: Import third-party CMake SDK (hand-written)

1. **Outside** `gz`, run **`cmake --install`** on the vendor tree (or install a shipped SDK) so `lib/` / `include/` are stable.
2. Hand-write **`package.xml`** / **`target.xml`**: use **`imported_installed_*` + `<install .../>`** or **`prebuilt_*` + `<prebuilt>`** plus **`<headers>`** (see **[`../zh/getting-started.md`](../zh/getting-started.md)**).
3. Run **`gz configure` / `gz build`** (§4.3); consume from another package with `<dependency name="pkg:target"/>`.
4. Library-only packages (no executable) are supported for configure/build.

---

## 5. FAQ

### Q1: `configure` says dependency package not found

Your scan roots do not include the required package.

Fix:

- Expand `--scan` roots
- Make sure package-level dependency is declared in `package.xml`

### Q2: Path-related errors (non-ASCII path)

Current implementation enforces ASCII path constraints to avoid backend toolchain incompatibilities.

### Q3: `run` cannot find target

Possible causes:

- missing or wrong **`--install-dir-name`** (using **`default`** by mistake — that is a **build** leaf, not the **`<arch>`** install folder name)
- wrong target name
- build not completed
- running under the wrong package/root

Use **`gz print-build-dir-name`** and run **`gz run --install-dir-name <arch> <name>`**.

### Q4: `test` finds no tests

Make sure:

- **`gz test --install-dir-name <arch>`** uses the same **`<arch>`** as the last successful configure/build
- configure/build succeeded
- target type is `executable`
- command is executed from the correct package/scan scope

### Q5: old nested `<dir>` under `<headers>` fails

Old:

```xml
<headers>
  <dir>../../include/xxx</dir>
</headers>
```

New:

```xml
<headers>
  <dir from="../../include/xxx" to="xxx"/>
</headers>
```

`file` and `glob` follow the same `from/to` style.

### Q6: Can a library-only package (no executable) run configure/build?

Yes. Library-only packages are supported, including pure `imported_installed_*` wrappers.  
`configure` requires at least one `target.xml` under the primary package, but no longer requires an executable target.

### Q7: single-package configure fails but repo-level scan works

Cross-package dependencies are usually invisible in single-package scan scope. Run from a higher-level directory and include all required packages via `--scan`.


