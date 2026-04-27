# prebuilt_static_stub

Demonstrates **`imported_static_library`** with **`<prebuilt import_lib="..."/>`**: a small C static library is built **once** into `lib/import/` (see `lib/CMakeLists.txt`; this folder name avoids matching the repo-root `.gitignore` rule `dist/`), then the main `gz` package links it as an **IMPORTED** target without compiling its sources in the aggregate `CMakeLists.txt`.

## Regenerate the stub import library (optional)

If you delete `lib/import/stub_import.lib` or need to refresh after editing `lib/tiny.c`:

**Windows (Visual Studio generator):**

```powershell
cmake -S lib -B lib/stub_build -G "Visual Studio 17 2022" -A x64
cmake --build lib/stub_build --config Release
```

**Windows / other (Ninja + MSVC):**

```powershell
cmake -S lib -B lib/stub_build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build lib/stub_build
```

The import library is written to **`lib/import/stub_import.lib`** (MSVC). The repository includes this file so `gz configure --scan test_projects` succeeds without an extra step on Windows x64 MSVC.

## Use with `gz`

From this directory:

```powershell
gz configure
gz build
gz run prebuilt_stub_app
```

Expected output: `tiny_add(2,3)=5`.

## Non-Windows

This fixture ships an **MSVC `stub_import.lib`**. On Linux/macOS, run the stub CMake with your toolchain so that `lib/import/` contains your platform’s static archive, then adjust **`sdk/target.xml`** `import_lib=` to that filename (e.g. `libstub_import.a` or `stub_import.a` depending on the toolchain).
