#!/usr/bin/env python3
"""
idcheck.py – Overlay numeric sprite IDs on a Texture-Packer sheet.
Now uses the trailing digits of each sprite’s filename.
"""

import argparse
try:                               # tolerate comments / trailing commas
    import json5 as json           # pip install json5
except ModuleNotFoundError:
    import json
import re
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


# ────────────────────────── helper functions ──────────────────────────
def load_atlas(path: Path):
    with open(path, "r", encoding="utf-8") as fh:
        return json.load(fh).get("textures", [])


def best_font(size: int) -> ImageFont.FreeTypeFont:
    for name in ("DejaVuSans-Bold.ttf", "Arial.ttf", "LiberationSans-Bold.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def text_wh(draw: ImageDraw.ImageDraw, text: str, font: ImageFont.ImageFont):
    if hasattr(draw, "textbbox"):                      # Pillow ≥10
        l, t, r, b = draw.textbbox((0, 0), text, font=font)
        return r - l, b - t
    return draw.textsize(text, font=font)              # Pillow 9


def draw_id(draw, text, box, alpha=160):
    x1, y1, x2, y2 = box
    draw.rectangle(box, fill=(255, 255, 255, alpha))
    w, h = text_wh(draw, text, font)
    draw.text((x1 + (x2 - x1 - w) // 2, y1 + (y2 - y1 - h) // 2),
              text, font=font, fill=(0, 0, 0, 255))


# ───────────────────────────── main logic ─────────────────────────────
def annotate_sheet(sheet_path, sprites, out_path, alpha):
    img = Image.open(sheet_path).convert("RGBA")
    overlay = Image.new("RGBA", img.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)

    for spr in sprites:
        r = spr["region"]
        x1, y1, x2, y2 = r["x"], r["y"], r["x"] + r["w"] - 1, r["y"] + r["h"] - 1

        # draw the digits at the end of the filename, default to 0
        m = re.search(r"(\d+)$", spr["filename"])
        id_text = m.group(1) if m else "0"

        global font
        font = best_font(max(10, min(r["w"], r["h"]) // 3))

        draw_id(draw, id_text, (x1, y1, x2, y2), alpha)

    Image.alpha_composite(img, overlay).save(out_path)
    print("✓ wrote", out_path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("atlas")
    ap.add_argument("--sheet-dir", default=".")
    ap.add_argument("--out")
    ap.add_argument("--alpha", type=int, default=160)
    args = ap.parse_args()

    textures = load_atlas(Path(args.atlas))
    if not textures:
        sys.exit("✖  No 'textures' array in atlas.")

    for tex in textures:
        sheet_path = Path(args.sheet_dir, tex["image"] + ".png")
        if not sheet_path.exists():
            sys.exit("✖  Sprite-sheet PNG not found: " + str(sheet_path))

        out_path = Path(args.out) if args.out else sheet_path.with_name(sheet_path.stem + "_IDs.png")
        annotate_sheet(sheet_path, tex["sprites"], out_path, args.alpha)

        if args.out:      # only first sheet if the user forced one output path
            break


if __name__ == "__main__":
    main()
