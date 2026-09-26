"""Typography for the trailer: rendered frame by frame with PIL.

Cards are black-matted frames with tracked capitals in the game's own display
face (``assets/fonts/StandardIronDisplay-Bold.ttf``) and, for small lines, EB
Garamond. Tracking, glow and fades are computed per frame, so the lettering is
identical on every machine and never depends on an ffmpeg drawtext build.

A card is described in ``cut.json``::

    {"card": {"lines": [{"text": "STANDARD OF IRON", "size": 96, "tracking": 0.32,
                          "at": 0.4, "fade": 1.6}],
              "fade_out": 0.0}}
"""

from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

REPO = Path(__file__).resolve().parents[2]
FONTS = {
    "display": REPO / "assets" / "fonts" / "StandardIronDisplay-Bold.ttf",
    "text": REPO / "assets" / "fonts" / "EBGaramond12-Bold.ttf",
}
INK = (231, 222, 204)


def _font(face: str, size: int) -> ImageFont.FreeTypeFont:
    return ImageFont.truetype(str(FONTS[face]), size)


def tracked_width(text: str, font: ImageFont.FreeTypeFont, tracking: float) -> float:
    gap = tracking * font.size
    return sum(font.getlength(ch) for ch in text) + gap * max(0, len(text) - 1)


def draw_tracked(draw: ImageDraw.ImageDraw, centre: tuple[float, float], text: str,
                 font: ImageFont.FreeTypeFont, tracking: float, fill) -> None:
    gap = tracking * font.size
    x = centre[0] - tracked_width(text, font, tracking) / 2
    ascent, descent = font.getmetrics()
    y = centre[1] - (ascent - descent) / 2 - descent * 0.3
    for ch in text:
        draw.text((x, y), ch, font=font, fill=fill)
        x += font.getlength(ch) + gap


def _ease(t: float) -> float:
    t = max(0.0, min(1.0, t))
    return t * t * (3 - 2 * t)


def render_card(card: dict, out_dir: Path, total: float, fps: int, width: int, height: int,
                scope_h: int, head: float = 0.0) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    frames = int(round(total * fps))
    lines = card.get("lines", [])
    fade_out = float(card.get("fade_out", 0.0))
    ss = 2
    for n in range(frames):
        t = n / fps - head
        canvas = Image.new("L", (width * ss, height * ss), 0)
        glow_layer = Image.new("L", (width * ss, height * ss), 0)
        for line in lines:
            at = float(line.get("at", 0.0))
            fade = float(line.get("fade", 1.0))
            alpha = _ease((t - at) / fade) if fade > 0 else (1.0 if t >= at else 0.0)
            if fade_out > 0:
                alpha *= _ease((total - head - t) / fade_out)
            end = line.get("until")
            if end is not None:
                alpha *= _ease((float(end) - t) / float(line.get("fade_out", 0.6)))
            if alpha <= 0.002:
                continue
            size = int(line.get("size", 64) * ss)
            drift = float(line.get("drift", 0.0))
            if drift:
                grow = 1.0 + drift * (1.0 - math.exp(-max(0.0, t - at) / 4.0))
                size = int(size * grow)
            font = _font(line.get("face", "display"), size)
            y = float(line.get("y", 0.5)) * height * ss
            tracking = float(line.get("tracking", 0.25))
            if line.get("track_open"):
                tracking += float(line["track_open"]) * (1 - math.exp(-max(0.0, t - at) / 3.0))
            value = int(255 * alpha * float(line.get("opacity", 1.0)))
            draw_tracked(ImageDraw.Draw(canvas), (width * ss / 2, y), line["text"], font,
                         tracking, value)
            glow = float(line.get("glow", 0.0))
            if glow > 0:
                draw_tracked(ImageDraw.Draw(glow_layer), (width * ss / 2, y), line["text"],
                             font, tracking, int(value * glow))
        text = canvas.resize((width, height), Image.LANCZOS)
        glow_img = glow_layer.resize((width, height), Image.LANCZOS).filter(
            ImageFilter.GaussianBlur(14))
        frame = Image.new("RGB", (width, height), (0, 0, 0))
        tint = card.get("ink", INK)
        warm = Image.new("RGB", (width, height), tuple(int(c * 0.9) for c in card.get("glow_ink", (255, 170, 90))))
        frame.paste(warm, (0, 0), glow_img)
        frame.paste(Image.new("RGB", (width, height), tuple(tint)), (0, 0), text)
        top = (height - scope_h) // 2
        ImageDraw.Draw(frame).rectangle([0, 0, width, top - 1], fill=0)
        ImageDraw.Draw(frame).rectangle([0, top + scope_h, width, height], fill=0)
        frame.save(out_dir / f"f{n + 1:05d}.png")
