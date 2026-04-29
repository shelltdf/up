#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Configure and build the repo-root CLI executables (gz, gz-gui, gz_reverse_cmake).

仓库根 CMake 工程里可执行目标为 gz、gz-gui、gz_reverse_cmake；本脚本只构建这三者；
不编译、不会改动 test_projects/ 下的示例包（那些由 gz configure/build 在 .intermediate
里单独生成）。
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

# 与 CMakeLists.txt 中 add_executable 名称一致；勿在此构建其它目标。
# 仅影响 --build-dir（默认 _build）；与 test_projects/ 无交集。
GZ_CLI_TARGETS = ("gz", "gz-gui", "gz_reverse_cmake")


def _run(cmd: list[str], cwd: Path) -> int:
    print("+", " ".join(cmd), flush=True)
    return subprocess.call(cmd, cwd=str(cwd))


def _default_generator() -> tuple[list[str], Path | None]:
    """Extra cmake configure args for -G / toolset; returns (args, cwd_hint)."""
    g = os.environ.get("GZ_CMAKE_GENERATOR", "").strip()
    if g:
        return (["-G", g], None)
    if sys.platform == "win32":
        return (["-G", "Visual Studio 17 2022", "-A", "x64"], None)
    if shutil.which("ninja"):
        return (["-G", "Ninja"], None)
    return ([], None)


def main() -> int:
    root = Path(__file__).resolve().parent
    ap = argparse.ArgumentParser(
        description="CMake configure + build for targets: " + ", ".join(GZ_CLI_TARGETS),
        epilog="说明：只构建宿主工具 gz、gz-gui、gz_reverse_cmake；不编译 test_projects 中的测试包源码。",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--build-dir", type=Path, default=root / "_build", help="CMake build directory")
    ap.add_argument(
        "--config",
        default="Release",
        choices=["Release", "Debug", "RelWithDebInfo", "MinSizeRel"],
        help="Build configuration (MSVC multi-config)",
    )
    ap.add_argument(
        "--generator", "-G", default="", help="CMake -G (overrides GZ_CMAKE_GENERATOR and defaults)"
    )
    ap.add_argument("--clean", action="store_true", help="Remove build directory before configure")
    ap.add_argument(
        "cmake_args", nargs="*", help="Extra arguments passed to the initial cmake configure line"
    )
    args = ap.parse_args()
    build_dir = args.build_dir
    if not build_dir.is_absolute():
        build_dir = (root / build_dir).resolve()

    if args.clean and build_dir.exists():
        print("+ rm tree", build_dir, flush=True)
        shutil.rmtree(build_dir)

    configure: list[str] = ["cmake", "-S", str(root), "-B", str(build_dir)]
    if args.generator:
        configure.extend(["-G", args.generator])
    else:
        gen_args, _ = _default_generator()
        configure.extend(gen_args)

    cfg_line = " ".join(configure)
    if "Visual Studio" not in cfg_line and "Xcode" not in cfg_line:
        if not any(a.startswith("-DCMAKE_BUILD_TYPE=") for a in args.cmake_args):
            configure.append(f"-DCMAKE_BUILD_TYPE={args.config}")

    configure.extend(args.cmake_args)

    code = _run(configure, root)
    if code != 0:
        return code

    build_cmd = ["cmake", "--build", str(build_dir), "--config", args.config]
    for t in GZ_CLI_TARGETS:
        build_cmd.extend(["--target", t])
    return _run(build_cmd, root)


if __name__ == "__main__":
    raise SystemExit(main())
