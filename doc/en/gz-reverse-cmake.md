# Reverse CMake to GroundZero XML (`gz_reverse_cmake`)



**Reverse engineering** here means: without running a `cmake` configure and **without** the [CMake File API](https://cmake.org/cmake/help/latest/manual/cmake-file-api.7.html), statically analyze **top-level and `add_subdirectory`-reachable** `CMakeLists.txt` files, reinterpret a **fixed, narrow subset** of the language, and emit a draft **`package.xml`** plus per-**`<package>/<target>/target.xml`**, for `gz configure` or hand-editing.  

**Relationship to “hand-written `package.xml` / `target.xml`”**: this is **bootstrap only**; **authoritative fields and behavior** remain in **[`package-target-xml-spec.md`](../en/package-target-xml-spec.md)** and **`gz spec`**. Real projects will **almost always** need manual fixes (macros, un-evaluated `if`, generator expressions, external `include` modules, etc.).



> **Documentation index** (full `doc/zh` / `doc/en` table): [`../README.md`](../README.md)



## 1. What it does



| Point | Description |

|-------|-------------|

| **No configure** | Reads Listfile text on disk; no need for a build tree or `codemodel-v2` JSON. |

| **Interpreted subset** | Flattens each file to `identifier( args )` and approximates a subset: `project`, `set`, `include_directories`, `add_subdirectory`, `add_executable`, `add_library`, `target_*`, etc. |

| **Not File API** | No JSON parsing, no `.cmake/api` reads. |

| **Where output goes** | Listfiles are read only under `--source`. XML is written to **`<--out>/<package>/`**. If `--out` is omitted, it defaults to **`<--source>/gz_reverse/`**; pass an explicit `--out` for any other root. |



For implementation detail and full limits, use the **`gz_reverse_cmake` binary** and **`ai-software-engineering/02-physical/gz-reverse-cmake/spec.md`** in the repo.



## 2. Limits (summary)



- **Not** full CMake; commands inside `function` / `macro` / `foreach` / `while` **do not** register targets; **`if/else/endif` is not evaluated.  

- **`$<...>`** generator expressions are **skipped** where the tool would interpret arguments.  

- **Cross-package** links, **`prebuilt_*`**, **`<config_files>`**, fine-grained **`when`**, etc. are usually **filled in by hand** later.



See **`ai-software-engineering/02-physical/gz-reverse-cmake/spec.md`** and **`../../gz_reverse_cmake/README.md`** for the full list.



## 3. Obtaining the tool



- **Root build**:



  ```powershell

  cd <repo root>

  python build.py

  # e.g. _build\Release\gz_reverse_cmake.exe (or your generator’s output path)

  ```



- If **`python install.py`** / **`python setup.py -i`** was used, **`gz_reverse_cmake`** is on **`PATH`**



## 4. Usage and output layout

With no path flags, run in the top `CMakeLists.txt` directory: **`--source`** defaults to the **current working directory**; **`--out`** defaults to **`<--source>/gz_reverse/`**.

```text
gz_reverse_cmake [ --source <path> ] [ --out <output root> ] …
```

| Item | Description |
|------|-------------|
| **`--source`** | Source root; read-only. **Omitted = current working directory**. |
| **`--out`** | Output root. **Omitted = `<--source>/gz_reverse/`**; files go under **`<out>/<package>/`**. |
| **Overlap** | A **warning** may be printed if an **explicit** `--out` is **exactly the same** as `--source` (see tool text). The default colocated `gz_reverse` folder does not trigger it. |
| **Other** | **`--package-name` / `--package-version`**; **`--help`**. |

**Windows**: the tool sets the console to UTF-8; critical errors are duplicated in **English** for legacy `cmd` code pages.

**Shortest** (from inside the zlib root; default output under `…/zlib-1.2.13/gz_reverse/<package>/`):

```text
cd E:\dev\egg_next\3rdparty\zlib-1.2.13
gz_reverse_cmake
```

**Custom workspace output root**:

```text
gz_reverse_cmake --source E:\dev\egg_next\3rdparty\zlib-1.2.13 --out E:\dev\egg_next\gz_reverse
```



## 5. After generation



1. Add the tree that contains **`package.xml`** to your `gz` scan (or place it as your project expects), and align with **[`user-manual.md`](../en/user-manual.md)** and **[`package-target-xml-spec.md`](../en/package-target-xml-spec.md)**: **package dependencies**, **prebuilt**, **header `to`**, **defines**, etc.  

2. Run **`gz configure` / `gz build`**. Discrepancies are resolved against **the spec and `gz` source**.



## 6. Related links



| Topic | Path |

|-------|------|

| XML spec | [`package-target-xml-spec.md`](../en/package-target-xml-spec.md) |

| Implementation README | [`../../gz_reverse_cmake/README.md`](../../gz_reverse_cmake/README.md) |

| Physical `spec` | [`../../ai-software-engineering/02-physical/gz-reverse-cmake/spec.md`](../../ai-software-engineering/02-physical/gz-reverse-cmake/spec.md) |

| Logic / data flow | [`../../ai-software-engineering/01-logic/detailed-design-gz-reverse-cmake.md`](../../ai-software-engineering/01-logic/detailed-design-gz-reverse-cmake.md) |

| User manual hub | [`user-manual.md`](../en/user-manual.md) |



---



[← `doc` index `README.md`](../README.md) · [Chinese `../zh/gz-reverse-cmake.md`](../zh/gz-reverse-cmake.md)

