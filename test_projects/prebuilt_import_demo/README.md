# prebuilt_import_demo

Demonstrates **`imported_static_library`** with a **vendor-shipped `.lib`** (Windows / MSVC) checked in under `third_party/stub/lib/stub.lib`, plus headers under `third_party/stub/`.

- `import_stub_lib/target.xml` declares the imported target and installs public headers via `<includes>`.
- `app/target.xml` links the imported library and calls `prebuilt_stub_value()`.

## Regenerating `stub.lib` (maintainers)

From `third_party/stub/` with MSVC x64 tools on PATH:

```bat
cl /nologo /c /O2 stub.c
lib /nologo /OUT:lib\stub.lib stub.obj
del stub.obj
```

On non-Windows hosts this sample is not exercised (the committed `.lib` is MSVC). Use a similar layout with `.a` / `.so` for other platforms if needed.
