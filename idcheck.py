#!/usr/bin/env python3
"""
idcheck.py  –  Overlay numeric sprite IDs on a Texture-Packer sprite-sheet.

Example
-------
python idcheck.py SOS/assets/spriteatlas/Tilemap_Flat.tpsheet \
    --sheet-dir SOS/assets/sprites
"""

import argparse
import json
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


# ────────────────────────── helper functions ─────────────────────────────
def load_atlas(path: Path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f).get("textures", [])


def best_font(size: int) -> ImageFont.FreeTypeFont:
    """Pick a truetype font that probably exists; else default bitmap font."""
    for name in ("DejaVuSans-Bold.ttf", "Arial.ttf", "LiberationSans-Bold.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def text_wh(draw: ImageDraw.ImageDraw, text: str, font: ImageFont.ImageFont):
    """
    Return (width, height) of *text* for both old (<10) and new (>=10) Pillow.
    """
    if hasattr(draw, "textbbox"):  # Pillow 10+ recommended path
        left, top, right, bottom = draw.textbbox((0, 0), text, font=font)
        return right - left, bottom - top
    # Fallback for Pillow 9.x:
    return draw.textsize(text, font=font)  # noqa: F821


def draw_id(
    draw: ImageDraw.ImageDraw,
    text: str,
    box: tuple[int, int, int, int],
    alpha: int = 160,
):
    """Draw a semi-transparent white box with centred black *text* inside *box*."""
    x1, y1, x2, y2 = box
    draw.rectangle(box, fill=(255, 255, 255, alpha))
    w, h = text_wh(draw, text, font)
    cx = x1 + (x2 - x1 - w) // 2
    cy = y1 + (y2 - y1 - h) // 2
    draw.text((cx, cy), text, font=font, fill=(0, 0, 0, 255))


# ───────────────────────────── main logic ────────────────────────────────
def annotate_sheet(
    sheet_path: Path, sprites: list[dict], out_path: Path, alpha: int
):
    img = Image.open(sheet_path).convert("RGBA")
    overlay = Image.new("RGBA", img.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)

    for idx, spr in enumerate(sprites):
        r = spr["region"]
        x1, y1, w, h = r["x"], r["y"], r["w"], r["h"]
        x2, y2 = x1 + w - 1, y1 + h - 1

        # Font size ≈ ⅓ of smaller dimension but min 10 px
        global font
        font = best_font(max(10, min(w, h) // 3))

        draw_id(draw, str(idx), (x1, y1, x2, y2), alpha=alpha)

    Image.alpha_composite(img, overlay).save(out_path)
    print(f"✓ wrote {out_path}")


def main():
    ap = argparse.ArgumentParser(
        description="Overlay numeric IDs on a sprite-sheet (keeps original intact)."
    )
    ap.add_argument("atlas", help=".tpsheet or .json produced by Texture-Packer")
    ap.add_argument(
        "--sheet-dir",
        default=".",
        help="Folder containing the PNG sheets referenced in the atlas",
    )
    ap.add_argument(
        "--out",
        help="Output file (default: <image>_IDs.png next to original)",
    )
    ap.add_argument(
        "--alpha",
        type=int,
        default=160,
        metavar="0-255",
        help="Opacity of white ID box (default 160 ≈ 63 % opaque)",
    )
    args = ap.parse_args()

    textures = load_atlas(Path(args.atlas))
    if not textures:
        sys.exit("✖  No 'textures' array in atlas.")

    for tex in textures:
        sheet_path = Path(args.sheet_dir, tex["image"] + ".png")
        if not sheet_path.exists():
            sys.exit(f"✖  Sprite-sheet PNG not found: {sheet_path}")

        out_path = (
            Path(args.out).resolve()
            if args.out
            else sheet_path.with_name(sheet_path.stem + "_IDs.png")
        )

        annotate_sheet(sheet_path, tex["sprites"], out_path, alpha=args.alpha)

        # If a single output path was supplied, only annotate the first sheet
        if args.out:
            break


if __name__ == "__main__":
    main()