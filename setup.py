#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Install/uninstall gz 运行时同目录制品：gz、gz-gui、gz_reverse_cmake、gz_gui.png。

-i: 从 dist/bin 复制到本机目标目录（Windows 为 Python 所在目录，其它为 ~/.local/bin）
-u: 从该目录删除上述文件
"""

from __future__ import annotations

import argparse
import shutil
import stat
import subprocess
import sys
from pathlib import Path


def _run(cmd: list[str], cwd: Path) -> int:
    print("+", " ".join(cmd), flush=True)
    return subprocess.call(cmd, cwd=str(cwd))


def _install_target_dir() -> Path:
    if sys.platform == "win32":
        return Path(sys.executable).resolve().parent
    return Path.home() / ".local" / "bin"


def _runtime_artifact_names() -> tuple[str, ...]:
    """与 cmake --install COMPONENT gz_runtime 装到 prefix/bin 下的制品一致。"""
    if sys.platform == "win32":
        return ("gz.exe", "gz-gui.exe", "gz_reverse_cmake.exe", "gz_gui.png")
    return ("gz", "gz-gui", "gz_reverse_cmake", "gz_gui.png")


def _copy_executable(src: Path, dst: Path) -> None:
    shutil.copy2(src, dst)
    if sys.platform != "win32":
        mode = dst.stat().st_mode
        dst.chmod(mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


def _copy_runtime_artifact(src: Path, dst: Path) -> None:
    """可执行文件设执行位；资源文件（如 png）仅 copy2。"""
    if src.suffix.lower() == ".png":
        shutil.copy2(src, dst)
        return
    _copy_executable(src, dst)


def _run_install_py(root: Path, build_dir: Path, config: str) -> int:
    cmd = [
        sys.executable,
        str(root / "install.py"),
        "--build-dir",
        str(build_dir),
        "--config",
        config,
        "--prefix",
        str(root / "dist"),
    ]
    return _run(cmd, root)


def do_install(root: Path, build_dir: Path, config: str) -> int:
    code = _run_install_py(root, build_dir, config)
    if code != 0:
        print(f"error: install.py failed with code {code}", file=sys.stderr)
        return code

    src_bin = root / "dist" / "bin"
    target_dir = _install_target_dir()
    target_dir.mkdir(parents=True, exist_ok=True)
    names = _runtime_artifact_names()
    missing: list[Path] = []
    for name in names:
        p = src_bin / name
        if not p.is_file():
            missing.append(p)
    if missing:
        print("error: missing dist/bin artifacts: " + ", ".join(str(p) for p in missing), file=sys.stderr)
        return 2

    for name in names:
        built = src_bin / name
        dst = target_dir / name
        print(f"+ copy {built} -> {dst}", flush=True)
        _copy_runtime_artifact(built, dst)
    print(f"installed to {target_dir}")
    return 0


def do_uninstall() -> int:
    target_dir = _install_target_dir()
    targets = [target_dir / n for n in _runtime_artifact_names()]
    for p in targets:
        if p.exists():
            print(f"+ remove {p}", flush=True)
            p.unlink()
        else:
            print(f"- not found {p}", flush=True)
    print(f"uninstall finished from {target_dir}")
    return 0


def main() -> int:
    root = Path(__file__).resolve().parent
    ap = argparse.ArgumentParser(description="Install/uninstall gz, gz-gui, gz_reverse_cmake, gz_gui.png.")
    group = ap.add_mutually_exclusive_group(required=True)
    group.add_argument("-i", action="store_true", help="Install binaries")
    group.add_argument("-u", action="store_true", help="Uninstall binaries")
    ap.add_argument("--build-dir", type=Path, default=root / "_build", help="Build directory for install.py")
    ap.add_argument("--config", default="Release", help="Build configuration for install.py")
    if len(sys.argv) == 1:
        ap.print_help()
        return 0
    args = ap.parse_args()

    build_dir = args.build_dir
    if not build_dir.is_absolute():
        build_dir = (root / build_dir).resolve()

    if args.i:
        return do_install(root, build_dir, args.config)
    return do_uninstall()


if __name__ == "__main__":
    raise SystemExit(main())
