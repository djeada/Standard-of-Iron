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
                scope_h: int, head: float = 0.0, transparent: bool = False) -> None:
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
        if transparent:
            shadow = text.filter(ImageFilter.GaussianBlur(10)).point(lambda v: min(255, v * 2))
            frame = Image.new("RGBA", (width, height), (0, 0, 0, 0))
            frame.paste(Image.new("RGBA", (width, height), (0, 0, 0, 215)), (0, 0), shadow)
            warm = Image.new("RGBA", (width, height), tuple(card.get("glow_ink", (255, 170, 90))) + (255,))
            frame.paste(warm, (0, 0), glow_img)
            frame.paste(Image.new("RGBA", (width, height), tuple(card.get("ink", INK)) + (255,)),
                        (0, 0), text)
            frame.save(out_dir / f"f{n + 1:05d}.png")
            continue
        frame = Image.new("RGB", (width, height), (0, 0, 0))
        tint = card.get("ink", INK)
        warm = Image.new("RGB", (width, height), tuple(int(c * 0.9) for c in card.get("glow_ink", (255, 170, 90))))
        frame.paste(warm, (0, 0), glow_img)
        frame.paste(Image.new("RGB", (width, height), tuple(tint)), (0, 0), text)
        top = (height - scope_h) // 2
        ImageDraw.Draw(frame).rectangle([0, 0, width, top - 1], fill=0)
        ImageDraw.Draw(frame).rectangle([0, top + scope_h, width, height], fill=0)
        frame.save(out_dir / f"f{n + 1:05d}.png")


GOLD = (212, 176, 104)


def render_caption(caption: dict, out_dir: Path, fps: int, width: int, height: int,
                   scope_h: int) -> int:
    """A caption over footage: tracked capitals that open as they fade in, a gold
    rule drawing out beneath, a soft shadow for legibility. Returns frame count."""
    out_dir.mkdir(parents=True, exist_ok=True)
    dur = float(caption["dur"])
    frames = int(round(dur * fps))
    ss = 2
    size = int(caption.get("size", 46))
    sub = caption.get("sub")
    top = (height - scope_h) // 2
    y = top + scope_h * float(caption.get("y", 0.80))
    for n in range(frames):
        t = n / fps
        a_in = _ease(t / 0.45)
        a_out = _ease((dur - t) / 0.4)
        alpha = a_in * a_out
        canvas = Image.new("L", (width * ss, height * ss), 0)
        rule = Image.new("L", (width * ss, height * ss), 0)
        d = ImageDraw.Draw(canvas)
        font = _font("display", size * ss)
        tracking = 0.22 + 0.10 * (1 - math.exp(-t / 0.9))
        draw_tracked(d, (width * ss / 2, y * ss), caption["text"], font, tracking,
                     int(255 * alpha))
        w = tracked_width(caption["text"], font, tracking)
        grow = _ease((t - 0.15) / 0.7)
        half = w * 0.36 * grow
        ry = (y + size * 0.78) * ss
        ImageDraw.Draw(rule).line([(width * ss / 2 - half, ry), (width * ss / 2 + half, ry)],
                                  fill=int(220 * alpha), width=2 * ss)
        if sub:
            sf = _font("text", int(size * 0.5) * ss)
            draw_tracked(d, (width * ss / 2, (y + size * 1.45) * ss), sub, sf, 0.18,
                         int(215 * alpha * _ease((t - 0.35) / 0.5)))
        text = canvas.resize((width, height), Image.LANCZOS)
        line = rule.resize((width, height), Image.LANCZOS)
        shadow = text.filter(ImageFilter.GaussianBlur(9)).point(lambda v: min(255, int(v * 1.6)))
        frame = Image.new("RGBA", (width, height), (0, 0, 0, 0))
        frame.paste(Image.new("RGBA", (width, height), (0, 0, 0, 190)), (0, 0), shadow)
        frame.paste(Image.new("RGBA", (width, height), GOLD + (255,)), (0, 0), line)
        frame.paste(Image.new("RGBA", (width, height), INK + (255,)), (0, 0), text)
        frame.save(out_dir / f"f{n + 1:05d}.png")
    return frames
