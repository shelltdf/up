#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Walk repo text files and write UTF-8 with BOM. See tools/README or CHANGELOG.

Files whose first bytes are #! (shebang) are skipped: a leading BOM breaks Unix exec.
"""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SKIP_DIR_NAMES = frozenset(
    {
        ".git",
        "node_modules",
        "__pycache__",
        ".venv",
        "venv",
        "3rdparty",
    }
)

TEXT_SUFFIXES = frozenset(
    {
        ".md",
        ".mdc",
        ".cmake",
        ".cpp",
        ".cc",
        ".cxx",
        ".c",
        ".h",
        ".hpp",
        ".hh",
        ".inl",
        ".ipp",
        ".m",
        ".mm",
        ".rc",
        ".xml",
        ".json",
        ".yaml",
        ".yml",
        ".toml",
        ".mmd",
        ".svg",
        ".css",
        ".html",
        ".htm",
        ".txt",
        ".ps1",
        ".bat",
        ".cmd",
        ".py",
    }
)

EXACT_NAMES = frozenset(
    {
        "CMakeLists.txt",
        "Makefile",
        "Doxyfile",
        ".gitignore",
        ".gitattributes",
        ".editorconfig",
        ".cursorignore",
    }
)


def should_skip_dir(parts: tuple[str, ...]) -> bool:
    for p in parts:
        if p in SKIP_DIR_NAMES:
            return True
        if p == ".intermediate":
            return True
        if p.startswith("_build"):
            return True
        if p == "dist" or p == "build" or p == "_tbuild":
            return True
    return False


def is_text_candidate(path: Path) -> bool:
    if path.name in EXACT_NAMES:
        return True
    return path.suffix.lower() in TEXT_SUFFIXES


def main() -> int:
    dry = "--dry-run" in sys.argv
    changed = 0
    skipped_shebang = 0
    skipped_empty = 0
    skipped_decode = 0
    skipped_already = 0

    for path in sorted(ROOT.rglob("*")):
        if not path.is_file():
            continue
        try:
            rel = path.relative_to(ROOT)
        except ValueError:
            continue
        if should_skip_dir(rel.parts[:-1]):
            continue
        if not is_text_candidate(path):
            continue

        raw = path.read_bytes()
        if len(raw) == 0:
            skipped_empty += 1
            continue
        body = raw[3:] if raw.startswith(b"\xef\xbb\xbf") else raw
        if body.startswith(b"#!"):
            skipped_shebang += 1
            print(f"[skip shebang] {rel.as_posix()}")
            continue

        has_bom = raw.startswith(b"\xef\xbb\xbf")
        try:
            text = raw.decode("utf-8-sig")
        except UnicodeDecodeError:
            skipped_decode += 1
            print(f"[skip decode] {rel.as_posix()}", file=sys.stderr)
            continue

        out_bytes = "\ufeff".encode("utf-8") + text.encode("utf-8")
        if has_bom and raw == out_bytes:
            skipped_already += 1
            continue

        if dry:
            print(f"[would write] {rel.as_posix()}")
            changed += 1
            continue

        path.write_bytes(out_bytes)
        print(f"[utf-8-bom] {rel.as_posix()}")
        changed += 1

    print(
        f"---\nchanged={changed} skip_shebang={skipped_shebang} "
        f"skip_empty={skipped_empty} skip_decode={skipped_decode} skip_unchanged={skipped_already} dry_run={dry}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
