#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Install runtime/dev components via CMake install COMPONENT (cmake --install).

默认仅安装 COMPONENT gz_runtime 下的三个可执行文件（gz、gz-gui、gz_reverse_cmake）；可选
--with-dev 额外安装 gz_dev（当前包含 gz.lib）。不会安装、不会触碰 test_projects/
目录（测试包由 gz 命令在各自 cwd / .intermediate 下处理）。
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

# 与 CMakeLists.txt 中 install(... COMPONENT ...) 一致；与 test_projects/ 无关。
GZ_RUNTIME_COMPONENT = "gz_runtime"
GZ_DEV_COMPONENT = "gz_dev"


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
        description=f"cmake --install for COMPONENT {GZ_RUNTIME_COMPONENT!r} (optional {GZ_DEV_COMPONENT!r})",
        epilog="说明：默认安装 gz、gz-gui、gz_reverse_cmake；加 --with-dev 时额外安装 gz.lib；与 test_projects 中的示例包无关。",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--build-dir", type=Path, default=root / "_build", help="Same CMake build dir as build.py")
    ap.add_argument(
        "--prefix",
        type=Path,
        default=root / "dist",
        help="Install prefix (bin/gz.exe, bin/gz-gui.exe under prefix)",
    )
    ap.add_argument(
        "--config",
        default="Release",
        help="Build configuration for multi-config generators (e.g. MSVC)",
    )
    ap.add_argument(
        "--with-dev",
        action="store_true",
        help=f"Also install COMPONENT {GZ_DEV_COMPONENT} (currently gz.lib)",
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
        GZ_RUNTIME_COMPONENT,
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
        GZ_DEV_COMPONENT,
    ]
    dev_cmd.extend(args.cmake_install_args)
    return _run(dev_cmd, root)


if __name__ == "__main__":
    raise SystemExit(main())
