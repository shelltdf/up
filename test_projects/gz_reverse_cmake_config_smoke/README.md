# gz_reverse_cmake `configure_file` 冒烟

本目录**不是** GZ 包（无手写 `package.xml`），仅提供**最小** `CMakeLists.txt` 供 `gz_reverse_cmake` 静态反解，验证 `configure_file` 映射为 `<config_files>`。

- **包级**：`configure_file(top.in top.out)` 出现在首个 `add_library` 之前 → 应写入逆向输出 `package.xml` 的 `<config_files>`。
- **目标级**：`configure_file(config.h.in config.h)` 在 `add_library` 之后 → 应写入 `lib_a/target.xml` 的 `<config_files>`。

在仓库根构建 `gz_reverse_cmake` 后：

```text
path\to\gz_reverse_cmake.exe --source <本目录> --out <临时输出根>
```

检查生成树中 `package.xml` 与 `lib_a/target.xml` 是否含对应 `<file in=... to=.../>`。
