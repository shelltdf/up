#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Generate square 1:1 gz-gui icon (mushroom cloud / ground-zero) into GroundZeroGUI/resources/.

Strategy (with ``--ai-master``):
  * **AI raster** for ``gz_gui.png`` (512) and ICO **256×256** (sharp, colorful).
  * **LOD vector** for ICO **16 / 32 / 48** — native-drawn for legibility, **colors sampled**
    from the same master (no second full-res copy in ``resources/``; keep your source PNG
    elsewhere if you need the original resolution).

  python tools/generate_gz_gui_icon.py --ai-master path/to.png

Procedural-only (no AI file):

  python tools/generate_gz_gui_icon.py
"""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageDraw

# Default LOD palette (high contrast)
_DEFAULT = {
    "bg": (10, 14, 28, 255),
    "cap": (228, 82, 18, 255),
    "cap_edge": (180, 55, 10, 255),
    "cap_hi": (255, 190, 95, 255),
    "cap_mid": (255, 150, 60, 240),
    "hi_soft": (255, 235, 170, 200),
    "stem": (92, 100, 118, 255),
    "stem_dark": (58, 64, 78, 255),
    "gz": (255, 248, 220, 255),
    "gz_core": (255, 255, 255, 255),
    "ring": (255, 195, 110, 110),
}


def _p(pal: dict[str, tuple[int, int, int, int]] | None, key: str) -> tuple[int, int, int, int]:
    if pal and key in pal:
        return pal[key]
    return _DEFAULT[key]


def _rect(draw: ImageDraw.ImageDraw, n: int, pal: dict | None) -> None:
    draw.rectangle((0, 0, n - 1, n - 1), fill=_p(pal, "bg"))


def _lighten(rgb: tuple[int, int, int], k: float = 0.22) -> tuple[int, int, int, int]:
    r, g, b = rgb[:3]
    return (min(255, int(r + (255 - r) * k)), min(255, int(g + (255 - g) * k)), min(255, int(b + (255 - b) * k)), 255)


def _darken(rgb: tuple[int, int, int], k: float = 0.25) -> tuple[int, int, int, int]:
    r, g, b = rgb[:3]
    return (max(0, int(r * (1 - k))), max(0, int(g * (1 - k))), max(0, int(b * (1 - k))), 255)


def sample_palette_from_master(im: Image.Image) -> dict[str, tuple[int, int, int, int]]:
    """Derive LOD colors from AI / painted square master (expects already square)."""
    sm = im.resize((64, 64), Image.Resampling.LANCZOS).convert("RGBA")
    w, h = sm.size
    px = sm.load()

    edge: list[tuple[int, int, int]] = []
    cap_px: list[tuple[int, int, int]] = []
    stem_px: list[tuple[int, int, int]] = []
    bot_px: list[tuple[int, int, int]] = []

    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if a < 40:
                continue
            if x < 3 or x >= w - 3 or y < 3 or y >= h - 3:
                edge.append((r, g, b))
            if y < int(h * 0.36) and int(w * 0.2) < x < int(w * 0.8):
                if r > g + 12 and r > b + 12:
                    cap_px.append((r, g, b))
            if int(h * 0.52) < y < int(h * 0.82) and int(w * 0.35) < x < int(w * 0.65):
                if r + g + b < 520:
                    stem_px.append((r, g, b))
            if y > int(h * 0.84) and int(w * 0.32) < x < int(w * 0.68):
                bot_px.append((r, g, b))

    def mean_rgb(ps: list[tuple[int, int, int]], fb: tuple[int, int, int]) -> tuple[int, int, int]:
        if not ps:
            return fb
        return (
            int(sum(p[0] for p in ps) / len(ps)),
            int(sum(p[1] for p in ps) / len(ps)),
            int(sum(p[2] for p in ps) / len(ps)),
        )

    bg_rgb = mean_rgb(edge, (10, 14, 28))
    cap_rgb = mean_rgb(cap_px, (228, 82, 18))
    stem_rgb = mean_rgb(stem_px, (92, 100, 118))
    gz_rgb = mean_rgb(bot_px, (255, 248, 220))
    if sum(gz_rgb) < 400:
        gz_rgb = (255, 248, 220)

    cap = cap_rgb + (255,)
    stem = stem_rgb + (255,)
    bg = bg_rgb + (255,)
    gz = gz_rgb + (255,)
    gz_core = (255, 255, 255, 255)
    cap_edge = _darken(cap_rgb, 0.22)
    cap_hi = _lighten(cap_rgb, 0.28)
    cap_mid = (
        min(255, int(cap_rgb[0] * 0.95 + 25)),
        min(255, int(cap_rgb[1] * 0.9 + 40)),
        min(255, int(cap_rgb[2] * 0.85)),
        240,
    )
    hi_soft = _lighten(cap_rgb, 0.38)
    hi_soft = (hi_soft[0], hi_soft[1], hi_soft[2], 200)
    stem_dark = _darken(stem_rgb, 0.18)
    ring = _lighten(cap_rgb, 0.35)
    ring = (min(255, ring[0]), min(255, ring[1]), min(255, ring[2]), 110)

    return {
        "bg": bg,
        "cap": cap,
        "cap_edge": cap_edge,
        "cap_hi": cap_hi,
        "cap_mid": cap_mid,
        "hi_soft": hi_soft,
        "stem": stem,
        "stem_dark": stem_dark,
        "gz": gz,
        "gz_core": gz_core,
        "ring": ring,
    }


def render_lod_icon(n: int, pal: dict[str, tuple[int, int, int, int]] | None = None) -> Image.Image:
    """Native-resolution mushroom glyph; optional palette from AI master."""
    img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    dr = ImageDraw.Draw(img)
    _rect(dr, n, pal)
    cx = n // 2

    if n <= 18:
        pad = 1
        dr.ellipse((pad, 1, n - 1 - pad, n * 2 // 3), fill=_p(pal, "cap"))
        dr.arc((pad, 1, n - 1 - pad, n * 2 // 3), 200, 340, fill=_p(pal, "cap_edge"), width=1)
        w = max(3, n // 4)
        y0 = n * 2 // 3 - 1
        y1 = n - 4
        dr.rectangle((cx - w // 2, y0, cx + w // 2 + (w % 2), y1), fill=_p(pal, "stem"))
        dr.rectangle((cx - 2, n - 3, cx + 2, n - 1), fill=_p(pal, "gz"))
        dr.rectangle((cx - 1, n - 2, cx + 1, n - 1), fill=_p(pal, "gz_core"))
        return img

    if n <= 36:
        cap_top = 2
        cap_h = n * 12 // 32
        cap_w = n - 4
        x0 = 2
        dr.ellipse((x0, cap_top, x0 + cap_w - 1, cap_top + cap_h), fill=_p(pal, "cap"))
        iw, ih = cap_w * 3 // 5, cap_h * 3 // 5
        ix = cx - iw // 2
        iy = cap_top + cap_h // 6
        dr.ellipse((ix, iy, ix + iw, iy + ih), fill=_p(pal, "cap_hi"))
        y1 = cap_top + cap_h - 1
        y2 = n - 4
        wt = max(8, n * 9 // 32)
        wb = max(5, n * 5 // 32)
        dr.polygon(
            [
                (cx - wt // 2, y1),
                (cx + wt // 2, y1),
                (cx + wb // 2, y2),
                (cx - wb // 2, y2),
            ],
            fill=_p(pal, "stem"),
        )
        dr.line([(cx, y1), (cx, y2)], fill=_p(pal, "stem_dark"), width=1)
        gr = max(3, n // 9)
        gy = n - 2 - gr // 3
        dr.ellipse((cx - gr, gy - gr // 2, cx + gr, gy + gr // 2), fill=_p(pal, "gz"))
        dr.ellipse((cx - gr // 2, gy - gr // 3, cx + gr // 2, gy + gr // 3), fill=_p(pal, "gz_core"))
        return img

    if n <= 56:
        s = float(n)
        cap_top = s * 0.06
        cap_h = s * 0.38
        cap_w = s * 0.88
        left = cx - cap_w / 2.0
        dr.ellipse((left, cap_top, left + cap_w, cap_top + cap_h), fill=_p(pal, "cap"))
        lw, lh = cap_w * 0.42, cap_h * 0.55
        dr.ellipse((left + cap_w * 0.04, cap_top + cap_h * 0.1, left + cap_w * 0.04 + lw, cap_top + cap_h * 0.1 + lh), fill=_p(pal, "cap_hi"))
        dr.arc((left, cap_top, left + cap_w, cap_top + cap_h), 200, 345, fill=_p(pal, "cap_edge"), width=max(2, n // 24))
        y1 = cap_top + cap_h * 0.78
        y2 = s * 0.9
        wt = s * 0.28
        wb = s * 0.16
        dr.polygon(
            [
                (cx - wt / 2, y1),
                (cx + wt / 2, y1),
                (cx + wb / 2, y2),
                (cx - wb / 2, y2),
            ],
            fill=_p(pal, "stem"),
        )
        dr.line([(cx, y1), (cx, y2)], fill=_p(pal, "stem_dark"), width=max(1, n // 20))
        gr = s * 0.1
        gy = y2 - s * 0.02
        dr.ellipse((cx - gr, gy - gr * 0.35, cx + gr, gy + gr * 0.35), fill=_p(pal, "gz"))
        dr.ellipse((cx - gr * 0.45, gy - gr * 0.2, cx + gr * 0.45, gy + gr * 0.2), fill=_p(pal, "gz_core"))
        m = s * 0.05
        dr.arc((m, gy - s * 0.06, s - m, gy + s * 0.22), 200, 340, fill=_p(pal, "ring"), width=max(1, n // 28))
        return img

    s = float(n)
    dr.rectangle((0, 0, n - 1, n - 1), fill=_p(pal, "bg"))
    cx_f = s * 0.5
    cap_w = s * 0.74
    cap_h = s * 0.36
    cap_left = cx_f - cap_w / 2.0
    cap_top = s * 0.07
    dr.ellipse((cap_left, cap_top, cap_left + cap_w, cap_top + cap_h), fill=_p(pal, "cap"))
    lobe_w, lobe_h = cap_w * 0.44, cap_h * 0.52
    dr.ellipse(
        (cap_left + cap_w * 0.05, cap_top + cap_h * 0.12, cap_left + cap_w * 0.05 + lobe_w, cap_top + cap_h * 0.12 + lobe_h),
        fill=_p(pal, "cap_hi"),
    )
    dr.ellipse(
        (cap_left + cap_w * 0.5, cap_top + cap_h * 0.14, cap_left + cap_w * 0.5 + lobe_w * 0.88, cap_top + cap_h * 0.14 + lobe_h * 0.88),
        fill=_p(pal, "cap_mid"),
    )
    hi_w, hi_h = cap_w * 0.32, cap_h * 0.26
    dr.ellipse(
        (cap_left + cap_w * 0.22, cap_top + cap_h * 0.16, cap_left + cap_w * 0.22 + hi_w, cap_top + cap_h * 0.16 + hi_h),
        fill=_p(pal, "hi_soft"),
    )
    dr.arc((cap_left, cap_top, cap_left + cap_w, cap_top + cap_h), 195, 350, fill=_p(pal, "cap_edge"), width=max(2, int(s * 0.012)))

    stem_top = cap_top + cap_h * 0.74
    stem_bot = s * 0.91
    stem_w_top = s * 0.26
    stem_w_bot = s * 0.15
    dr.polygon(
        [
            (cx_f - stem_w_top / 2, stem_top),
            (cx_f + stem_w_top / 2, stem_top),
            (cx_f + stem_w_bot / 2, stem_bot),
            (cx_f - stem_w_bot / 2, stem_bot),
        ],
        fill=_p(pal, "stem"),
    )
    dr.line([(cx_f, stem_top), (cx_f, stem_bot)], fill=_p(pal, "stem_dark"), width=max(1, int(s * 0.014)))

    gz_y = stem_bot - s * 0.018
    gz_r = max(4.0, s * 0.065)
    dr.ellipse((cx_f - gz_r, gz_y - gz_r * 0.38, cx_f + gz_r, gz_y + gz_r * 0.38), fill=_p(pal, "gz"))
    dr.ellipse((cx_f - gz_r * 0.48, gz_y - gz_r * 0.22, cx_f + gz_r * 0.48, gz_y + gz_r * 0.22), fill=_p(pal, "gz_core"))

    if n >= 96:
        ring_margin = s * 0.055
        dr.arc(
            (ring_margin, gz_y - s * 0.07, s - ring_margin, gz_y + s * 0.26),
            start=198,
            end=342,
            fill=_p(pal, "ring"),
            width=max(2, int(s * 0.016)),
        )
    return img


def load_square_master(path: Path) -> Image.Image:
    im = Image.open(path).convert("RGBA")
    w, h = im.size
    if w != h:
        side = min(w, h)
        left = (w - side) // 2
        top = (h - side) // 2
        im = im.crop((left, top, left + side, top + side))
    return im


def write_lod_outputs(out_dir: Path) -> None:
    """Procedural-only: all sizes LOD."""
    png_path = out_dir / "gz_gui.png"
    ico_path = out_dir / "gz_gui.ico"
    f16, f32, f48, f256 = (render_lod_icon(s) for s in (16, 32, 48, 256))
    ico_sizes = [(16, 16), (32, 32), (48, 48), (256, 256)]
    f256.save(ico_path, format="ICO", sizes=ico_sizes, append_images=[f16, f32, f48])
    render_lod_icon(512).save(png_path, format="PNG")
    print("Wrote", ico_path, "(LOD 16/32/48/256) and", png_path, "(LOD 512)")


def write_ai_hybrid(master: Image.Image, out_dir: Path) -> None:
    """AI master for PNG + ICO 256; LOD 16/32/48 with palette from master."""
    png_path = out_dir / "gz_gui.png"
    ico_path = out_dir / "gz_gui.ico"
    master_rgba = master.convert("RGBA")
    pal = sample_palette_from_master(master_rgba)
    f16 = render_lod_icon(16, pal)
    f32 = render_lod_icon(32, pal)
    f48 = render_lod_icon(48, pal)
    f256 = master_rgba.resize((256, 256), Image.Resampling.LANCZOS)
    master_rgba.resize((512, 512), Image.Resampling.LANCZOS).save(png_path, format="PNG")
    f256.save(ico_path, format="ICO", sizes=[(16, 16), (32, 32), (48, 48), (256, 256)], append_images=[f16, f32, f48])
    print("Wrote", png_path, "(AI 512),", ico_path, "(AI 256 + LOD 16/32/48 tinted from AI)")


def main() -> int:
    root = Path(__file__).resolve().parent.parent
    out_dir = root / "GroundZeroGUI" / "resources"
    out_dir.mkdir(parents=True, exist_ok=True)

    ap = argparse.ArgumentParser(description="Generate gz_gui icon assets.")
    ap.add_argument(
        "--ai-master",
        type=Path,
        default=None,
        help="Square PNG/JPEG (e.g. AI-generated master); writes AI 512 PNG + ICO 256, LOD small sizes with sampled colors.",
    )
    args = ap.parse_args()

    if args.ai_master:
        src = args.ai_master.expanduser().resolve()
        if not src.is_file():
            print("error: --ai-master not found:", src)
            return 2
        master = load_square_master(src)
        write_ai_hybrid(master, out_dir)
        return 0

    write_lod_outputs(out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
