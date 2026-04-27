#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""开发调试入口：先打包，再运行 dist/bin/gz-gui.exe。"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def _run(cmd: list[str], cwd: Path) -> int:
    print("+", " ".join(cmd), flush=True)
    return subprocess.call(cmd, cwd=str(cwd))


def main() -> int:
    root = Path(__file__).resolve().parent
    package_py = root / "package.py"
    gui_exe = root / "dist" / "bin" / "gz-gui.exe"

    code = _run([sys.executable, str(package_py)], root)
    if code != 0:
        return code

    if not gui_exe.is_file():
        print(f"error: gui not found: {gui_exe}", file=sys.stderr)
        return 2

    return _run([str(gui_exe)], root)


if __name__ == "__main__":
    raise SystemExit(main())
