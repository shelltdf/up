# gz User Manual (GroundZero)

This manual is for first-time users of `gz` (GroundZero): mental model, **how this doc fits next to the others**, `gz-gui`, **FAQ**, and short workflow templates.  
**Not** duplicated here: this page stays a **hub**; full argv / XML field tables live in the **English** topic pages in this same **`doc/en/`** directory (see table below). If anything disagrees with **`gz spec`** or source code, **`gz spec` and source** win. Optional **Simplified Chinese** editions live under **`doc/zh/`** with the **same filenames**.

> **Documentation index** (full `doc/zh` / `doc/en` table): [`../README.md`](../README.md)

### Where to look

| Topic | Doc |
|-------|-----|
| **CLI argv, every flag, cwd, intermediate dirs, exit codes, PowerShell** | [`cli-reference.md`](cli-reference.md) |
| **Step-by-step Hello World → libs / third-party** | [`getting-started.md`](getting-started.md) |
| **`package.xml` / `target.xml` fields, `when`, merge** | [`package-target-xml-spec.md`](package-target-xml-spec.md) + **`gz spec`** |
| **`gz_cache.txt`, `GZ_*`, `arch`** | [`internal-variables.md`](internal-variables.md) |
| **Script triggers** | [`script-messages.md`](script-messages.md) |
| **Preprocess / Qt** | [`script-tutorial.md`](script-tutorial.md) |
| **Repo overview / build from source** | [`README.md`](../README.md) |
| **Design** | [`DESIGN.md`](../DESIGN.md) |
| **Examples** | [`test_projects/README.md`](../test_projects/README.md) |

*Simplified Chinese (`doc/zh/`): same filenames for every row above.*

---

## 0. Quick entry and build-directory note

### Build directory (`_build` is only an example)

Paths like **`.\_build\Release\gz.exe`** use **`_build`** as a **sample** `cmake -B` directory name (could be **`_build_gz`**, etc.); replace consistently on your machine. You can also use root **`build.py`** / **`install.py`** (see **`README.md`**).

### First successful run (no long command dump here)

1. **Build `gz`**: **`README.md`** or **`python build.py`**.  
2. **Run Hello World or `test_projects`**: follow **[`getting-started.md`](getting-started.md)** from step 1.  
3. **Easy mistakes**: **`build`** needs **`--build-dir-name`**; **`run` / `test` / `pack`** need **`--install-dir-name`** naming a child under **`.intermediate/install/`** (usually **`gz print-build-dir-name`** / **`arch=`** in **`gz_cache.txt`**), **not** the build leaf **`default`**. Details: **[`cli-reference.md`](cli-reference.md)** section 2; troubleshooting: **§5 FAQ** below.

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

## 2. Internal workflow (bird’s-eye)

1. **Scan** `package.xml` / `target.xml` under **`cwd`** (or **`--scan`**).
2. **Attach** each **`target.xml`** to the nearest **`package.xml`**.
3. **Validate** package and target dependencies.
4. **Generate** under **`.intermediate/build/<leaf>/`** and write **`gz_cache.txt`**.
5. **Build/install** to **`.intermediate/install/<name>/`** (usually **`arch=`**, not necessarily **`<leaf>`**).
6. **`run` / `test` / `pack`** use **`--install-dir-name`**.

Path names, allowed characters, and **`print-build-dir-name`**: **[`cli-reference.md`](cli-reference.md)** section 2; cache keys: **[`internal-variables.md`](internal-variables.md)** §3.

**Backends:** CMake ( **`CMakeLists.txt`** ) or Ninja ( **`build.ninja`** ).

---

## 3. Concept index (details in topic docs)

### 3.1 Package / target, types, dependencies, `<headers>`

Full **`type`** list, **`<dependency>`** (including **`visibility`**), **`<headers>`** **`from`/`to`**, **`<prebuilt …/>`**: **[`package-target-xml-spec.md`](package-target-xml-spec.md)** and **`gz spec`**. Prebuilt sample: **[`test_projects/prebuilt_static_stub/README.md`](../test_projects/prebuilt_static_stub/README.md)**.

### 3.2 Aggregate CMake and `CMAKE_PREFIX_PATH`

See **[`internal-variables.md`](internal-variables.md)** §4.1.

### 3.3 `arch` and install directory name

See **[`internal-variables.md`](internal-variables.md)** (`arch` row) and **[`cli-reference.md`](cli-reference.md)** section 2.

---

## 4. Main usage (summary)

### 4.1 Prerequisites and building host tools

- CMake 3.20+, C++17; on Windows, VS 2022 + MSVC recommended.  
- **Build `gz` / `gz-gui`**: **`README.md`** or **`python build.py`** / **`install.py`**.  
- Legacy **`project`** / **`--project-dir`** removed (no alias).

### 4.2 CLI

**Argv, every flag, exit codes, `list` combinations, PowerShell snippets**: **[`cli-reference.md`](cli-reference.md)** (and **`gz --help`**). Tutorials: **[`getting-started.md`](getting-started.md)**.

### 4.3 Work in a single package directory

With **`gz` on PATH**: **`gz configure`** → **`gz print-build-dir-name`** → **`gz build --build-dir-name …`** → **`gz test` / `run --install-dir-name …`**. Details in **getting-started**.

### 4.4 `gz-gui` quick flow

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

### 4.5 Common workflow templates

- **A — New library target**: new dir + **`target.xml`** + **`<dependency>`** from exe → **`configure` → `print-build-dir-name` → `build` → `test`/`run`** (flags: **cli-reference**).  
- **B — Cross-package**: **`package.xml`** package dep + **`otherPkg:target`** + **`--scan`**.  
- **C — Headers layout**: **`from`/`to`** in **package-target-xml-spec** §3.3; inspect **`.intermediate/install/<name>/include/`**.  
- **D — Third-party SDK**: vendor the install tree (or set **`GZ_CMAKE_PREFIX_PATH`**) + hand-written **`prebuilt_*` + `<prebuilt>`** + **`<headers>`** (**getting-started** steps 7–8) → **`configure` / `build`** → **`<dependency name="pkg:target"/>`**.

Library-only packages are supported.

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

Nested **`<dir>path</dir>`** is unsupported; use **`from`/`to`** self-closing entries (for example **`<dir from="..." to="..."/>`**). See **[`package-target-xml-spec.md`](package-target-xml-spec.md)** §3.3.

### Q6: single-package configure fails but repo-level scan works

Cross-package dependencies are usually invisible in single-package scan scope. Run from a higher-level directory and include all required packages via **`--scan`**.

### Q7: Can a library-only package (no executable) run configure/build?

Yes. Library-only packages are supported (for example only **`prebuilt_*`** or only compile targets).  
`configure` requires at least one **`target.xml`** under the primary package, but no longer requires an executable target.

---

## Suggested reading order

1. **This manual** (split table + bird’s-eye + FAQ + `gz-gui`)  
2. **[`getting-started.md`](getting-started.md)** (hands-on)  
3. **[`cli-reference.md`](cli-reference.md)** (argv / exit codes)  
4. **[`package-target-xml-spec.md`](package-target-xml-spec.md)** / **`gz spec`** (XML)  
5. Variables → **[`internal-variables.md`](internal-variables.md)**; scripts → **[`script-messages.md`](script-messages.md)** / **[`script-tutorial.md`](script-tutorial.md)**  
6. **[`README.md`](../README.md)**, **[`DESIGN.md`](../DESIGN.md)**, **[`test_projects/README.md`](../test_projects/README.md)**  
7. Optional **Simplified Chinese** mirror: [`../zh/`](../zh/) (same filenames as in **`doc/en/`**).


