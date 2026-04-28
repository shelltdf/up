# Scripts, preprocess, and build-flow tutorial

> **Documentation index** (full `doc/zh` / `doc/en` table): [`../README.md`](../README.md)

From a minimal runnable package to conditional sources, custom variables, and heavier preprocess/sub-target patterns. **Full English**; optional **Simplified Chinese**: [`../zh/script-tutorial.md`](../zh/script-tutorial.md). **Step-by-step from zero**: **`getting-started.md`**. **Argv, exit codes, paths**: **`cli-reference.md`** and repo **`README.md`**; **doc split and FAQ**: **`user-manual.md`**; **XML detail**: **`package-target-xml-spec.md`** and **`gz spec`**.

---

## 1. Minimal example: one package + one executable

### 1.1 Layout and `package.xml`

Under an empty directory, package root e.g. `hello_pkg/package.xml`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<package name="hello_pkg" version="0.1.0">
</package>
```

### 1.2 `target.xml` (executable)

`hello_pkg/hello_exe/target.xml`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<target name="hello" type="executable">
  <sources>
    <file>main.cpp</file>
  </sources>
</target>
```

Put `main.cpp` alongside (plain `int main()`).

### 1.3 Configure, build, run

From **parent of package root** (or any cwd with **`--scan`** pointing at a tree that contains `package.xml`):

```bash
gz configure
gz build --build-dir-name default
gz print-build-dir-name --build-dir-name default
gz run --install-dir-name <arch dir from previous step> hello
```

Notes:

- **`build`** must pass **`--build-dir-name`**, same leaf as configure.  
- **`run` / `test` / `pack`** must pass **`--install-dir-name`**: a **child name under `.intermediate/install/`**, usually **`arch=`** in **`gz_cache.txt`** or **`gz print-build-dir-name`** (**not** the build leaf `default` itself).

---

## 2. System / built-in conditions: `when`

Built-ins: **`internal-variables.md` §2**, usable in **`when`** and `@KEY@` templates.

### 2.1 Compile a source only on Windows

```xml
<sources>
  <file from="main.cpp"/>
  <file from="win_only.cpp" when="GZ_OS==windows"/>
</sources>
```

### 2.2 Truthiness with a bare key

Define a switch in `<vars>`, reference in `when`:

```xml
<vars>
  <var name="USE_NET" value="on"/>
</vars>
<sources>
  <file from="net_client.cpp" when="USE_NET"/>
</sources>
```

**`when` subset** (whole string must match one form): empty (true), `true`/`false`, **`KEY == token`** / **`KEY != token`** (`token` is **`[A-Za-z0-9_.]+` only**), or **single existing key** truth test. See **`package-target-xml-spec.md` §3.5**.

---

## 3. Custom variables

### 3.1 Defaults in XML

Package or target:

```xml
<vars>
  <var name="MY_REV" value="dev"/>
</vars>
```

### 3.2 Override at configure

```bash
gz configure --opt MY_REV=rc2
```

Or add `MY_REV=rc2` to **`gz_cache.txt`** and re-**configure** (merge: **CLI and cache later entries override**).

### 3.3 Use in generated headers/sources

**`<config_files>`**:

```xml
<config_files>
  <file in="version.h.in" to="generated/version.h"/>
</config_files>
```

Inside `version.h.in`: **`@MY_REV@`** or **`${MY_REV}`**; **`#cmakedefine`** subset supported. See **`package-target-xml-spec.md` §3.5**.

---

## 4. Preprocess / postprocess: first step to complex builds

Per **source entry** inside `<sources>` you may nest **`<preprocess/>` / `<postprocess/>`** (void tags, `command="..."`). The implementation emits **CMake `add_custom_command`** or **Ninja rules** run by **shell/cmd**, not Lua.

Example: generate code before compile (you must make the command repeatable and paths correct):

```xml
<sources>
  <file from="gen_input.txt">
    <preprocess command="python tools/gen_code.py gen_input.txt gen_out.cpp"/>
  </file>
  <file from="gen_out.cpp"/>
</sources>
```

**Note**: command strings enter generated backends; use **portable** commands or **split by OS with `when`**.

**`<headers>`** and **`<assets>`** have analogous preprocess/postprocess; see `simple_xml.cpp` and **`cmake_backend.cpp` / `ninja_backend.cpp`**.

**Message / metaprogramming hookup**: valid **`trigger`**, whether configure **dispatches**, precedence vs **`<preprocess>`**, package/target **DOM walk**: **`script-messages.md`** (`GroundZero/lib/engine/dom/script_execution.cpp`).

If XML has **`command="..."`**, that string is the backend command; if **no** `command`, configure may use **`<var type="script" … value="..."/>`** **`value`** as the whole command (`resolve_script_command`: **XML command wins**, script var is not merged in). The repo does **not** run a Lua VM on `value`; **`script_type="lua"`** filters type only—for **moc/uic/rcc**, put the real command line in **`preprocess command=`** or in **`value`**.

---

## 5. `<var type="script" …>` (Lua name and triggers)

Inside `<vars>` you may declare **script vars** (void tag, command in **`value`**):

```xml
<var name="hook" type="script" trigger="sources.preprocess" script_type="lua" value="echo prepare-sources"/>
```

Supported **`trigger`** values and **`manual`**: **`script-messages.md`** (`is_supported_script_trigger` in `simple_xml.cpp`). Non-**`lua`** `script_type` may be skipped on some paths; prefer **§4 shell `preprocess`** or dedicated tool targets. To run “script only” for a stage with no XML `command`, put a **full shell line** in **`value`**.

---

## 6. Debug merged variables

- After each configure, read **`.intermediate/build/<leaf>/packages.md`** for package/target **`<vars>`** and option overlay summary.  
- Read **`gz_cache.txt`** for final **`GZ_*`** and custom keys.  
- **`gz configure --verbose`** or **`GZ_VERBOSE=1`** for phase logs.

---

## 7. External metaprogramming: Qt **moc / uic / rcc** example

**Do not** rely on Qt CMake **`AUTOMOC` / `AUTOUIC` / `AUTORCC`** (the generator does not call `find_package(Qt6)` for you). Instead, hang **Qt CLI tools** on **§4 `preprocess`** (or **`headers` / `assets` preprocess**), emit generated `.cpp` / `.h`, then list them as normal **`<sources>` / `<headers>`**.

### 7.1 General rules

1. **Working directory / paths**: `command` runs under **shell/cmd**; use paths reliable relative to the **target directory** (`target.xml` dir). **Unlike `<config_files>` templates**, **`preprocess` `command` is not `@KEY@` / `${KEY}` substituted at configure**; for configurable Qt paths, use **PATH at build time**, **wrapper scripts**, or hand-embed fragments you control.
2. **List every generated file**: if `moc` writes `gen/moc_widget.cpp`, the target must include **`<file from="gen/moc_widget.cpp"/>`** and preprocess must create it first.
3. **Include dirs**: if `uic` writes `gen/ui_mainwindow.h`, ensure compile **include paths** cover `gen/` (via **`<headers>`** / generated include dirs as your project does).
4. **Link Qt**: **`<dependency>`** to **`prebuilt_*`** or in-repo libraries; **prefer** vendored Qt binaries or **`GZ_CMAKE_PREFIX_PATH`** to Qt’s prefix; this section covers **codegen** only.

### 7.2 **moc**

Typical: generate `moc_*.cpp` **before** compiling the translation unit that needs it.

- **Approach A (recommended)**: list **moc output** as a source; attach preprocess on the **input .cpp** or a stub so ordering is correct.

```xml
<sources>
  <file from="widget.cpp">
    <preprocess command="moc widget.hpp -o gen/moc_widget.cpp"/>
  </file>
  <file from="gen/moc_widget.cpp"/>
</sources>
```

(On Windows, adjust `command` to `cmd /c`, PowerShell, or split with **`when`**.)

- **Approach B**: driving moc from a **header-only** row inside `<sources>` is awkward; safer is **A** or put **`widget.hpp`** under **`<headers>`** and use **`headers.preprocess`** to emit `gen/moc_widget.cpp`, then `<sources>` includes that cpp.

### 7.3 **uic**

Run **before** the `.cpp` that includes the generated header:

```xml
<sources>
  <file from="mainwindow.cpp">
    <preprocess command="uic mainwindow.ui -o gen/ui_mainwindow.h"/>
  </file>
</sources>
```

In `mainwindow.cpp`: `#include "gen/ui_mainwindow.h"` (paths aligned with include dirs).

### 7.4 **rcc**

Put **`.qrc`** under **`<assets>`** or **`<sources>`**, call **`rcc`** in **`assets.preprocess`** or **`sources.preprocess`**:

```xml
<assets>
  <file from="app.qrc">
    <preprocess command="rcc -name qapp app.qrc -o gen/qrc_app.cpp"/>
  </file>
</assets>
```

Then **`<sources>`**: **`<file from="gen/qrc_app.cpp"/>`**.

### 7.5 With **`<var type="script" trigger="…">`**

- If **`<preprocess/>`** has **no** `command`, a matching **`<var type="script" trigger="sources.preprocess" … value="…"/>`** supplies the **whole shell line** (see §4 table). Good for long commands maintained next to package vars.
- **Do not** expect Lua to **append** when **`command="moc …"`** already exists: **XML command wins**.

### 7.6 Backend differences (**read this**)

- **Ninja**: per source with preprocess, a **stamp** runs before that `.o`; same behavior for exe and lib.
- **CMake** (`cmake_backend.cpp`): today **`add_custom_command` + `add_dependencies`** (and library **`POST_BUILD`** postprocess) are emitted for **`<sources>` preprocess** only on **`static_library` / `shared_library`** (**`library`** already resolved to one of these); **`executable` targets do not** get `<sources>` preprocess rules. Mitigations: put moc/uic/rcc code in a **`static_library`** sub-target and **`dependency`** from the exe; or use **Ninja** as the top-level backend; or keep upstream Qt CMake **outside** gz and **hand-write** consumption in `target.xml`.

---

## 8. Related links

| Doc | Content |
|-----|---------|
| `internal-variables.md` | Built-ins, `gz_cache` fixed lines, `GZ_*` list |
| `package-target-xml-spec.md` | XML / `when` / `config_files` |
| `cli-reference.md` | argv, exit codes, intermediate dirs, examples |
| `user-manual.md` | Doc split, bird’s-eye, FAQ, gz-gui |
