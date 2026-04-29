#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Pack gz + gz-gui + gz_reverse_cmake (+ optional gz.lib) into an archive.

默认只打包宿主工具，与 test_projects/ 无关。
约定：package.py 会先执行 install.py；install.py 会先执行 build.py。
"""

from __future__ import annotations

import argparse
import io
import platform
import subprocess
import sys
import tarfile
import zipfile
from pathlib import Path


def _run_install_py(root: Path, build_dir: Path, config: str, prefix: Path, with_dev: bool) -> int:
    cmd = [
        sys.executable,
        str(root / "install.py"),
        "--build-dir",
        str(build_dir),
        "--config",
        config,
        "--prefix",
        str(prefix),
    ]
    if with_dev:
        cmd.append("--with-dev")
    print("+", " ".join(cmd), flush=True)
    return subprocess.call(cmd, cwd=str(root))


def _find_installed_gz_runtime(prefix: Path) -> tuple[Path, Path, Path]:
    """Return (gz, gz-gui, gz_reverse_cmake) paths under install prefix/bin."""
    bin_dir = prefix / "bin"
    if sys.platform == "win32":
        gz_cli = bin_dir / "gz.exe"
        gui = bin_dir / "gz-gui.exe"
        rev = bin_dir / "gz_reverse_cmake.exe"
    else:
        gz_cli = bin_dir / "gz"
        gui = bin_dir / "gz-gui"
        rev = bin_dir / "gz_reverse_cmake"
    if gz_cli.is_file() and gui.is_file() and rev.is_file():
        return gz_cli, gui, rev
    raise FileNotFoundError(f"找不到已安装文件: {gz_cli} / {gui} / {rev}")


def _default_archive_path(root: Path, fmt: str, config: str) -> Path:
    (root / "dist").mkdir(parents=True, exist_ok=True)
    sys_name = platform.system().lower().replace(" ", "")
    machine = platform.machine().lower().replace(" ", "")
    base = f"gz-tools_{sys_name}_{machine}_{config.lower()}"
    ext = ".zip" if fmt == "zip" else ".tar.gz"
    return (root / "dist" / f"{base}{ext}").resolve()


def _readme_bytes(with_dev: bool) -> bytes:
    text = (
        "Contents:\n"
        "  bin/gz (or gz.exe)\n"
        "  bin/gz-gui (or gz-gui.exe)\n"
        "  bin/gz_reverse_cmake (or gz_reverse_cmake.exe)\n"
        + ("  lib/gz.lib\n" if with_dev else "")
        + "\n"
        "Keep both binaries in the same directory.\n"
        "gz-gui runs gz from the same folder.\n"
    )
    return text.encode("utf-8")


def _write_zip(out: Path, gz_cli: Path, gui: Path, rev: Path, dev_lib: Path | None) -> None:
    out.parent.mkdir(parents=True, exist_ok=True)
    readme = _readme_bytes(dev_lib is not None)
    with zipfile.ZipFile(out, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        zf.write(gz_cli, f"bin/{gz_cli.name}")
        zf.write(gui, f"bin/{gui.name}")
        zf.write(rev, f"bin/{rev.name}")
        if dev_lib is not None:
            zf.write(dev_lib, f"lib/{dev_lib.name}")
        zf.writestr("README_PACKAGE.txt", readme)


def _write_tar_gz(out: Path, gz_cli: Path, gui: Path, rev: Path, dev_lib: Path | None) -> None:
    out.parent.mkdir(parents=True, exist_ok=True)
    readme = _readme_bytes(dev_lib is not None)
    with tarfile.open(out, "w:gz") as tf:
        tf.add(gz_cli, arcname=f"bin/{gz_cli.name}")
        tf.add(gui, arcname=f"bin/{gui.name}")
        tf.add(rev, arcname=f"bin/{rev.name}")
        if dev_lib is not None:
            tf.add(dev_lib, arcname=f"lib/{dev_lib.name}")
        ti = tarfile.TarInfo(name="README_PACKAGE.txt")
        ti.size = len(readme)
        tf.addfile(ti, fileobj=io.BytesIO(readme))


def main() -> int:
    root = Path(__file__).resolve().parent
    ap = argparse.ArgumentParser(
        description="Zip/tar.gz gz + gz-gui + gz_reverse_cmake for distribution",
        epilog="示例: python package.py   或   python package.py -o dist/my.zip",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--build-dir", type=Path, default=root / "_build", help="与 build.py 相同的 CMake 构建目录")
    ap.add_argument("--config", default="Release", help="MSVC 多配置时的配置名")
    ap.add_argument("--prefix", type=Path, default=root / "dist", help="与 install.py 相同的安装前缀")
    ap.add_argument(
        "--format",
        choices=("auto", "zip", "tgz"),
        default="auto",
        help="auto: Windows 用 zip，其它平台用 tar.gz",
    )
    ap.add_argument(
        "--with-dev",
        action="store_true",
        help="Also include gz.lib (installs gz_dev component before packing)",
    )
    ap.add_argument("-o", "--output", type=Path, default=None, help="输出归档路径（默认 dist/gz-tools_*.zip）")
    args = ap.parse_args()
    build_dir = args.build_dir
    if not build_dir.is_absolute():
        build_dir = (root / build_dir).resolve()
    prefix = args.prefix
    if not prefix.is_absolute():
        prefix = (root / prefix).resolve()

    # 约定：package.py 总是先执行 install.py（install.py 内会先 build.py）。
    code = _run_install_py(root, build_dir, args.config, prefix, args.with_dev)
    if code != 0:
        return code
    dev_lib: Path | None = None
    if args.with_dev:
        candidate = prefix / "lib" / "gz.lib"
        if not candidate.is_file():
            print("error: --with-dev requested but lib/gz.lib not found under prefix", file=sys.stderr)
            return 2
        dev_lib = candidate


    try:
        gz_cli, gui, rev = _find_installed_gz_runtime(prefix)
    except FileNotFoundError as e:
        print("error:", e, file=sys.stderr)
        return 2

    fmt = args.format
    if fmt == "auto":
        fmt = "zip" if sys.platform == "win32" else "tgz"

    out = args.output
    if out is None:
        out = _default_archive_path(root, fmt, args.config)
    elif not out.is_absolute():
        out = (root / out).resolve()

    if fmt == "zip":
        _write_zip(out, gz_cli, gui, rev, dev_lib)
    else:
        _write_tar_gz(out, gz_cli, gui, rev, dev_lib)

    print("wrote", out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
