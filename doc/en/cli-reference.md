# `gz` command-line reference (argv and behavior)

> **Documentation index** (full `doc/zh` / `doc/en` table): [`../README.md`](../README.md)  
> **Related**: field-level XML → [`package-target-xml-spec.md`](package-target-xml-spec.md) and **`gz spec`**; variables/cache → [`internal-variables.md`](internal-variables.md); doc split, FAQ, gz-gui → [`user-manual.md`](user-manual.md). **Physical layer** (hard constraints, `gz_dom`, exit-code summary) → [`../../ai-software-engineering/02-physical/gz-cli/spec.md`](../../ai-software-engineering/02-physical/gz-cli/spec.md); **subcommand → source** → [`../../ai-software-engineering/02-physical/gz-cli/mapping.md`](../../ai-software-engineering/02-physical/gz-cli/mapping.md).

This document is the **single source of truth** for **`gz` argv**: current implementation shapes, **every subcommand**’s flags, **cwd** semantics, **intermediate directory layout**, and **copy-paste examples**. It is the **full English** edition under **`doc/en/`**; optional **Simplified Chinese** mirror: [`../zh/cli-reference.md`](../zh/cli-reference.md). Implementation entrypoints: `GroundZero/exe/cli_dispatch.cpp` (dispatch), `GroundZero/lib/infra/i18n/lang.cpp` (`gz --help` text).

**Encoding**: repo **Markdown / C++** etc. use **UTF-8 with BOM**; root **`build.py`** shebang scripts are **UTF-8 without BOM** (see **`ai-software-engineering/03-ops/developer-manual.md`** “Source and doc encoding”).

---

## 1. Overview: argv shape and cwd

- **Executable name**: on Windows usually **`gz.exe`**; below we write **`gz`**.
- **Current working directory (`cwd`)**: unless stated otherwise, **all relative paths** (including `--scan`, export paths, trees resolved from `--build-dir-name` / `--install-dir-name`) are relative to **process cwd** (`std::filesystem::current_path()`).
- **No config file for CLI**: the CLI **does not** read `gz_gui_settings.txt`; **`--opt`** and **`gz_cache.txt`** are the main workspace override channel (the GUI appends `--opt` when invoking `gz configure`, same as the shell).
- **Path charset**: **`configure`** / **`list`** require scanned **`package.xml` / `target.xml` paths** to be **ASCII-only** (non-ASCII paths fail with an error). Other subcommands mostly use generated install/cache paths.

### 1.1 Top-level usage (matches `gz --help`)

```text
gz [--verbose|-v] <subcommand> ...
gz [--verbose|-v] print-build-dir-name [--build-dir-name <leaf>] [--opt KEY=VALUE]...
gz spec
gz list [--format tree|json|xml] [--xml <path>] [--json <path>] [--quiet] [--scan <dir>]...
gz configure [--build-dir-name <leaf>] [--scan <dir>]... [--opt KEY=VALUE]...
gz build --build-dir-name <leaf> [--no-emit-redistribution-xml]
gz run --install-dir-name <name> <executable-target-name>
gz test --install-dir-name <name> [test-exe-stem]
gz pack --install-dir-name <name> [--install-dir-name <name>]...
gz --help | -h | help
```

- **`help` / `-h` / `--help`**: may appear as the **first** argument (`gz help`), same as **`gz --help`** (prints usage, **exit 0**).
- **No arguments**: `gz` alone prints usage, **exit 0**.

### 1.2 Global `--verbose` / `-v` and `GZ_VERBOSE`

- **Position**: **must appear before the subcommand** (implementation strips globally first).  
  **Valid**: `gz -v configure ...`, `gz --verbose list`  
  **Invalid**: `gz configure -v ...` (`-v` becomes an unknown configure flag; configure does not accept `-v`)
- **Environment**: `GZ_VERBOSE` in **`1` / `true` / `yes` / `on`** (case-insensitive) turns verbose on (can stack with `--verbose`; result is on).
- **Behavior**: extra phase messages to **stderr**; **does not** change subcommand semantics.

---

## 2. Intermediate directories and “leaf / arch name”

All paths below are under **cwd**:

| Concept | Typical path | Meaning |
|---------|--------------|---------|
| **Build leaf `<leaf>`** | `.intermediate/build/<leaf>/` | **`configure`** writes generated files and **`gz_cache.txt`**; **`build`** selects this tree with **`--build-dir-name <leaf>`**. If **`configure --build-dir-name`** is omitted, the implementation uses **`default`**. |
| **Install directory name `<name>`** | `.intermediate/install/<name>/` | After **`build`**, install root; **`run` / `test` / `pack`** use **`--install-dir-name <name>`**. `<name>` usually equals **`arch=`** in **`gz_cache.txt`** or one line from **`gz print-build-dir-name`** (**not** necessarily the same as **`<leaf>`**). |
| **Pack output** | `.intermediate/pack/<name>/` | **`pack`** writes archives using the **last segment** of each install dir as `<name>` (see `pack.cpp`). |

### 2.1 String rules for `--build-dir-name` / `--install-dir-name`

**`<leaf>` / `<name>`** must satisfy (`GroundZero/exe/cli_paths.cpp` **`intermediate_leaf_name_ok`**):

- Non-empty, not **`.`** or **`..`**, no **`..`** substring;
- No **`/`** or **`\`**;
- On Windows, additionally no **`:` `<` `>` `|` `?` `*`**.

Otherwise the subcommand returns **exit code 2** with an **`invalid ...`** style message.

---

## 3. Exit codes (for scripts)

| Code | Meaning (summary) |
|------|---------------------|
| **0** | Success; or only printed **`--help`** / usage. |
| **1** | **Unknown subcommand** (usage printed again). |
| **2** | **Missing/invalid args**, common “cache/dir not found” recoverable errors (stderr explains). |
| **3** | Some heavy steps failed (e.g. **`list`** non-DOM write failures—see each command). |
| **4** | **`list`**: failed to export XML/JSON to a file or write DOM to stdout. |
| **5** | **`configure`**: **business failure** when generating the backend graph / target model (e.g. **`target.xml` `type`** not in the allowed set ⇒ stderr **`configure: unknown target type "…" for target "…"`**; other prebuilt/import checks may also return **5**—see `configure.cpp`). |
| **6** | **`list`**: `package.xml` / `target.xml` path contains **non-ASCII**. |
| **8** | **`build`**: **`gz_redist_manifest.json`** exists but **cannot be parsed** (corrupt JSON, etc.). If the file is **missing**, redistribution is **skipped** with **exit 0** (configure may omit the file when the primary package has no library-like targets). |
| **9** | **`build`**: redistribution XML **emit failed** (missing expected binaries under the install tree, etc.). |
| Other | **`run`** etc. may return **child process exit** via `std::system` (on Windows, codes >255 may collapse to **1**—see `test.cpp`). |

Finer “physical spec” summary: **`ai-software-engineering/02-physical/gz-cli/spec.md`**.

---

## 4. Subcommand: `print-build-dir-name`

**Purpose**: without running **configure**, read **`.intermediate/build/<leaf>/gz_cache.txt`** (missing file ⇒ **empty option map**), merge **`GZ_*` `--opt`** from the command line, print the **`arch`** string (**one line**, stdout) for scripts to pass to **`--install-dir-name`**.

```text
gz print-build-dir-name [--build-dir-name <leaf>] [--opt KEY=VALUE]...
```

| Flag | Required | Meaning |
|------|------------|---------|
| **`--build-dir-name <leaf>`** | No | Read **`.intermediate/build/<leaf>/gz_cache.txt`**. Omitted ⇒ **`default`**. |
| **`--opt KEY=VALUE`** | No | Repeatable. Only keys **starting with `GZ_`** participate in merge and affect output; other keys are **ignored** (unlike **`configure --opt`**, which accepts more keys). |
| **`--opt=KEY=VALUE`** | No | Single-token form, same as **`--opt KEY=VALUE`**. |

**Merge (short)**: load cache map, apply CLI **`GZ_*` `--opt`**, compute **`arch`**, print. Invalid **`--build-dir-name`** ⇒ **exit 2**.

**Examples**

```powershell
gz print-build-dir-name
gz print-build-dir-name --build-dir-name default --opt GZ_CMAKE_GENERATOR=Ninja
```

---

## 5. Subcommand: `configure`

**Purpose**: scan **`package.xml` / `target.xml`**, generate the backend project (CMake or Ninja, etc.), write **`.intermediate/build/<leaf>/gz_cache.txt`** (metadata includes **`arch=`**, **`scan_roots=`**, merged options).

```text
gz configure [--build-dir-name <leaf>] [--scan <dir>]... [--opt KEY=VALUE]...
```

| Flag | Required | Meaning |
|------|------------|---------|
| **`--build-dir-name <leaf>`** | No | Build leaf for output; default **`default`**. Invalid ⇒ **exit 2**. |
| **`--scan <dir>`** | No | Repeatable; each dir is a scan root (with **cwd**, recursively find XML). Recursion **never descends** into **`.intermediate`**. Any **`--scan`** root that resolves **under `<cwd>/.intermediate/`** is **dropped** with a warning. If no **`--scan`**, only **cwd** is scanned. |
| **`--opt KEY=VALUE`** | No | Repeatable; also **`--opt=KEY=VALUE`**. Overrides **`GZ_*`** and project keys (see **`internal-variables.md`** / **`gz spec`**). |

**Paths**: scanned XML paths must be **ASCII-only** (otherwise configure fails).

**`type` and exit code 5**: **`target.xml` `type`** must be one of the values listed in **[`package-target-xml-spec.md`](package-target-xml-spec.md) §3.1** and accepted by the implementation whitelist. If it is not recognized, stderr prints **`configure: unknown target type "…" for target "…" in …/target.xml`**, **exit 5** (other **`configure` failures** may also use **5**—see the table above and `GroundZero/lib/engine/commands/configure.cpp`).

**`GZ_TARGET_DYNAMIC_LIBRARY` (alias `GZ_DYNAMIC_LIBRARY`)**: participates in **`arch`** static/dynamic segment and resolves **`target.xml` `type="library"`** to **`static_library`** or **`shared_library`** before backend emission; **`static_library` / `shared_library`** are **not** overridden by this. See **[`package-target-xml-spec.md`](package-target-xml-spec.md) §3.1**.

**Examples**

```powershell
gz configure --scan test_projects
gz configure --build-dir-name myleaf --scan . --opt GZ_CMAKE_GENERATOR="Visual Studio 17 2022"
Set-Location test_projects\hello_demo
gz configure
```

---

## 6. Subcommand: `build`

**Purpose**: read **`.intermediate/build/<leaf>/gz_cache.txt`**, build the generated project, **install** to **`.intermediate/install/<arch>/`**. By default, after a successful install, emit **redistribution** **`gz-redist/package.xml`** and per-target **`gz-redist/<name>/target.xml`** (see **[`package-target-xml-spec.md`](package-target-xml-spec.md) §8**). Disable with **`--no-emit-redistribution-xml`** or **`GZ_EMIT_REDIST_XML`** set to a falsy value.

```text
gz build --build-dir-name <leaf> [--no-emit-redistribution-xml]
```

| Flag | Required | Meaning |
|------|------------|---------|
| **`--build-dir-name <leaf>`** | **Yes** | Missing ⇒ stderr **`missing required`**, **exit 2**. |
| **`--no-emit-redistribution-xml`** | No | Skip writing **`gz-redist/`** under the install root. **`GZ_EMIT_REDIST_XML=0` / `false` / `off` / `no`** also disables. Missing manifest while emit is enabled ⇒ **exit 8**; emit/validation failure ⇒ **exit 9**; manifest with **empty `targets`** ⇒ prints a skip message and **exit 0**. |

**Example**

```powershell
gz build --build-dir-name default
```

---

## 7. Subcommand: `run`

**Purpose**: find an executable under the install tree **`bin/`** and start it with **`std::system`** (on Windows, if the name has no extension, **`.exe`** is appended).

```text
gz run --install-dir-name <name> <executable-target-name>
```

| Argument | Required | Meaning |
|----------|------------|---------|
| **`--install-dir-name <name>`** | **Yes** | Points at **`.intermediate/install/<name>`**; invalid ⇒ **exit 2**. |
| **`<executable-target-name>`** | **Yes** | Installed executable **stem** (no path); missing ⇒ **exit 1**. |

**Example**

```powershell
$ARCH = gz print-build-dir-name --build-dir-name default
gz run --install-dir-name $ARCH hello_demo
```

---

## 8. Subcommand: `test`

**Purpose**: under **`.intermediate/install/<name>/bin/`**, find **test executables** and run them (current backend: directory scan + `std::system`; see `test.cpp` / `backend_dispatch.cpp`).

```text
gz test --install-dir-name <name> [test-exe-stem]
```

| Argument | Required | Meaning |
|----------|------------|---------|
| **`--install-dir-name <name>`** | **Yes** | Same as **`run`**. |
| **`[test-exe-stem]`** | No | **Omitted**: enumerate exes in `bin`, run only those whose **filename stem contains `test`** (case-insensitive), possibly multiple. **Given**: run exactly the one whose stem **matches** the argument (case-insensitive). |

Missing **`bin`** ⇒ **exit 2**; no matching tests after filter ⇒ **exit 2**.

**Examples**

```powershell
gz test --install-dir-name $ARCH
gz test --install-dir-name $ARCH hello_foo_test
```

---

## 9. Subcommand: `pack`

**Purpose**: archive one or more **install trees** (already-built outputs). Implementation tries **CPack** first, falls back to **archive** (`pack.cpp` / `backend_dispatch.cpp`). Output under **`.intermediate/pack/<name>/`**.

**Note**: **`pack` itself does not generate** **`package.xml` / `target.xml`**. If **`gz-redist/`** was produced by a normal **`gz build`** (emit is on by default), **`pack`** **includes it** in the archive. Other hand-written or scripted layouts still follow **[`package-target-xml-spec.md`](package-target-xml-spec.md) §8**.

```text
gz pack --install-dir-name <name> [--install-dir-name <name>]...
```

| Flag | Required | Meaning |
|------|------------|---------|
| **`--install-dir-name <name>`** | **At least once** | Repeatable for multi-arch / multiple install trees. Each value is an **install subdirectory name**, not a full path. |

No **`--install-dir-name`** at all ⇒ **exit 2**.

**Examples**

```powershell
gz pack --install-dir-name $ARCH
gz pack --install-dir-name win-x64-msvc --install-dir-name win-arm64-msvc
```

---

## 10. Subcommand: `list`

**Purpose**: collect **`package.xml` / `target.xml`** from **cwd + each `--scan` root**, build an in-memory **DOM**, print tree or JSON/XML, optional file export.

```text
gz list [--format tree|json|xml] [--xml <path>] [--json <path>] [--quiet] [--scan <dir>]...
```

| Flag | Required | Meaning |
|------|------------|---------|
| **`--format tree\|json\|xml`** | No | Selects **stdout payload**; default **`tree`**. Invalid ⇒ **exit 2**. |
| **`--xml <path>`** | No | Write DOM to an **XML file** (root **`<gz_dom>`**). Relative to **cwd**. On success and without quiet, prints one export hint line. |
| **`--json <path>`** | No | Write DOM to **JSON**. Same rules. |
| **`--quiet`** | No | Suppresses **tree text** and **export hint lines**; **does not** suppress stdout JSON/XML when **`--format json|xml`**. |
| **`--scan <dir>`** | No | Repeatable; extra scan roots. If **`--scan`** is passed and the list does **not** include a root equivalent to **cwd**, the implementation **appends cwd** to the scan set (deduped). Same recursion rules as **`configure`**: skip **`.intermediate`**, drop roots under **`<cwd>/.intermediate/`** with a warning. |

**Warnings (non-fatal)**:

- **`--format xml`** with **`--json <path>`** → stderr **warning**.
- **`--format json`** with **`--xml <path>`** → stderr **warning**.

**No** `package.xml`/`target.xml` found ⇒ **exit 2**.

**Examples**

```powershell
gz list
gz list --format json
gz list --xml .intermediate/dom.xml --quiet
gz list --scan test_projects --scan examples
```

---

## 11. Subcommand: `spec`

**Purpose**: print the **embedded English** `package.xml` / `target.xml` rules summary to **stdout** (long text starting with `GZ_XML_SPEC_REVISION=...`) for offline tools / AI.

```text
gz spec [any extra args...]
```

- **Current behavior**: **ignores** all trailing args (`cmd_spec` does not parse them), always prints the full text, **exit 0**.

**Example**

```powershell
gz spec > spec-embedded.txt
```

---

## 12. End-to-end example (PowerShell, repo root)

> **`_build`** is only an example **`cmake -B`** directory name; if you use **`_build_gz`**, replace every **`.\_build\Release\`** prefix below. See [`user-manual.md`](user-manual.md) **§0 Quick entry and build-directory note**.

```powershell
cmake -S . -B _build -G "Visual Studio 17 2022" -A x64
cmake --build _build --config Release

.\_build\Release\gz.exe configure --scan test_projects
$ARCH = .\_build\Release\gz.exe print-build-dir-name
.\_build\Release\gz.exe build --build-dir-name default
.\_build\Release\gz.exe test --install-dir-name $ARCH
.\_build\Release\gz.exe run --install-dir-name $ARCH hello_demo
.\_build\Release\gz.exe list --xml .intermediate\dom.xml --quiet
```

With **`gz.exe` directory on PATH**, you can write **`gz configure`**, etc. (still need correct **cwd**).

---

## 13. Boundary with GUI / repo scripts

- **CLI does not read `gz_gui_settings.txt`**; the GUI turns environment page options into **`--opt`** for configure.
- **`build.py` / `install.py`** only drive **repo-root CMake** for **`gz` / `gz-gui`**; they **do not** run **`gz configure`** on **`test_projects/`** (see root **`README.md`**).

---

## 14. Further reading (implementation and specs)

| Topic | Path |
|-------|------|
| CLI dispatch and `print-build-dir-name` | `GroundZero/exe/cli_dispatch.cpp` |
| Help text | `GroundZero/lib/infra/i18n/lang.cpp` |
| `list` flag parsing | `GroundZero/lib/engine/commands/list.cpp` |
| Intermediate root | `GroundZero/lib/infra/platform/paths.cpp` |
| Physical spec (dirs / hard constraints / `gz_dom`; **argv is this doc**) | `ai-software-engineering/02-physical/gz-cli/spec.md` |
| Subcommand → main implementation | `ai-software-engineering/02-physical/gz-cli/mapping.md` |

---

[← `doc/README.md`](../README.md) · [`user-manual.md`](user-manual.md) · [`getting-started.md`](getting-started.md)
