#!/usr/bin/env python3
"""Render a proof sheet for the display face.

    python3 tools/font/proof.py [out.png]

Two things are being checked, and only one of them is the alphabet. The other
is the phrases: this face exists for mission names, outcome screens and reel
captions, so the proof sets the strings the game actually shows, at the size a
phone actually shows them, rather than a pangram nobody will ever read.
"""

from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

FONT = (
    Path(__file__).resolve().parents[2]
    / "assets"
    / "fonts"
    / "StandardIronDisplay-Bold.ttf"
)

ROWS = [
    ("ABCDEFGHIJKLM", 92),
    ("NOPQRSTUVWXYZ", 92),
    ("0123456789", 92),
    (".…,:;!?-+×/%&#()[]'\"", 64),
    ("ÁÀÂÃÄÅÇÉÈÊËÍÌÎÏÑÓÒÔÕÖÚÙÛÜÝ¿¡", 56),
    ("ĞİŞ  DAĞ  İSTANBUL  ŞEHİR", 56),
    ("ĄĆĘŁŃÓŚŹŻ  ŚLĄSK  ZWYCIĘSTWO  WŁÓCZNIA", 56),
    ("АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ", 50),
    ("ЗАМА  КАРФАГЕН  ПОБЕДА  ДРУЖИНА", 56),
    ("", 20),
    ("STANDARD OF IRON", 88),
    ("ROME IS COMING", 76),
    ("240 VS 900", 96),
    ("KILL THE COMMANDER", 60),
    ("SURVIVE 60 SECONDS", 60),
    ("HANNIBAL", 88),
    ("CARTHAGE", 88),
    ("OUTNUMBERED 4:1", 76),
    ("", 20),
    ("HOLD 1 7 0 O S", 72),
]


def render(out: Path) -> Path:
    width, pad = 1500, 44
    height = pad * 2 + sum(size + 26 for _, size in ROWS)
    image = Image.new("RGB", (width, height), (16, 15, 14))
    draw = ImageDraw.Draw(image)

    y = pad
    for text, size in ROWS:
        if text:
            face = ImageFont.truetype(str(FONT), size)
            draw.text((pad, y), text, font=face, fill=(226, 219, 205))
        y += size + 26

    image.save(out)
    return out


if __name__ == "__main__":
    target = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("proof.png")
    print(f"wrote {render(target)}")
