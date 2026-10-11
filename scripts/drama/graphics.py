"""Full-frame graphics for a drama episode: title, inserts and end card.

    python3 scripts/drama/graphics.py EPISODE_DIR --out DIR

Inserts are props the characters read (a casualty list, a transfer slip, an
order with a mark on it), drawn on parchment at 1920x1080. Colours and faces
come from the documentary style module so the two series share a look.
"""

import argparse
import json
import math
import random
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from documentary import style  # noqa: E402

W, H = 1920, 1080
INK = (52, 34, 20)
INK_FADED = (96, 70, 44)
WAX = (138, 28, 24)


def font(kind, size):
    return ImageFont.truetype(str(style.FONTS[kind]), size)


def tracked(draw, xy, text, fnt, fill, tracking=0.0, anchor="la"):
    if tracking == 0.0:
        draw.text(xy, text, font=fnt, fill=fill, anchor=anchor)
        return
    widths = [draw.textlength(ch, font=fnt) for ch in text]
    total = sum(widths) + tracking * fnt.size * (len(text) - 1)
    x, y = xy
    if anchor[0] == "m":
        x -= total / 2
    for ch, w in zip(text, widths):
        draw.text((x, y), ch, font=fnt, fill=fill, anchor="l" + anchor[1])
        x += w + tracking * fnt.size


def parchment(seed):
    rng = random.Random(seed)
    img = Image.new("RGB", (W, H), (30, 22, 15))
    sheet = Image.new("RGB", (W - 260, H + 140), (226, 205, 160))
    px = sheet.load()
    sw, sh = sheet.size
    for _ in range(9000):
        x, y = rng.randrange(sw), rng.randrange(sh)
        r = rng.randint(-18, 10)
        c = px[x, y]
        px[x, y] = (c[0] + r, c[1] + r, c[2] + r)
    sheet = sheet.filter(ImageFilter.GaussianBlur(1.6))
    stain = Image.new("L", sheet.size, 0)
    sd = ImageDraw.Draw(stain)
    for _ in range(14):
        cx, cy, rad = rng.randrange(sw), rng.randrange(sh), rng.randint(60, 260)
        sd.ellipse((cx - rad, cy - rad, cx + rad, cy + rad), fill=rng.randint(10, 34))
    stain = stain.filter(ImageFilter.GaussianBlur(60))
    sheet = Image.composite(Image.new("RGB", sheet.size, (176, 140, 92)), sheet, stain)
    edge = Image.new("L", sheet.size, 0)
    ImageDraw.Draw(edge).rectangle((0, 0, sw, sh), outline=255, width=40)
    edge = edge.filter(ImageFilter.GaussianBlur(40))
    sheet = Image.composite(Image.new("RGB", sheet.size, (150, 112, 70)), sheet, edge)
    sheet = sheet.rotate(-1.2, resample=Image.BICUBIC, expand=True, fillcolor=(30, 22, 15))
    img.paste(sheet, ((W - sheet.width) // 2, (H - sheet.height) // 2 + 20))
    return img


def vignette(img, strength=0.55):
    mask = Image.new("L", (W, H), 0)
    ImageDraw.Draw(mask).ellipse((-W * 0.15, -H * 0.25, W * 1.15, H * 1.25), fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(220))
    dark = Image.new("RGB", (W, H), (0, 0, 0))
    return Image.composite(img, Image.blend(img, dark, strength), mask)


def seal(draw, cx, cy, r, broken):
    draw.ellipse((cx - r, cy - r, cx + r, cy + r), fill=WAX)
    draw.ellipse((cx - r * 0.72, cy - r * 0.72, cx + r * 0.72, cy + r * 0.72), outline=(98, 16, 14), width=4)
    draw.text((cx, cy), "SPQR", font=font("display", int(r * 0.42)), fill=(98, 16, 14), anchor="mm")
    if broken:
        draw.line((cx - r * 1.05, cy - r * 0.2, cx + r * 0.2, cy + r * 0.1, cx + r * 1.05, cy - r * 0.35),
                  fill=(226, 205, 160), width=7)


def nail_mark(draw, cx, cy, r, width=7):
    draw.ellipse((cx - r, cy - r, cx + r, cy + r), outline=INK, width=width)
    draw.line((cx - r * 0.25, cy - r * 1.45, cx + r * 0.2, cy + r * 1.35), fill=INK, width=width)
    draw.line((cx - r * 0.55, cy - r * 1.45, cx + r * 0.05, cy - r * 1.45), fill=INK, width=width)


def casualty_list(names, highlight=None, footer=None, seed=7):
    img = parchment(seed)
    d = ImageDraw.Draw(img)
    tracked(d, (W / 2, 150), "RETURNS OF THE DEAD", font("display", 60), INK, 0.2, "mm")
    d.text((W / 2, 220), "a.d. V Kal. Sextiles  ·  killed in the service of the Republic",
           font=font("text", 36), fill=INK_FADED, anchor="mm")
    d.line((360, 262, W - 360, 262), fill=INK_FADED, width=2)
    y = 320
    for name, unit in names:
        colour = INK
        if highlight and name == highlight:
            d.rectangle((330, y - 34, W - 330, y + 34), fill=(205, 176, 118))
            colour = (40, 14, 8)
        d.text((380, y), name, font=font("text", 54), fill=colour, anchor="lm")
        d.text((W - 380, y), unit, font=font("text", 40), fill=INK_FADED, anchor="rm")
        y += 86
    if footer:
        d.text((W / 2, y + 30), footer, font=font("text", 44), fill=(110, 24, 18), anchor="mm")
    seal(d, W - 430, H - 190, 70, broken=True)
    return vignette(img)


def transfer_slip():
    img = parchment(31)
    d = ImageDraw.Draw(img)
    tracked(d, (W / 2, 200), "TRANSFER", font("display", 64), INK, 0.24, "mm")
    rows = [
        ("Name", "QUINTUS"),
        ("Rank", "hastatus, fourth maniple"),
        ("To", "quiet duty: the road guard"),
        ("Station", "the ninth milestone"),
    ]
    y = 320
    for label, value in rows:
        d.text((420, y), label, font=font("text", 40), fill=INK_FADED, anchor="lm")
        d.text((700, y), value, font=font("text", 58), fill=INK, anchor="lm")
        y += 100
    d.text((420, y + 40), "By order of", font=font("text", 40), fill=INK_FADED, anchor="lm")
    rng = random.Random(4)
    pts, x = [], 700
    while x < 1300:
        pts.append((x, y + 40 + rng.randint(-28, 28)))
        x += rng.randint(18, 40)
    d.line(pts, fill=(30, 22, 60), width=6, joint="curve")
    d.text((420, y + 150), "Requested by", font=font("text", 40), fill=INK_FADED, anchor="lm")
    d.text((700, y + 150), "Felix, the pay office", font=font("text", 58), fill=INK, anchor="lm")
    return vignette(img)


def order_with_mark():
    img = parchment(53)
    d = ImageDraw.Draw(img)
    tracked(d, (W / 2, 190), "ORDERS", font("display", 60), INK, 0.24, "mm")
    lines = ["Detached for the road guard.", "Rations: four hundred and twelve.", "Station: the ninth milestone"]
    y = 330
    for text in lines:
        d.text((420, y), text, font=font("text", 60), fill=INK, anchor="lm")
        y += 120
    nail_mark(d, 1420, 570, 62)
    seal(d, 1500, 870, 64, broken=False)
    return vignette(img, 0.7)


def title_card(series, number, title):
    img = Image.new("RGB", (W, H), style.COLORS["iron"])
    d = ImageDraw.Draw(img)
    rng = random.Random(9)
    for _ in range(140):
        x, y = rng.randrange(W), rng.randrange(H)
        r = rng.choice((1, 1, 2, 3))
        c = rng.choice((style.COLORS["ember"], style.COLORS["ember_core"], style.COLORS["gold"]))
        d.ellipse((x - r, y - r, x + r, y + r), fill=c)
    img = img.filter(ImageFilter.GaussianBlur(1.2))
    d = ImageDraw.Draw(img)
    tracked(d, (W / 2, 360), "STANDARD OF IRON", font("display", 44), style.COLORS["ink_dim"], 0.4, "mm")
    tracked(d, (W / 2, 470), series, font("display", 120), style.COLORS["ink"], 0.18, "mm")
    d.line((W / 2 - 360, 560, W / 2 + 360, 560), fill=style.COLORS["rule"], width=3)
    tracked(d, (W / 2, 640), f"EPISODE {number}", font("display", 40), style.COLORS["gold"], 0.3, "mm")
    tracked(d, (W / 2, 720), title.upper(), font("display", 72), style.COLORS["gold_bright"], 0.16, "mm")
    return vignette(img, 0.4)


def end_card(next_title):
    img = Image.new("RGB", (W, H), style.COLORS["iron"])
    d = ImageDraw.Draw(img)
    tracked(d, (W / 2, 420), "NEXT", font("display", 44), style.COLORS["ink_dim"], 0.4, "mm")
    tracked(d, (W / 2, 520), next_title.upper(), font("display", 96), style.COLORS["ink"], 0.18, "mm")
    d.line((W / 2 - 300, 610, W / 2 + 300, 610), fill=style.COLORS["rule"], width=3)
    tracked(d, (W / 2, 690), "STANDARD OF IRON  ·  THE DEAD COMPANY", font("display", 38),
            style.COLORS["gold"], 0.24, "mm")
    return img


def seal_closeup():
    img = parchment(71)
    d = ImageDraw.Draw(img)
    cx, cy, r = W // 2, H // 2 + 10, 250
    d.ellipse((cx - r - 30, cy - r + 20, cx + r - 30, cy + r + 20), fill=(110, 20, 18))
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=WAX)
    d.ellipse((cx - r * 0.78, cy - r * 0.78, cx + r * 0.78, cy + r * 0.78), outline=(92, 14, 12), width=10)
    tracked(d, (cx, cy - 60), "TRIB", font("display", 120), (96, 16, 14), 0.1, "mm")
    tracked(d, (cx, cy + 70), "MIL", font("display", 96), (96, 16, 14), 0.1, "mm")
    d.line((cx - r * 1.05, cy - 40, cx - 60, cy + 10, cx + 40, cy - 30, cx + r * 1.05, cy + 30), fill=(232, 210, 165), width=14)
    d.ellipse((cx + 70, cy - r * 0.55, cx + 150, cy - r * 0.35), fill=(150, 34, 30))
    d.text((W / 2, H - 190), "a tribune's ring, pressed in a hurry", font=font("text", 46), fill=INK_FADED, anchor="mm")
    return vignette(img, 0.6)


def rosters():
    img = parchment(83)
    d = ImageDraw.Draw(img)
    tracked(d, (W / 2, 250), "TRANSFERS, LAST TEN DAYS", font("display", 50), INK, 0.18, "mm")
    rows = [("VIII Kal.", "Petronius, III maniple", "road guard"), ("VII Kal.", "Calvus, VI maniple", "road guard"),
            ("VII Kal.", "Herennius, II maniple", "road guard"), ("VI Kal.", "Decimus Aurelius, III", "road guard"),
            ("VI Kal.", "Quintus, IV maniple", "road guard"), ("V Kal.", "...and 407 more", "road guard")]
    y = 350
    for when, who, where in rows:
        d.text((360, y), when, font=font("text", 42), fill=INK_FADED, anchor="lm")
        d.text((620, y), who, font=font("text", 50), fill=INK, anchor="lm")
        d.text((W - 380, y), where, font=font("text", 42), fill=(120, 30, 22), anchor="rm")
        y += 78
    return vignette(img, 0.6)


def road_map():
    img = parchment(97)
    d = ImageDraw.Draw(img)
    tracked(d, (W / 2, 250), "THE ROAD WITH NO NAME", font("display", 52), INK, 0.2, "mm")
    pts = [(330, 820), (560, 740), (780, 700), (1000, 610), (1220, 560), (1460, 470), (1620, 420)]
    d.line(pts, fill=INK, width=10, joint="curve")
    d.line((250, 880, 520, 820, 760, 860), fill=(70, 100, 140), width=12)
    d.text((300, 900), "the river", font=font("text", 38), fill=(70, 100, 140), anchor="lm")
    for i, (x, y) in enumerate(pts[1:], start=4):
        d.ellipse((x - 9, y - 9, x + 9, y + 9), fill=INK)
        d.text((x, y + 38), ["IV", "V", "VI", "VII", "VIII", "IX"][i - 4], font=font("text", 34), fill=INK_FADED, anchor="mm")
    for gx, gy in ((900, 655), (1340, 515)):
        d.rectangle((gx - 26, gy - 46, gx + 26, gy + 46), outline=(120, 30, 22), width=8)
        d.line((gx - 26, gy, gx + 26, gy), fill=(120, 30, 22), width=8)
    d.text((1120, 760), "gates (barred outside)", font=font("text", 38), fill=(120, 30, 22), anchor="mm")
    for tx, ty in ((1500, 360), (1580, 330), (1660, 380)):
        d.rectangle((tx - 18, ty - 30, tx + 18, ty + 18), fill=INK_FADED)
    d.text((1580, 290), "old tombs", font=font("text", 38), fill=INK_FADED, anchor="mm")
    nail_mark(d, 1620, 420, 34, width=6)
    return vignette(img, 0.6)


def render(episode_dir, out):
    edit = json.loads((episode_dir / "edit.json").read_text())
    out.mkdir(parents=True, exist_ok=True)
    names = [
        ("Gaius Petronius", "III maniple"),
        ("Marcus Calvus", "VI maniple"),
        ("Aulus Herennius", "II maniple"),
        ("Septimus Varro", "VIII maniple"),
        ("Felix, librarius", "the pay office"),
    ]
    graphics = {
        "title": title_card("THE DEAD COMPANY", edit["episode"].lstrip("ep0") or "1", edit["title"]),
        "endcard": end_card(edit.get("next", "The Funeral")),
        "list_names": casualty_list(names[:4], seed=7),
        "list_felix": casualty_list(names, highlight="Felix, librarius",
                                    footer="Pay ceases from this day.", seed=7),
        "slip": transfer_slip(),
        "seal": seal_closeup(),
        "rosters": rosters(),
        "map": road_map(),
        "mark": order_with_mark(),
        "black": Image.new("RGB", (W, H), (0, 0, 0)),
    }
    for name, image in graphics.items():
        image.save(out / f"{name}.png")
    print(f"{len(graphics)} graphics in {out}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("episode_dir")
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    render(Path(args.episode_dir), Path(args.out))
