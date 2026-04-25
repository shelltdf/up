#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Install runtime/dev components via CMake install COMPONENT (cmake --install).

默认仅安装 COMPONENT up_runtime 下的两个可执行文件；可选 --with-dev
额外安装 up_dev（当前包含 up.lib）。不会安装、不会触碰 test_projects/
目录（测试包由 up 命令在各自 cwd / .intermediate 下处理）。
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

# 与 CMakeLists.txt 中 install(... COMPONENT ...) 一致；与 test_projects/ 无关。
UP_RUNTIME_COMPONENT = "up_runtime"
UP_DEV_COMPONENT = "up_dev"


def _run(cmd: list[str], cwd: Path) -> int:
    print("+", " ".join(cmd), flush=True)
    return subprocess.call(cmd, cwd=str(cwd))


def _run_build_py(root: Path, build_dir: Path, config: str) -> int:
    cmd = [
        sys.executable,
        str(root / "build.py"),
        "--build-dir",
        str(build_dir),
        "--config",
        config,
    ]
    print("+", " ".join(cmd), flush=True)
    return subprocess.call(cmd, cwd=str(root))


def main() -> int:
    root = Path(__file__).resolve().parent
    ap = argparse.ArgumentParser(
        description=f"cmake --install for COMPONENT {UP_RUNTIME_COMPONENT!r} (optional {UP_DEV_COMPONENT!r})",
        epilog="说明：默认只安装 up / up-gui；加 --with-dev 时额外安装 up.lib；与 test_projects 中的示例包无关。",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--build-dir", type=Path, default=root / "_build", help="Same CMake build dir as build.py")
    ap.add_argument(
        "--prefix",
        type=Path,
        default=root / "dist",
        help="Install prefix (bin/up.exe, bin/up-gui.exe under prefix)",
    )
    ap.add_argument(
        "--config",
        default="Release",
        help="Build configuration for multi-config generators (e.g. MSVC)",
    )
    ap.add_argument(
        "--with-dev",
        action="store_true",
        help=f"Also install COMPONENT {UP_DEV_COMPONENT} (currently up.lib)",
    )
    ap.add_argument(
        "cmake_install_args",
        nargs="*",
        help="Extra arguments appended after cmake --install (e.g. -v)",
    )
    args = ap.parse_args()
    build_dir = args.build_dir
    if not build_dir.is_absolute():
        build_dir = (root / build_dir).resolve()
    prefix = args.prefix
    if not prefix.is_absolute():
        prefix = (root / prefix).resolve()

    # 约定：install.py 总是先执行 build.py。
    code = _run_build_py(root, build_dir, args.config)
    if code != 0:
        return code

    cmd = [
        "cmake",
        "--install",
        str(build_dir),
        "--prefix",
        str(prefix),
        "--config",
        args.config,
        "--component",
        UP_RUNTIME_COMPONENT,
    ]
    cmd.extend(args.cmake_install_args)
    code = _run(cmd, root)
    if code != 0:
        return code
    if not args.with_dev:
        return 0

    dev_cmd = [
        "cmake",
        "--install",
        str(build_dir),
        "--prefix",
        str(prefix),
        "--config",
        args.config,
        "--component",
        UP_DEV_COMPONENT,
    ]
    dev_cmd.extend(args.cmake_install_args)
    return _run(dev_cmd, root)


if __name__ == "__main__":
    raise SystemExit(main())
