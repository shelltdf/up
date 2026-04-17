#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Pack up.exe and up-gui.exe into a distributable archive (zip / tar.gz).

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


def _run_install_py(root: Path, build_dir: Path, config: str, prefix: Path) -> int:
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
    print("+", " ".join(cmd), flush=True)
    return subprocess.call(cmd, cwd=str(root))


def _find_installed_up_pair(prefix: Path) -> tuple[Path, Path]:
    """Return (up, up-gui) paths under install prefix/bin."""
    bin_dir = prefix / "bin"
    if sys.platform == "win32":
        up = bin_dir / "up.exe"
        gui = bin_dir / "up-gui.exe"
    else:
        up = bin_dir / "up"
        gui = bin_dir / "up-gui"
    if up.is_file() and gui.is_file():
        return up, gui
    raise FileNotFoundError(f"找不到已安装文件: {up} / {gui}")


def _default_archive_path(root: Path, fmt: str, config: str) -> Path:
    (root / "dist").mkdir(parents=True, exist_ok=True)
    sys_name = platform.system().lower().replace(" ", "")
    machine = platform.machine().lower().replace(" ", "")
    base = f"up-tools_{sys_name}_{machine}_{config.lower()}"
    ext = ".zip" if fmt == "zip" else ".tar.gz"
    return (root / "dist" / f"{base}{ext}").resolve()


def _readme_bytes() -> bytes:
    text = (
        "Contents:\n"
        "  bin/up (or up.exe)\n"
        "  bin/up-gui (or up-gui.exe)\n\n"
        "Keep both binaries in the same directory.\n"
        "up-gui runs up from the same folder.\n"
    )
    return text.encode("utf-8")


def _write_zip(out: Path, up: Path, gui: Path) -> None:
    out.parent.mkdir(parents=True, exist_ok=True)
    readme = _readme_bytes()
    with zipfile.ZipFile(out, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        zf.write(up, f"bin/{up.name}")
        zf.write(gui, f"bin/{gui.name}")
        zf.writestr("README_PACKAGE.txt", readme)


def _write_tar_gz(out: Path, up: Path, gui: Path) -> None:
    out.parent.mkdir(parents=True, exist_ok=True)
    readme = _readme_bytes()
    with tarfile.open(out, "w:gz") as tf:
        tf.add(up, arcname=f"bin/{up.name}")
        tf.add(gui, arcname=f"bin/{gui.name}")
        ti = tarfile.TarInfo(name="README_PACKAGE.txt")
        ti.size = len(readme)
        tf.addfile(ti, fileobj=io.BytesIO(readme))


def main() -> int:
    root = Path(__file__).resolve().parent
    ap = argparse.ArgumentParser(
        description="Zip/tar.gz up + up-gui for distribution",
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
    ap.add_argument("-o", "--output", type=Path, default=None, help="输出归档路径（默认 dist/up-tools_*.zip）")
    args = ap.parse_args()
    build_dir = args.build_dir
    if not build_dir.is_absolute():
        build_dir = (root / build_dir).resolve()
    prefix = args.prefix
    if not prefix.is_absolute():
        prefix = (root / prefix).resolve()

    # 约定：package.py 总是先执行 install.py（install.py 内会先 build.py）。
    code = _run_install_py(root, build_dir, args.config, prefix)
    if code != 0:
        return code

    try:
        up, gui = _find_installed_up_pair(prefix)
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
        _write_zip(out, up, gui)
    else:
        _write_tar_gz(out, up, gui)

    print("wrote", out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
