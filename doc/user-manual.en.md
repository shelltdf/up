# up User Manual

This manual is for first-time users of `up`. It helps you understand and run `up` from scratch with both CLI and `up-gui`.

- Design document: [`DESIGN.md`](../DESIGN.md)
- XML spec: [`doc/package-target-xml-spec.md`](package-target-xml-spec.md)
- Example projects: [`test_projects/README.md`](../test_projects/README.md)

---

## 0. 10-minute Quick Start

If you only need a fast path, follow these steps.

### Step 1: Build `up.exe`

Run in repository root:

```powershell
cmake -S . -B _build -G "Visual Studio 17 2022" -A x64
cmake --build _build --config Release
```

### Step 2: Run sample projects

```powershell
.\_build\Release\up.exe configure --scan test_projects
.\_build\Release\up.exe build
.\_build\Release\up.exe test
.\_build\Release\up.exe run hello_demo
```

### Step 3: Check outputs

- Build tree: `.intermediate/build/<arch>/`
- Install tree: `.intermediate/install/<arch>/`
- You should see `hello_demo` logs including calls into `hello_foo`, `hello_simple_lib`, and `rock_stack`.

---

## 1. Overview: What it is and why it exists

`up` (uni-package) is a data-driven package/build orchestrator. You describe package and target relationships with `package.xml` and `target.xml`, and `up` handles:

1. Scanning descriptors and building a package/target graph
2. Generating backend build files (mainly CMake now, also Ninja)
3. Build, test, run, and pack workflows

### Why this software

In C/C++ projects, common pain points are:

- Growing dependency complexity between libraries and executables
- Scattered build/relationship definitions
- High switching cost across build backends

`up` follows a simple principle: data defines behavior.

### Boundary with repo scripts

`build.py` and `install.py` only build/install host tools (`up.exe`, `up-gui.exe`). They do not build packages under `test_projects`.

---

## 2. Internal workflow details

Main flow of `up`:

1. **Scan**: find `package.xml` and `target.xml` under `cwd` (or `--scan` roots)
2. **Attach**: each `target.xml` is attached to the nearest package root
3. **Validate**:
   - package dependencies exist in the scan set
   - target dependencies resolve to library targets
4. **Generate**: create backend files under `.intermediate/build/<arch>/`
5. **Build/install**: outputs go to `.intermediate/install/<arch>/`
6. **Follow-up**: `run` / `test` / `pack` use generated/build metadata

Working directories (relative to command `cwd`):

- `.intermediate/build/<arch>/`
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
  - `static_library`
  - `shared_library`

### 3.2 Dependency levels

- Package-level dependency: `<dependency name="..."/>` in `package.xml`
- Target-level dependency: `<dependency name="..."/>` in `target.xml`
  - Intra-package: `myLib`
  - Cross-package: `otherPkg:otherLib`

### 3.3 Includes syntax (`from/to`)

Use self-closing entries inside `<includes>`:

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

### 4.2 Build `up` (repo root)

Option A: manual CMake

```powershell
cmake -S . -B _build -G "Visual Studio 17 2022" -A x64
cmake --build _build --config Release
```

Option B: Python scripts

```powershell
python build.py
python install.py --prefix dist
python package.py
```

### 4.3 CLI quick flow

```powershell
.\_build\Release\up.exe configure --scan test_projects
.\_build\Release\up.exe build
.\_build\Release\up.exe test
.\_build\Release\up.exe run hello_demo
```

Common commands:

- `up configure [--scan <dir>]... [--opt KEY=VALUE]...`
- `up build`
- `up run <target-name>`
- `up test [test-target-name]`
- `up pack`
- `up project` (placeholder)

### 4.4 Work in a single package directory

If `up` is on PATH:

```powershell
up configure
up build
up test
up run rock_app_one
```

### 4.5 `up-gui` quick flow

`up-gui` is a Win32 shell on top of `up.exe`.

Suggested flow:

1. Open `up-gui`
2. In Environment Settings, select:
   - build system
   - compiler
   - optional Android/emsdk paths
3. Pick working directory
4. Run `Configure`
5. Run `Build`
6. Optionally run `Test` / `Run`

The GUI passes selected settings to `up.exe` via `--opt`.

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

- wrong target name
- build not completed
- running under the wrong package/root

### Q4: `test` finds no tests

Make sure:

- configure/build succeeded
- target type is `executable`
- command is executed from the correct package/scan scope

### Q5: old includes syntax fails

Old:

```xml
<includes>
  <dir>../../include/xxx</dir>
</includes>
```

New:

```xml
<includes>
  <dir from="../../include/xxx" to="xxx"/>
</includes>
```

`file` and `glob` follow the same `from/to` style.

### Q6: single-package configure fails but repo-level scan works

Cross-package dependencies are usually invisible in single-package scan scope. Run from a higher-level directory and include all required packages via `--scan`.


