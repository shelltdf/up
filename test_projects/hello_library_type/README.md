# hello_library_type

演示 **`target.xml`** 中 **`type="library"`**：在 **`gz configure`** 时按工作区选项 **`GZ_TARGET_DYNAMIC_LIBRARY`**（及兼容键 **`GZ_DYNAMIC_LIBRARY`**）解析为 **静态库** 或 **动态库**，再生成 CMake / Ninja。

## 布局

- **`mylib/`**：**`type="library"`** 的库目标 **`mylib`**。
- **`app/`**：可执行 **`hello_library_type_app`**；未写 **`<dependency>`**，依赖 configure 对同包库的**默认自动链接**（与 **`hello_simple_lib`** 工具目标相同策略）。

## 建议验证命令（仓库根 cwd）

```powershell
# 默认（多为静态库形态，取决于实现默认 OFF/ON）
.\_build\Release\gz.exe configure --scan test_projects\hello_library_type --build-dir-name libtype_default
.\_build\Release\gz.exe build --build-dir-name libtype_default
$ARCH = .\_build\Release\gz.exe print-build-dir-name --build-dir-name libtype_default
.\_build\Release\gz.exe run --install-dir-name $ARCH hello_library_type_app
# 期望 stdout 一行：42

# 显式动态库偏好（新构建叶子，避免与上一步缓存混淆）
.\_build\Release\gz.exe configure --scan test_projects\hello_library_type --build-dir-name libtype_dyn --opt GZ_TARGET_DYNAMIC_LIBRARY=ON
.\_build\Release\gz.exe build --build-dir-name libtype_dyn
$ARCH2 = .\_build\Release\gz.exe print-build-dir-name --build-dir-name libtype_dyn
.\_build\Release\gz.exe run --install-dir-name $ARCH2 hello_library_type_app
```

权威字段说明：**[`../../doc/zh/package-target-xml-spec.md`](../../doc/zh/package-target-xml-spec.md)** §3.1。
