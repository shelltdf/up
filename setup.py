#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Install/uninstall up and up-gui with simple switches.

-i: install binaries to platform target directory
-u: uninstall binaries from platform target directory
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


def _binary_names() -> tuple[str, str]:
    if sys.platform == "win32":
        return ("up.exe", "up-gui.exe")
    return ("up", "up-gui")


def _copy_executable(src: Path, dst: Path) -> None:
    shutil.copy2(src, dst)
    if sys.platform != "win32":
        mode = dst.stat().st_mode
        dst.chmod(mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


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
    up_name, gui_name = _binary_names()
    src_up = src_bin / up_name
    src_gui = src_bin / gui_name
    if not src_up.is_file() or not src_gui.is_file():
        print(f"error: missing built binaries: {src_up} / {src_gui}", file=sys.stderr)
        return 2

    dst_up = target_dir / up_name
    dst_gui = target_dir / gui_name
    print(f"+ copy {src_up} -> {dst_up}", flush=True)
    _copy_executable(src_up, dst_up)
    print(f"+ copy {src_gui} -> {dst_gui}", flush=True)
    _copy_executable(src_gui, dst_gui)
    print(f"installed to {target_dir}")
    return 0


def do_uninstall() -> int:
    target_dir = _install_target_dir()
    up_name, gui_name = _binary_names()
    targets = [target_dir / up_name, target_dir / gui_name]
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
    ap = argparse.ArgumentParser(description="Install/uninstall up and up-gui.")
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
