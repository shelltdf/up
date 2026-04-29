# `gz_reverse_cmake` 与 `configure_file`

本目录**不是** GZ 测试包（无手写 `package.xml`），仅提供**最小** `CMakeLists.txt` 以手工验证 **[`gz_reverse_cmake`](https://github.com/groundzero) / 本仓库** 的 `configure_file` → `<config_files>` 逆向。

**用法**（在仓库根构建出 `gz_reverse_cmake` 后）:

```text
cd test_projects/gz_reverse_cmake_config_file
<path-to>/gz_reverse_cmake --source . --out ./_gz_reverse_out
```

- 应生成 `package.xml` 中带 **包级** `<config_files>`（`top.in`）；
- `demo` 目标的 `target.xml` 中带 **目标级** `<config_files>`（`config.h.in`）；
- 输出路径中的 `to=` 为相对 `--source` 的启发式结果，**需**与真实 CMake 结果对照，见 `ai-software-engineering/02-physical/gz-reverse-cmake/spec.md`。
