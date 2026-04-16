# `up project` fixtures

Manual checks (from repository root, using built `up.exe`):

```bat
cd test_projects\project_fixtures\cmake_hello
..\..\..\_build\Release\up.exe project --dry-run
```

Expect: probe CMake, `fixture_hello` target with `main.cpp`.

```bat
cd test_projects\project_fixtures\zlib_style
..\..\..\_build\Release\up.exe project --dry-run
```

Expect: source-tree heuristic, one `static_library` with `a.c` and `b.c`.

```bat
cd test_projects\project_fixtures\autotools_min
..\..\..\_build\Release\up.exe project --dry-run
```

Expect: Autotools, executable `demo` with `main.c`.

```bat
cd test_projects\project_fixtures\qmake_min
..\..\..\_build\Release\up.exe project --dry-run
```

Expect: QMake, executable `qdemo` with `main.cpp`.

Use `--output-dir` to generate into a temp directory; add `--force` if re-running against an existing `package.xml`.
