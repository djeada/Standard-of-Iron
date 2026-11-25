#!/usr/bin/env python3
"""Rome, as map_cli operations. Run from the repo root with a Python that has
shapely; writes tools/map_editor/plans/rome.ops.json and the consul's walk for
the city film.

North is -z, east is +x. The Tiber comes in from the north, swings west round
the Campus Martius, splits about its island under the Capitol and runs down to
the sea along the south edge, where Ostia stands. The Servian wall follows the
hills; inside it every quarter has its own warped street web.
"""
import json
import math
import os
import random

from organic import SHOP_W, Town, blob, facing_of, rect
from planlib import DOCK_SETBACK, SEA_LEVEL, Plan, spline
from shapely.geometry import LineString, Point, Polygon

SIZE = 801
p = Plan("roman_republic")
t = Town(p, seed=753)
rng = random.Random(753)
ROMAN = (("tenement", 6), ("", 1))

p.map(
    name="Rome",
    description="The city of seven hills on the Tiber: the Forum under the Capitol, the imperial fora, "
    "the Colosseum and the Circus Maximus, the cattle market by the river, the crowded "
    "Subura, the gardens of Sallust, Maecenas and Lucullus with their pools, the Campus "
    "Martius in the river bend, Trastevere under the Janiculum, villas in their olive "
    "groves, a legion drawn up outside the Servian wall, and Ostia where the Tiber meets "
    "the sea.",
    biome={
        "ground_type": "grass_dry",
        "seed": 753,
        "procedural_trees_enabled": False,
        "procedural_boulders_enabled": False,
        "procedural_iron_ore_enabled": False,
        "plant_density": 0.35,
        "height_noise": [0.04, 0.05],
        "tree_mix": {"palm": 0},
    },
    environment={
        "lighting_profile": "mediterranean_summer",
        "start_time": 10.5,
        "time_mode": "locked",
        "day_length_seconds": 1800,
    },
    rain={"enabled": False, "intensity": 0, "type": "rain"},
    camera={
        "center": [-30, 0, 0],
        "distance": 95,
        "tilt_deg": 48,
        "yaw": 225,
        "fov_y": 45,
        "near": 1,
        "far": 2000,
    },
    victory={
        "type": "elimination",
        "key_structures": ["barracks"],
        "defeat_conditions": [
            "no_key_structures",
            "no_commander",
            "only_commander_remaining",
        ],
    },
    max_troops_per_player=8100,
)


p.lake(-40, 880, 2200, 1080, height=SEA_LEVEL, shape="round")
TIBER = [
    [-20, -420],
    [-50, -340],
    [-110, -296],
    [-200, -262],
    [-250, -205],
    [-246, -136],
    [-206, -84],
    [-170, -40],
    [-156, 6],
    [-156, 52],
    [-160, 104],
    [-172, 160],
    [-196, 214],
    [-224, 268],
    [-238, 330],
    [-244, 420],
]
TIBER_PTS = spline(TIBER, 12)
p.river(TIBER_PTS, 24, SEA_LEVEL)
ARM = spline([[-158, -4], [-138, 8], [-130, 30], [-136, 54], [-158, 66]], 6)
p.river(ARM, 10, SEA_LEVEL)


HILLS = {
    "capitoline": (-100, -44, 58, 42, 3.4, 30),
    "palatine": (-36, 58, 96, 66, 2.8, -12),
    "aventine": (-92, 182, 104, 78, 2.8, 22),
    "caelian": (78, 128, 118, 60, 2.0, -12),
    "esquiline": (112, -6, 130, 96, 2.0, 8),
    "viminal": (66, -112, 96, 40, 1.6, -28),
    "quirinal": (-12, -170, 150, 52, 2.2, -24),
    "pincian": (-12, -318, 140, 70, 2.4, -14),
    "janiculum": (-338, 40, 96, 260, 4.6, 8),
}
for x, z, w, d, h, r in HILLS.values():
    p.rise(x, z, w, d, h, 0.55, r)


WALL = [
    (-112, 206),
    (-112, -66),
    (-96, -118),
    (-62, -202),
    (-20, -236),
    (100, -236),
    (170, -172),
    (206, -98),
    (206, 40),
    (186, 122),
    (150, 188),
    (100, 226),
    (16, 226),
    (-40, 254),
    (-96, 244),
]
city = t.wall("servian", WALL, closed=True, tower_spacing=48)
inside = city.buffer(-6)
GATES = {
    "porta_trigemina": (-112, 150),
    "porta_flumentana": (-112, 0),
    "porta_salaria": (24, -236),
    "porta_collina": (78, -236),
    "porta_esquilina": (206, -40),
    "porta_caelimontana": (206, 20),
    "porta_capena": (80, 226),
    "porta_fontinalis": (-112, -28),
    "porta_navalis": (-112, 96),
}
for name, (gx, gz) in GATES.items():
    t.gate(name, gx, gz)


FORUM = (-50, 2)
FR = 33
fw = t.plaza(
    "forum", FORUM[0], FORUM[1], 86, 26, rotation=FR, sides="ns", grand="ns", statues=0
)
for u, tag, rot in [(-50, "temple_of_saturn", 90), (50, "temple_of_divus_julius", 270)]:
    x, z = fw(u, 0)
    t.building(tag, "temple", x, z, (rot - FR) % 360)
for k in range(7):
    x, z = fw(-36 + k * 12, (-1) ** k * 5)
    p.prop("statue", x, z, 1.1)

for u, v in [(-40, -10), (-40, 10), (40, -10), (40, 10)]:
    for du, dv in [(0, 0), (2.2, 0.0), (1.1, 1.9)]:
        x, z = fw(u + (du if u < 0 else -du), v + (dv if v < 0 else -dv))
        p.prop("statue", x, z, 1.0)
for u, sgn in [(-42.5, 1), (42.5, -1)]:
    for v in (-4.2, 4.2):
        x, z = fw(u + sgn * 1.5, v)
        p.prop("statue", x, z, 1.25)
x0, z0 = fw(-34, 0)
x1, z1 = fw(34, 0)
t.street([(x0, z0), (x1, z1)], 3.0, "stone", curve=False, mark=False)
p.crowd(
    "forum_crowd",
    FORUM[0] - 26,
    FORUM[1] - 8,
    FORUM[0] + 26,
    FORUM[1] + 10,
    64,
    seed=11,
    radius=18,
)

t.building("temple_of_jupiter", "temple", -104, -50, 120)
t.building("temple_of_juno_moneta", "temple", -86, -60, 210)
t.scatter(
    Polygon([(-128, -70), (-76, -70), (-72, -22), (-126, -24)]),
    18,
    (("cypress_tree", 3), ("pine_tree", 1)),
    spacing=4.0,
    seed=4,
)


fc = t.plaza("forum_caesaris", -46, -54, 44, 18, rotation=FR, sides="ns")
x, z = fc(-28, 0)
t.building("temple_of_venus_genetrix", "temple", x, z, (90 - FR) % 360)
fa = t.plaza("forum_augusti", -6, -64, 40, 22, rotation=-30, sides="ns", grand="ns")
x, z = fa(26, 0)
t.building("temple_of_mars_ultor", "temple", x, z, (270 + 30) % 360)
ft = t.plaza(
    "forum_traiani", -40, -98, 54, 30, rotation=30, sides="ns", grand="s", statues=3
)
t.arc("mercati", -18, -112, 26, 20, 200, 340, rotation=30, outward=False)


CX, CZ = 46, 30
CRX, CRZ = 32, 26


pts = []
for k in range(36):
    deg = k * 10 + 5
    if min(abs(((deg - axis) + 180) % 360 - 180) for axis in (0, 90, 180, 270)) < 12:
        continue
    pts.append(
        (CX + CRX * math.cos(math.radians(deg)), CZ + CRZ * math.sin(math.radians(deg)))
    )
    if deg == 75:
        pts += [(CX + 4, CZ + CRZ), (CX - 4, CZ + CRZ)]
    if deg == 165:
        pts += [(CX - CRX, CZ + 4), (CX - CRX, CZ - 4)]
    if deg == 255:
        pts += [(CX - 4, CZ - CRZ), (CX + 4, CZ - CRZ)]
    if deg == 345:
        pts += [(CX + CRX, CZ - 4), (CX + CRX, CZ + 4)]
t.wall("colosseum", pts, closed=True, tower_spacing=0, style="masonry", clear=2.0)
for gx, gz, name in (
    (CX - CRX, CZ, "porta_triumphalis"),
    (CX + CRX, CZ, "porta_libitinensis"),
    (CX, CZ - CRZ, "colosseum_north"),
    (CX, CZ + CRZ, "colosseum_south"),
):
    t.gate(name, gx, gz)

t.arc(
    "colosseum_arcade",
    CX,
    CZ,
    CRX + 4.0,
    CRZ + 4.0,
    0,
    360,
    outward=True,
    style="basilica",
    gaps=(0, 90, 180, 270),
    gap_width=40,
)
t.arc(
    "colosseum_tier_high",
    CX,
    CZ,
    26.5,
    20.5,
    0,
    360,
    outward=False,
    style="basilica",
    gaps=(0, 90, 180, 270),
    gap_width=60,
)
t.arc(
    "colosseum_tier_low",
    CX,
    CZ,
    20.5,
    14.5,
    0,
    360,
    outward=False,
    style="portico",
    gaps=(0, 90, 180, 270),
    gap_width=76,
)
for ax, az, bx, bz in (
    (CX - CRX - 14, CZ, CX - CRX + 1, CZ),
    (CX + CRX + 14, CZ, CX + CRX - 1, CZ),
    (CX, CZ - CRZ - 12, CX, CZ - CRZ + 1),
    (CX, CZ + CRZ + 12, CX, CZ + CRZ - 1),
):
    t.street([(ax, az), (bx, bz)], 4.0, "stone", curve=False)
t.keep_out(blob(CX, CZ, CRX + 9, CRZ + 9, wobble=0.0))
t.building("temple_of_venus_and_rome", "temple", 20, -24, 300)
p.crowd(
    "colosseum_crowd",
    CX - CRX - 30,
    CZ - 4,
    CX - CRX - 14,
    CZ + 12,
    34,
    seed=12,
    radius=10,
)
p.crowd("arena_crowd", CX - 4, CZ - 10, CX + 16, CZ + 10, 24, seed=13, radius=8)
p.crowd("sacra_crowd", -34, 22, -4, 42, 30, seed=14, radius=10)
p.prop("statue", CX - CRX - 10, CZ - 9, 1.8)


CIRC_C, CIRC_R = (-48, 122), 28
cr = math.radians(CIRC_R)
cc, cs = math.cos(cr), math.sin(cr)


def circ(u, v):
    return (CIRC_C[0] + cc * u - cs * v, CIRC_C[1] + cs * u + cc * v)


t.run("circus_n", circ(-58, -14), circ(48, -14), facing_of(-cs, cc))
t.run("circus_s", circ(-58, 14), circ(48, 14), facing_of(cs, -cc))
t.arc("circus_turn", *circ(48, 0), 14, 14, -90 + CIRC_R, 90 + CIRC_R)
t.run("carceres", circ(-62, -12), circ(-62, 12), facing_of(cc, cs), style="basilica")
for k in range(10):
    x, z = circ(-44 + k * 10, 0)
    t.tree("statue" if k % 3 == 0 else "cypress_tree", x, z, 0.9)
t.keep_out(Polygon([circ(-68, -22), circ(66, -22), circ(66, 22), circ(-68, 22)]))
p.crowd(
    "circus_crowd",
    CIRC_C[0] - 30,
    CIRC_C[1] - 4,
    CIRC_C[0] + 30,
    CIRC_C[1] + 4,
    16,
    seed=5,
    radius=16,
)


t.building("temple_of_apollo", "temple", -60, 52, 200)
pal = t.plaza(
    "area_palatina", -26, 66, 30, 18, rotation=-12, sides="we", style="flagstone"
)
for k, (bx, bz, rot) in enumerate([(-4, 42, -12), (-58, 82, -12)]):
    p.block(f"domus_palatina_{k}", bx, bz, 5, 2, rot, alley=2, style="")
    t.keep_out(
        rect(
            bx + 10,
            bz + 9,
            26,
            22,
            math.cos(math.radians(rot)),
            math.sin(math.radians(rot)),
        )
    )
t.pool_garden("palatine_gardens", -14, 92, r=13, seed=7)


BT = (112, 46)
bw = t.plaza(
    "thermae_traiani",
    BT[0],
    BT[1],
    40,
    30,
    rotation=8,
    sides="nswe",
    grand="n",
    style="flagstone",
)
p.lake(BT[0], BT[1], 14, 9, rotation=8, height=-0.35, shape="basin", calm=True)
for u, v in [(-14, -9), (14, -9), (-14, 9), (14, 9)]:
    x, z = bw(u, v)
    t.tree("cypress_tree", x, z, 1.0)


MAEC = blob(160, -70, 34, 44, seed=21, rotation=10).intersection(inside)
t.pool_garden("maecenas_pool", 160, -76, r=15, seed=21)
t.park("horti_maecenatis", MAEC, seed=21, statues=5, cross=0)
t.keep_out(MAEC)

t.plaza("macellum", 54, -84, 22, 20, rotation=-18, sides="nswe", statues=1)
t.building("temple_of_tellus", "temple", 108, -52, 180)
t.building("castra_urbana", "barracks", 166, 100, 300, max_population=120)
t.building("castra_peregrina", "barracks", 120, 160, 330, max_population=120)


EZ0, EZ1 = 104, 140
bank = p.shore([-114, (EZ0 + EZ1) / 2], [-1, 0])[0]
t.run(
    "emporium_quay",
    (bank + DOCK_SETBACK, EZ0),
    (bank + DOCK_SETBACK, EZ1),
    270,
    style="dock",
)
t.run("porticus_aemilia", (-112 - 8.5, EZ0), (-112 - 8.5, EZ1), 270, style="portico")
t.street([(bank + 9.5, EZ0 - 6), (bank + 9.5, EZ1 + 6)], 4.4, "flagstone", curve=False)
for qz in (EZ0 - 3.5, EZ1 + 3.5):
    t.street(
        [(bank + 9.5, qz), (bank - 2.5, qz)], 3.0, "flagstone", curve=False, mark=False
    )
for k in range(6):
    p.prop("supply_cart", round(bank + 6, 1), round(EZ0 + 4 + k * 6.5, 1), 1.0, 90)
p.crowd("wharf_crowd", bank + 5, EZ0 + 6, bank + 12, EZ1, 32, seed=7, radius=10)
t.building("temple_of_portunus", "temple", -128, 78, 270)

BOARIUM = (-90, 84)
p.crowd("boarium_crowd", -96, 60, -72, 86, 50, seed=17, radius=9)


def span(pts, width, near, deck=10.0):
    ea, eb = t.river_bridge(pts, width, near, deck=deck)
    east, west = (ea, eb) if ea[0] > eb[0] else (eb, ea)
    return east, west


BR = {}
BR["aemilius"] = span(TIBER_PTS, 24, (-160, 96))
BR["sublicius"] = span(TIBER_PTS, 24, (-172, 172))
BR["aelius"] = span(TIBER_PTS, 24, (-246, -150))

BR["fabricius"] = span(ARM, 10, (-130, 33), deck=8.0)
BR["cestius"] = span(TIBER_PTS, 24, (-156, 30), deck=8.0)


def approach(name, side, to, width=4.6):
    east, west = BR[name]
    end = east if side == "e" else west
    other = west if side == "e" else east
    vx, vz = other[0] - end[0], other[1] - end[1]
    n = math.hypot(vx, vz)
    onto = (end[0] + vx / n * 4.0, end[1] + vz / n * 4.0)
    return t.street([to, end, onto], width, "stone", curve=False)


for name in ("aemilius", "sublicius", "fabricius"):
    approach(name, "e", (BR[name][0][0] + 8, BR[name][0][1]))
approach("aelius", "e", (BR["aelius"][0][0] + 10, BR["aelius"][0][1]))
t.building("temple_of_aesculapius", "temple", -147, 14, 0)

t.street(
    [
        (BR["fabricius"][0][0] + 8, BR["fabricius"][0][1]),
        (-120, 14),
        (-118, 2),
        (-112, 0),
    ],
    4.6,
    "stone",
)
approach("fabricius", "w", BR["cestius"][0], 4.0)
approach("cestius", "e", BR["fabricius"][1], 4.0)
for name in ("aemilius", "sublicius", "aelius", "cestius"):
    approach(name, "w", (BR[name][1][0] - 8, BR[name][1][1]))
w_main = BR["cestius"][1]
vw = BR["aelius"][1]
VZ = vw[1]

MAIN = 5.4
streets = []

streets.append(t.street([fw(43, 10), fw(60, 10), (CX - CRX - 14, CZ)], MAIN, "stone"))
streets.append(
    t.street(
        [
            (CX - CRX - 8, CZ),
            (CX - CRX - 6, CZ - 20),
            (30, -6),
            (100, -4),
            (150, -24),
            (206, -40),
        ],
        MAIN,
        "stone",
    )
)
streets.append(
    t.street(
        [fw(10, -18), (14, -40), (28, -62), (28, -102), (56, -130), (78, -236)],
        MAIN,
        "stone",
    )
)
streets.append(
    t.street([(-70, -84), (-40, -130), (-8, -190), (24, -236)], MAIN, "stone")
)
streets.append(
    t.street([(CX, CZ + CRZ + 12), (60, 110), (80, 200), (80, 226)], MAIN, "stone")
)
streets.append(
    t.street([(CX + CRX + 14, CZ), (130, 40), (170, 26), (206, 20)], 4.8, "stone")
)
streets.append(t.street([fw(-30, 16), (-98, 28), (-112, 0)], MAIN, "stone"))

market_street = t.street(
    [
        (BR["aemilius"][0][0] + 8, BR["aemilius"][0][1]),
        (-100, 96),
        (-84, 72),
        (-70, 50),
        fw(-10, 16),
    ],
    5.0,
    "flagstone",
)
streets.append(t.street([circ(-58, 18), (-108, 150), (-112, 150)], 4.8, "stone"))
streets.append(
    t.street(
        [(-106, 150), (-90, 200), (-60, 222), (10, 222), (60, 222), (80, 226)],
        4.4,
        "stone",
    )
)
streets.append(
    t.street([(-112, -28), (-104, -40), (-84, -36), fw(-40, -10)], 4.6, "stone")
)
for line in streets:
    if line is not None:
        t.later(
            t.frontage,
            "via",
            line,
            MAIN,
            region=inside,
            styles=(("tenement", 5), ("", 1)),
            alley=(6, 12),
        )

if market_street is not None:
    t.later(
        t.frontage,
        "boarium_stalls",
        market_street,
        5.0,
        region=inside,
        kind="marketplace",
        styles=(("", 3), ("portico", 1)),
        alley=(2, 3),
        start=4.0,
    )
    t.later(t.avenue, market_street, spacing=14, offset=7.5, kind="olive_tree")
if streets[0] is not None:
    t.later(
        t.frontage,
        "sacra_shops",
        streets[0],
        MAIN,
        region=inside,
        kind="marketplace",
        styles=(("portico", 1.0),),
        alley=(3, 5),
    )


QUARTERS = [
    (
        "subura",
        Polygon([(-20, -40), (30, -60), (96, -120), (108, -60), (90, -10), (30, 0)]),
        32,
        25,
        1,
    ),
    (
        "viminal",
        Polygon([(30, -150), (100, -200), (160, -150), (130, -100), (96, -120)]),
        -30,
        27,
        2,
    ),
    (
        "quirinal",
        Polygon(
            [(-80, -130), (-50, -200), (-10, -230), (40, -232), (30, -150), (-10, -120)]
        ),
        -24,
        24,
        3,
    ),
    (
        "esquiline",
        Polygon([(108, -60), (140, -120), (196, -110), (200, 0), (150, 10), (100, 0)]),
        6,
        26,
        4,
    ),
    (
        "caelian",
        Polygon(
            [
                (70, 70),
                (140, 70),
                (196, 60),
                (180, 130),
                (130, 190),
                (80, 200),
                (40, 150),
            ]
        ),
        -12,
        25,
        5,
    ),
    ("velabrum", Polygon([(-108, 10), (-70, 30), (-64, 80), (-108, 86)]), 64, 22, 6),
    (
        "aventine",
        Polygon(
            [
                (-108, 150),
                (-60, 150),
                (20, 170),
                (40, 200),
                (10, 218),
                (-60, 240),
                (-106, 236),
            ]
        ),
        24,
        25,
        7,
    ),
    ("velia", Polygon([(-10, 0), (40, -6), (60, -40), (-4, -26)]), -10, 22, 8),
]
for name, poly, ang, major, seed in QUARTERS:
    region = poly.intersection(inside)
    lanes = t.lanes(
        region,
        angle=ang,
        major=major + (6 if name == "subura" else 0),
        minor=(30, 56),
        warp=(7.0, 150.0),
        wiggle=2.2,
        seed=seed,
    )
    if name == "subura":
        t.later(
            t.build_along,
            name,
            lanes,
            region=region,
            styles=(("tenement", 3), ("", 2)),
            rows=2,
        )
    else:
        t.later(t.build_along, name, lanes, region=region, styles=ROMAN)
    t.later(t.courts, region, density=0.011, seed=seed)
rest = inside
for _, poly, *_x in QUARTERS:
    rest = rest.difference(poly)
for k, piece in enumerate(getattr(rest, "geoms", [rest])):
    if piece.area < 1500:
        continue
    lanes = t.lanes(piece, angle=rng.choice([-20, 10, 35]), major=26, seed=40 + k)
    t.later(t.build_along, f"rest{k}", lanes, region=piece, styles=ROMAN)
    t.later(t.courts, piece, density=0.010, seed=40 + k)


CAMPUS = Polygon(
    [
        (-112, -70),
        (-150, -52),
        (-196, -96),
        (-228, -150),
        (-230, -200),
        (-196, -240),
        (-120, -270),
        (-70, -262),
        (-96, -124),
    ]
).buffer(-4)
flam = t.street(
    [
        (-112, -28),
        (-124, -60),
        (-122, -80),
        (-106, -140),
        (-84, -200),
        (-70, -300),
        (-52, -400),
    ],
    6.0,
    "stone",
)
pan = t.plaza("piazza_pantheon", -140, -150, 30, 20, rotation=0, sides="we")
t.building("pantheon", "temple", -140, -168, 180)
t.plaza("saepta", -114, -178, 18, 50, rotation=0, sides="we", grand="we")
TH = (-176, -96)
t.arc(
    "theatrum_pompei",
    TH[0],
    TH[1],
    22,
    22,
    180 + 20,
    360 - 20,
    outward=False,
    rotation=-35,
)
QP = blob(-150, -76, 20, 12, seed=3, wobble=0.0, rotation=-35)
t.keep_out(blob(TH[0], TH[1], 28, 28, wobble=0.0))
t.park("porticus_pompei", QP, seed=3, statues=4, cross=1, loop=False)
t.building("stadium_domitiani_n", "marketplace", -200, -180, 90, style="portico")
SD = (-188, -196)
t.arc(
    "stadium_domitiani",
    SD[0],
    SD[1],
    12,
    34,
    0,
    360,
    outward=True,
    gaps=(90, 270),
    gap_width=24,
)
t.street(
    [(SD[0], SD[1] - 44), (SD[0], SD[1] - 30)], 4.0, "stone", curve=False, mark=False
)
t.street(
    [(SD[0], SD[1] + 30), (SD[0], SD[1] + 44)], 4.0, "stone", curve=False, mark=False
)
t.keep_out(blob(SD[0], SD[1], 16, 38, wobble=0.0))
MA = (-120, -238)
p.rise(MA[0], MA[1], 30, 30, 2.4, 0.35)
for k in range(16):
    a = 2 * math.pi * k / 16
    t.tree("cypress_tree", MA[0] + 12 * math.cos(a), MA[1] + 12 * math.sin(a), 1.15)
p.prop("statue", MA[0], MA[1], 1.4)
t.keep_out(Point(MA).buffer(16))
t.park(
    "nemus_augusti",
    blob(-160, -232, 26, 18, seed=9).intersection(CAMPUS),
    seed=9,
    statues=2,
    cross=1,
)
t.building("ara_pacis", "temple", -96, -226, 270)
campus_rest = CAMPUS.difference(blob(-160, -232, 30, 22, seed=9))
lanes = t.lanes(
    campus_rest, angle=-8, major=30, minor=(36, 70), warp=(6, 160), seed=11, skip=0.3
)
t.later(t.build_along, "campus", lanes, region=campus_rest, styles=ROMAN, alley=(4, 8))
t.later(t.courts, campus_rest, density=0.012, seed=12)
if flam is not None:
    t.later(t.avenue, flam, spacing=10, offset=6.5, kind="pine_tree", scale=1.05)


SALL = blob(120, -300, 70, 48, seed=31, rotation=-10)
t.pool_garden("sallust_pool", 120, -296, r=18, seed=31)
t.building("temple_of_venus_erycina", "temple", 120, -328, 0)
t.park("horti_sallustiani", SALL, seed=31, statues=6, cross=0, density=0.012)
LUC = blob(-16, -318, 60, 36, seed=32, rotation=-14)
t.pool_garden("lucullus_pool", -16, -306, r=14, seed=32, ring="pine_tree")
t.park("horti_luculliani", LUC, seed=32, statues=5, cross=0, density=0.013)
CAES = blob(-262, 226, 50, 40, seed=33)
t.pool_garden("caesar_pool", -262, 226, r=16, seed=33)
t.park("horti_caesaris", CAES, seed=33, statues=5, cross=0, density=0.012)
JAN = blob(-338, 40, 40, 120, seed=34, rotation=8)
t.scatter(
    JAN,
    150,
    (("pine_tree", 5), ("cypress_tree", 1), ("olive_tree", 2)),
    spacing=6.0,
    seed=35,
)
t.park(
    "horti_agrippinae",
    blob(-318, -170, 44, 40, seed=36),
    seed=36,
    statues=3,
    cross=2,
    density=0.010,
)


TRANS = Polygon(
    [
        (-186, -60),
        (-176, 0),
        (-188, 70),
        (-206, 150),
        (-232, 190),
        (-296, 180),
        (-300, 120),
        (-292, -40),
        (-250, -70),
    ]
)
TRANS = TRANS.difference(CAES.buffer(6))
tv = t.street(
    [(w_main[0] - 8, w_main[1]), (-220, 34), (-262, 50), (-300, 60)], 5.0, "stone"
)
tv2 = t.street(
    [(BR["aemilius"][1][0] - 8, BR["aemilius"][1][1]), (-220, 110), (-250, 140)],
    4.6,
    "stone",
)
tv3 = t.street(
    [
        (BR["sublicius"][1][0] - 8, BR["sublicius"][1][1]),
        (-226, 176),
        (-250, 140),
        (-290, 100),
    ],
    4.6,
    "stone",
)
for line in (tv, tv2, tv3):
    if line is not None:
        t.later(t.frontage, "trans_main", line, 5.0, region=TRANS, styles=ROMAN)
lanes = t.lanes(TRANS, angle=58, major=24, minor=(28, 50), warp=(8, 120), seed=13)
t.later(t.build_along, "trastevere", lanes, region=TRANS, styles=ROMAN, alley=(4, 9))
t.later(t.courts, TRANS, density=0.011, seed=14)
t.street([(vw[0] - 10, VZ), (-300, -150), (-340, -170), (-400, -176)], 5.0, "stone")


ROADS = {
    "appia": [(80, 226), (100, 262), (170, 300), (260, 330), (400, 360)],
    "latina": [(100, 262), (150, 250), (240, 220), (330, 236), (400, 262)],
    "praenestina": [(206, -40), (260, -50), (330, -80), (400, -90)],
    "tiburtina": [(206, 20), (270, 40), (340, 80), (400, 100)],
    "salaria": [(24, -236), (40, -290), (58, -340), (66, -400)],
    "nomentana": [(78, -236), (150, -250)],
    "ostiensis": [(-112, 150), (-134, 176), (-142, 230), (-150, 270)],
}
road_lines = {}
for name, pts in ROADS.items():
    road_lines[name] = t.street(pts, 5.2, "stone")
t.street([(150, -250), (240, -300), (320, -360), (400, -370)], 5.0, "stone")
for name in ("latina", "praenestina", "tiburtina", "salaria"):
    line = road_lines[name]
    if line is None:
        continue
    ribbon = (
        line.buffer(20)
        .difference(city.buffer(8))
        .intersection(Point(line.coords[0]).buffer(110))
    )
    t.later(
        t.frontage,
        f"suburb_{name}",
        line,
        5.2,
        region=ribbon,
        styles=(("", 3), ("tenement", 2)),
        alley=(3, 6),
    )
    t.later(t.avenue, line, spacing=13, offset=8.5, kind="cypress_tree")
ap = road_lines["appia"]
L = ap.length
for k in range(1, int(L // 13)):
    q = ap.interpolate(k * 13)
    a, b = ap.interpolate(k * 13 - 1), ap.interpolate(k * 13 + 1)
    tx, tz = b.x - a.x, b.y - a.y
    n = math.hypot(tx, tz) or 1
    for side in (1, -1):
        x, z = q.x - tz / n * 7 * side, q.y + tx / n * 7 * side
        kind = ("ruins", "statue", "pine_tree", "cypress_tree")[(k + (side > 0)) % 4]
        t.tree(kind, x, z, 1.0 if kind != "pine_tree" else 1.15)
BC = (150, 270)
bcw = t.plaza(
    "thermae_antoninianae",
    BC[0],
    BC[1],
    46,
    26,
    rotation=18,
    sides="ns",
    grand="ns",
    style="flagstone",
)
p.lake(BC[0], BC[1], 16, 8, rotation=18, height=-0.35, shape="basin", calm=True)
t.park(
    "horti_thermarum",
    blob(BC[0], BC[1], 52, 38, seed=40, rotation=18).difference(
        blob(BC[0], BC[1], 38, 26, rotation=18, wobble=0.0)
    ),
    seed=40,
    statues=4,
    cross=0,
    loop=False,
    density=0.010,
)


COAST = p.shore([-80, 260], [0, 1])[1]
HCX = -90
HB_W, HB_D = 88, 46
HCZ = COAST - 12
p.harbour("portus", HCX, HCZ, HB_W, HB_D, sides="")
HN = HCZ - HB_D / 2
x = HCX - HB_W / 2 + 4
bay = 0
while x + 3 * SHOP_W < HCX + HB_W / 2 - 3:
    t.run(
        f"ostia_bay{bay}",
        (x, HN - DOCK_SETBACK),
        (x + 3 * SHOP_W, HN - DOCK_SETBACK),
        0,
        style="dock",
    )
    t.run(
        f"ostia_horrea{bay}",
        (x, HN - 14),
        (x + 3 * SHOP_W, HN - 14),
        0,
        style="portico",
    )
    for k in range(3):
        p.prop(
            "supply_cart",
            round(x + 3 * SHOP_W + 2.6, 1),
            round(HN - 4.5 - k * 3.2, 1),
            1.0,
            90,
        )
    x += 3 * SHOP_W + 6.0
    bay += 1
for side, sgn in (("w", -1), ("e", 1)):
    qx = HCX + sgn * (HB_W / 2 + DOCK_SETBACK)
    t.run(
        f"ostia_quay_{side}",
        (qx, HN + 4),
        (qx, HCZ + HB_D / 2 - 4),
        270 if sgn > 0 else 90,
        style="dock",
    )
t.street(
    [(HCX - HB_W / 2 - 4, HN - 8), (HCX + HB_W / 2 + 4, HN - 8)],
    5.0,
    "flagstone",
    curve=False,
)
t.building("pharos", "defense_tower", HCX + HB_W / 2 + 8, COAST - 2, 0)
t.building("pharos_west", "defense_tower", HCX - HB_W / 2 - 8, COAST - 2, 0)
p.crowd("ostia_crowd", HCX - 36, HN - 11, HCX + 36, HN - 6, 18, seed=9, radius=12)
OST = Polygon(
    [
        (-200, HN - 18),
        (-150, 236),
        (-40, 236),
        (10, 250),
        (20, COAST - 8),
        (-30, COAST - 6),
        (-36, HN - 18),
    ]
)
t.plaza("ostia_forum", -60, HN - 40, 30, 16, rotation=6, sides="ns", grand="n")
t.building("ostia_capitolium", "temple", -60, HN - 60, 6)
ostia_main = t.street(
    [(-150, 270), (-110, HN - 22), (-30, HN - 26), (20, COAST - 14)], 5.0, "stone"
)
t.later(t.frontage, "ostia_dec", ostia_main, 5.0, region=OST, styles=ROMAN)
lanes = t.lanes(OST, angle=6, major=24, minor=(26, 44), warp=(4, 100), seed=15)
t.later(t.build_along, "ostia", lanes, region=OST, styles=ROMAN, alley=(4, 8))
t.later(t.courts, OST, density=0.009, seed=16)


VILLAS = [
    (300, -250, 10),
    (330, -150, -20),
    (290, 130, 15),
    (340, 190, -10),
    (250, -150, 30),
    (360, 10, 0),
    (120, -380, 0),
    (-120, -372, 20),
    (-360, -300, -15),
    (-370, 250, 10),
    (300, 300, -5),
]
for k, (vx, vz, rot) in enumerate(VILLAS):
    t.villa(f"villa_{k}", vx, vz, rot, seed=100 + k)
for k, (fx, fz) in enumerate([(230, -230), (330, 40), (370, -320), (230, 180)]):
    t.building(f"farm_{k}", "farm", fx, fz, 0, pad=3)
for fx, fz, r in [(350, -360, 26), (-370, -385, 20), (380, 370, 18), (-385, 345, 18)]:
    p.forest(f"wood_{fx}_{fz}".replace("-", "m"), fx, fz, r)
t.scatter(
    Polygon([(220, -230), (390, -230), (390, 200), (220, 200)]),
    140,
    (("olive_tree", 5), ("cypress_tree", 1)),
    spacing=7,
    seed=50,
)

t.troops("legio_hastati", "swordsman", 262, -16, count=8, facing=270, cols=4)
t.troops("legio_principes", "spearman", 278, -16, count=8, facing=270, cols=4)
t.troops("legio_sagittarii", "archer", 292, -16, count=4, facing=270, cols=4)
t.troops(
    "legio_equites", "horse_swordsman", 270, 22, count=4, facing=270, cols=4, spacing=9
)
t.troops("praetorians", "swordsman", 150, 98, count=3, facing=120, cols=3)

t.troops(
    "quay_guard", "spearman", bank + 9.5, 112, count=2, facing=0, cols=2, spacing=6
)
t.troops("market_guard", "swordsman", -100, 78, count=2, facing=40, cols=2, spacing=6)
FC = fw(4, -7)
t.troops(
    "cohors_in_foro",
    "swordsman",
    FC[0],
    FC[1],
    count=6,
    facing=(180 - FR) % 360 if False else 150,
    cols=3,
)
EQ = fw(28, -6)
t.troops(
    "equites_in_foro",
    "horse_swordsman",
    EQ[0],
    EQ[1],
    count=2,
    facing=150,
    cols=2,
    spacing=9,
)
t.troops("praetoria_palatina", "spearman", -26, 66, count=3, facing=180, cols=3)
t.troops(
    "custodes_amphitheatri",
    "spearman",
    CX - CRX - 18,
    CZ - 14,
    count=2,
    facing=90,
    cols=2,
    spacing=6,
)
t.troops(
    "gladiatores", "swordsman", CX + 14, CZ - 3, count=2, facing=270, cols=1, spacing=8
)

for k in range(4):
    x, z = circ(-36 + k * 18, -3)
    t.troops(
        f"legio_circi_{k}",
        "swordsman",
        x,
        z,
        count=2,
        facing=(118) % 360,
        cols=1,
        spacing=6,
    )
for k in range(2):
    x, z = circ(-20 + k * 24, 7)
    t.troops(
        f"equites_circi_{k}",
        "horse_swordsman",
        x,
        z,
        count=2,
        facing=118,
        cols=1,
        spacing=8,
    )
t.troops("peregrini", "spearman", 104, 158, count=3, facing=150, cols=3)
t.troops("vigiles", "spearman", -30, 40, count=2, facing=300, cols=2)


for x, z, r in (
    (bank + 10, 132, 3.0),
    (-88, 76, 3.0),
    (-60, 30, 3.0),
    (-18, 36, 3.0),
    (CX - CRX - 20, CZ + 8, 3.0),
    (240, -200, 12),
    (320, 110, 12),
    (-350, -280, 12),
):
    p.add("wildlife", species="sheep", x=x, z=z, radius=r)
for x, z in ((-50, 2), (46, 30), (-140, 120), (120, -296)):
    p.add("wildlife", species="birds", x=x, z=z, radius=30)


p.root(
    "starting_resources",
    {"food": 500, "gold": 600, "iron": 150, "stone": 300, "wood": 400},
)
p.root(
    "theme", "A day in Rome: the river port, the market, the Forum and the Colosseum."
)
t.building(
    "punic_siege_camp",
    "barracks",
    284,
    -370,
    180,
    max_population=120,
    player_id=2,
    nation="carthage",
)
p.add(
    "spawn",
    type="roman_veteran_consul",
    x=round(bank + 9.5, 1),
    z=124.0,
    id="p1_commander",
    player_id=1,
    team_id=1,
    nation="roman_republic",
)
p.add(
    "spawn",
    type="carthage_sword_commander",
    x=300,
    z=-390,
    id="siege_commander",
    player_id=2,
    team_id=2,
    nation="carthage",
)
for name, (gx, gz) in GATES.items():
    inx = 5 if gx == -112 else (-5 if gx == 206 else 0)
    inz = 5 if gz == -236 else (-5 if gz == 226 else 0)
    p.folk(
        f"{name}_guard",
        "spearman",
        [
            (gx + inx - 2 * (inx == 0), gz + inz - 2 * (inz == 0)),
            (gx + inx + 2 * (inx == 0), gz + inz + 2 * (inz == 0)),
        ],
        radius=10,
    )

t.connect()
t.tidy_dead_ends(keep_near=[g for g in GATES.values()])
t.connect()
t.connect(reach=70)
t.tidy_dead_ends(keep_near=[g for g in GATES.values()])
left = t.connect(reach=70)
print("road networks:", left)
made = t.run_jobs()
print(
    "openings cut where streets meet porticoes:", len(t.clear_streets(keep_styles=()))
)
print("streets into buildings:", t.check_streets())


castra_fields = ((108, 150), (124, 192), (150, 162))


def occupies_castra_field(op):
    lot = op.get("json", {})
    if op.get("kind") == "structure" and lot.get("type") == "home":
        half_extent = 10.5
    elif op.get("kind") == "prop":
        half_extent = 8.5
    else:
        return False
    return any(
        abs(lot["x"] - x) < half_extent and abs(lot["z"] - z) < half_extent
        for x, z in castra_fields
    )


p.ops = [op for op in p.ops if not occupies_castra_field(op)]


walk = []
for a, b in (
    ((bank + 9.5, 124), (-112, 96)),
    ((-112, 96), fw(-20, 4)),
    (fw(-20, 4), (CX - CRX - 14, CZ)),
):
    pts, length = t.route(a, b)
    walk += pts if not walk else pts[1:]


for pt in ([CX - CRX - 14, CZ], [CX - CRX - 9, CZ], [CX - CRX + 2, CZ], [CX - 8, CZ]):
    if math.dist(walk[-1], pt) > 1.0:
        walk.append(pt)
json.dump(
    {"map": "assets/maps/map_rome.json", "run": False, "points": walk},
    open("tools/arena/promos/city_film/rome_walk.json", "w"),
    indent=1,
)
print("walk:", len(walk), "points", round(LineString(walk).length), "m")

n = p.write("tools/map_editor/plans/rome.ops.json")
json.dump(
    {"size": SIZE},
    open(os.path.expanduser("~/.cache/soi-tools/city-plans/rome.size.json"), "w"),
)
print(
    "rome:",
    n,
    "operations;",
    made,
    "street houses;",
    t.n_house,
    "structures by Town;",
    t.n_prop,
    "props;",
    p.tally(),
)
