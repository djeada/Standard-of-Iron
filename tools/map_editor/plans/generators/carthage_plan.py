#!/usr/bin/env python3
"""Carthage, as map_cli operations. Run from the repo root with the soi-tools venv;
writes tools/map_editor/plans/carthage.ops.json.

North is -z, east is +x. The city fills a headland: the gulf to the east, the
Lake of Tunis to the south, the salt marsh to the north-west and the isthmus
with its triple wall to the west. The Byrsa rises in steps in the middle; the
lower city fans out from it to the sea in radial streets and ring streets; the
two harbours lie in the south-east; Megara, the garden suburb, fills the north
inside its own long wall.
"""
import json
import math
import os
import random

from organic import SHOP_W, Town, blob, rect
from planlib import DOCK_SETBACK, SEA_LEVEL, Plan, spline
from shapely.geometry import LineString, Point, Polygon, box

SIZE = 801
p = Plan("carthage")
t = Town(p, seed=814)
rng = random.Random(814)
PUNIC = (("tenement", 5), ("", 2))

p.map(
    name="Carthage",
    description="The city on its headland: the Byrsa rising in terraces to the temple of Eshmun, the "
    "lower city fanning out to the sea, the round war harbour with the admiral's island and "
    "the merchant basin, the agora behind the quays, the tophet by the lagoon, the walled "
    "gardens and villas of Megara, the necropolis on the hills, and the triple wall across "
    "the isthmus.",
    biome={
        "ground_type": "grass_dry",
        "seed": 814,
        "procedural_boulders_enabled": False,
        "procedural_iron_ore_enabled": False,
        "procedural_trees_enabled": False,
        "plant_density": 0.3,
        "height_noise": [0.04, 0.05],
        "tree_mix": {"palm": 1.4, "olive": 0.3, "cypress": 0.0, "pine": 0.0},
    },
    environment={
        "lighting_profile": "mediterranean_summer",
        "start_time": 10.5,
        "time_mode": "locked",
        "day_length_seconds": 1800,
    },
    rain={"enabled": False, "intensity": 0, "type": "rain"},
    camera={
        "center": [80, 0, 120],
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


p.lake(700, -20, 1000, 2400, height=SEA_LEVEL)
p.lake(-140, 610, 1300, 660, height=SEA_LEVEL)
p.lake(-440, -410, 560, 400, height=SEA_LEVEL)
HZ = 196
X = p.shore([0, HZ], [1, 0])[0]
LAG = p.shore([60, HZ], [0, 1])[1]
print("coast x", X, "lagoon z", LAG)


BW, BD = 74, 44
BCX = X - 24
p.harbour("merchant", BCX, HZ, BW, BD, sides="")
BWEST = BCX - BW / 2

for side, z, face in (
    ("n", HZ - BD / 2 - DOCK_SETBACK, 0),
    ("s", HZ + BD / 2 + DOCK_SETBACK, 180),
):
    x = BWEST + 4
    k = 0
    while x + 3 * SHOP_W < X - 6:
        t.run(f"merchant_{side}{k}", (x, z), (x + 3 * SHOP_W, z), face, style="dock")
        for c in range(2):
            p.prop(
                "supply_cart",
                round(x + 3 * SHOP_W + 2.4, 1),
                round(z + (-1 if side == "n" else 1) * (4 + c * 3), 1),
                1.0,
                90,
            )
        x += 3 * SHOP_W + 5.0
        k += 1
    sgn = -1 if side == "n" else 1
    t.run(
        f"merchant_stores_{side}",
        (BWEST + 4, z + sgn * 12.5),
        (X - 10, z + sgn * 12.5),
        (face + 180) % 360 if False else face,
        style="portico",
    )
    t.street(
        [(BWEST - 2, z + sgn * 6.5), (X + 2, z + sgn * 6.5)],
        5.0,
        "flagstone",
        curve=False,
    )

RING_R, RING_W = 26, 15
CCX = BWEST - 12 - (RING_R + RING_W / 2)
p.cothon("cothon", CCX, HZ, RING_R, RING_W, docks=26, gap_deg=0, gap_width=40)
QUAY_R = RING_R + RING_W / 2 + 8.5
ring = [
    [
        round(CCX + QUAY_R * math.cos(math.radians(a)), 2),
        round(HZ + QUAY_R * math.sin(math.radians(a)), 2),
    ]
    for a in range(26, 335, 7)
]
t.street(ring, 5.0, "flagstone", curve=False)

t.loop(
    [
        (CCX - 13, HZ - 13),
        (CCX + 13, HZ - 13),
        (CCX + 13, HZ + 13),
        (CCX - 13, HZ + 13),
    ],
    5.0,
)
for k in range(8):
    a = math.radians(22.5 + 45 * k)
    r0, r1 = RING_R - RING_W / 2 - 4.5, RING_R - RING_W / 2 + 0.8
    t.street(
        [
            (CCX + r0 * math.cos(a), HZ + r0 * math.sin(a)),
            (CCX + r1 * math.cos(a), HZ + r1 * math.sin(a)),
        ],
        3.0,
        "flagstone",
        curve=False,
    )
p.river([[CCX + RING_R, HZ], [BWEST + 6, HZ]], 10, SEA_LEVEL + 0.01, calm=True)
t.building("admiralty", "temple", CCX, HZ, 90)
t.building("admiralty_tower", "defense_tower", CCX - 4, HZ - 10.5, 0)
t.river_bridge(
    [[CCX - RING_R, HZ + 10], [CCX - RING_R, HZ], [CCX - RING_R, HZ - 10]],
    RING_W,
    (CCX - RING_R, HZ),
    deck=8.0,
    land=3.0,
    approach=((CCX - QUAY_R - 1, HZ), (CCX - 13, HZ)),
    style="flagstone",
)
t.building("pharos", "defense_tower", X + 6, HZ - BD / 2 - 6, 0)
t.building("pharos_south", "defense_tower", X + 6, HZ + BD / 2 + 6, 0)
t.keep_out(Point(CCX, HZ).buffer(QUAY_R + 4))
t.keep_out(box(BWEST - 4, HZ - BD / 2 - 20, X, HZ + BD / 2 + 20))
p.crowd(
    "harbour_crowd",
    BWEST + 4,
    HZ - BD / 2 - 12,
    X - 8,
    HZ - BD / 2 - 6,
    16,
    seed=3,
    radius=12,
)


BX, BZ = -70, 30
p.rise(BX, BZ, 160, 128, 3.0, 0.32)
p.rise(BX, BZ, 104, 84, 4.6, 0.22)
p.rise(BX, BZ - 2, 62, 50, 6.0, 0.18)
bp = t.plaza(
    "byrsa_precinct", BX, BZ, 40, 22, rotation=0, sides="ns", grand="n", statues=0
)
t.building("temple_of_eshmun", "temple", BX - 27, BZ, 90)
for tx, tz in [
    (BX - 25, BZ - 17),
    (BX - 25, BZ + 17),
    (BX + 25, BZ - 17),
    (BX + 25, BZ + 17),
]:
    t.tree("palm_tree", tx, tz, 1.15)
for k in range(5):
    p.prop("statue", BX + 2 + k * 3.6, BZ + (1 if k % 2 else -1) * 4.5, 0.9)


APPROACH = [(BX + 22, BZ), (BX + 60, BZ + 6), (BX + 100, BZ + 22), (BX + 140, BZ + 46)]
stair = t.street(APPROACH, 7.0, "flagstone", curve=True)
t.later(t.avenue, stair, spacing=8, offset=5.2, kind="cypress_tree")

terraces = []
for rx, rz, seed in ((38, 30, 1), (56, 46, 2), (78, 64, 3)):
    terraces += t.ring(
        Polygon(blob(BX, BZ, 200, 200, wobble=0).exterior),
        BX,
        BZ,
        rx,
        rz,
        width=3.6,
        style="flagstone" if rx < 50 else "stone",
        a0=0,
        a1=360,
        seed=seed,
        wobble=0.03,
    )


AG = (CCX + 4, HZ - 74)
ag = t.plaza(
    "agora", AG[0], AG[1], 70, 30, rotation=14, sides="nswe", grand="n", statues=4
)
p.crowd(
    "agora_crowd", AG[0] - 26, AG[1] - 8, AG[0] + 26, AG[1] + 8, 52, seed=11, radius=18
)
t.building("temple_of_baal_hammon", "temple", *ag(-46, 0), (90 - 14) % 360)
t.building("temple_of_reshef", "temple", *ag(46, 0), (270 - 14) % 360)

EY = t.compound("elephant_yard", 92, 64, 124, 90, gate_side="s")
t.troops("war_elephants", "elephant", 108, 76, count=3, facing=180, cols=3, spacing=9)
t.building("barracks_of_the_wall", "barracks", -234, 20, 90, max_population=120)


TX, TZ = CCX - 60, LAG - 36
t.building("tophet_shrine", "temple", TX, TZ - 10, 180)
TOPH = blob(TX, TZ + 8, 26, 14, seed=5)
t.keep_out(TOPH)
for k in range(28):
    x, z = TX - 20 + (k % 7) * 6.5 + rng.uniform(-1, 1), TZ + 2 + (
        k // 7
    ) * 4.2 + rng.uniform(-0.6, 0.6)
    if TOPH.contains(Point(x, z)):
        p.prop("statue", round(x, 1), round(z, 1), round(rng.uniform(0.55, 0.75), 2))
for k in range(10):
    a = 2 * math.pi * k / 10
    t.tree(
        "cypress_tree" if k % 2 else "palm_tree",
        TX + 30 * math.cos(a),
        TZ + 8 + 17 * math.sin(a),
        1.0,
    )
p.prop("magic_shrine", TX + 24, TZ - 6, 1.0)


W = -250
WALL_S = p.shore([W, 100], [0, 1])[1] + 1
WALL_N = p.shore([W, -150], [0, -1])[1] - 1
land = t.wall("isthmus", [(W, WALL_S), (W, WALL_N)], closed=False, tower_spacing=36)
t.wall(
    "outwork",
    [(W - 20, WALL_S - 14), (W - 20, WALL_N + 16)],
    closed=False,
    tower_spacing=60,
)
t.wall(
    "ditch_wall",
    [(W - 38, WALL_S - 24), (W - 38, WALL_N + 30)],
    closed=False,
    tower_spacing=0,
)
GATES = {
    "gate_of_tunis": (W, 120),
    "gate_of_utica": (W, -100),
    "outwork_south": (W - 20, 120),
    "outwork_north": (W - 20, -100),
    "ditch_south": (W - 38, 120),
    "ditch_north": (W - 38, -100),
}

NW = -112
CITY_N = [(W, NW), (-120, NW), (-60, NW - 22), (40, NW - 22), (100, NW - 6)]
coast_n = p.shore([100, NW - 6], [1, 0])
CITY_N.append((coast_n[0] + 1, NW - 6))
t.wall("city_north", CITY_N, closed=False, tower_spacing=44)
GATES.update({"gate_of_megara": (-160, NW), "gate_of_the_gardens": (0, NW - 22)})

SEA = []
for z in range(int(HZ - BD / 2 - 28), int(NW - 6) - 1, -38):
    q = p.shore([X - 120, z], [1, 0])
    SEA.append((q[0] - 9, z))
SEA.append((coast_n[0] + 1, NW - 6))
t.wall("sea_wall", SEA, closed=False, tower_spacing=40)
for name, (gx, gz) in GATES.items():
    t.gate(name, gx, gz)
CITY = Polygon(
    [(W + 6, NW + 6), (-120, NW + 6), (-60, NW - 16), (40, NW - 16), (100, NW)]
    + [(x - 6, z) for (x, z) in reversed(SEA)]
    + [(X - 12, HZ - BD / 2 - 24), (X - 6, LAG - 8), (W + 6, LAG - 8)]
)
CITY = CITY.buffer(0)


GS = []
GS.append(
    t.street(
        [
            (W - 60, 126),
            (W, 120),
            (-180, 112),
            (-130, 92),
            (-60, 100),
            (0, 130),
            ag(-36, 8),
        ],
        5.6,
        "stone",
    )
)
GS.append(
    t.street(
        [
            (W - 60, -104),
            (W, -100),
            (-180, -80),
            (-110, -60),
            (-30, -50),
            (60, -40),
            (140, -20),
            (p.shore([150, -20], [1, 0])[0] - 16, -20),
        ],
        5.4,
        "stone",
    )
)
GS.append(
    t.street([(-160, NW), (-150, -60), (-120, -20), (BX - 40, BZ - 10)], 5.0, "stone")
)
GS.append(
    t.street([(0, NW - 22), (6, -60), (-20, -10), (BX + 30, BZ - 18)], 5.0, "stone")
)
GS.append(t.street([ag(-20, -15), (84, 80), (60, 40), (40, -40)], 5.0, "stone"))
GS.append(t.street([EY, ag(-4, -15)], 4.4, "flagstone", curve=False))


harbour_street = t.street(
    [(BWEST - 2, HZ - BD / 2 - DOCK_SETBACK - 6.5), (BWEST - 6, HZ - 44), ag(30, 3.3)],
    5.0,
    "flagstone",
)
bazaar = t.street([ag(-30, -10), ag(-30, -22), (74, 90), (70, 76)], 6.0, "flagstone")
p.crowd("bazaar_crowd", 62, 74, 86, 112, 56, seed=19, radius=8)
p.crowd(
    "quay_crowd",
    BWEST - 10,
    HZ - 50,
    BWEST + 20,
    HZ - BD / 2 - 4,
    30,
    seed=20,
    radius=8,
)
GS.append(t.street([(BX - 10, BZ + 40), (-60, 130), (TX - 10, TZ - 22)], 4.8, "stone"))
t.street([(W - 60, 126), (-400, 140)], 5.6, "stone", curve=False)
t.street([(W - 60, -104), (-400, -110)], 5.4, "stone", curve=False)

prom = []
for x in range(int(W + 14), int(BWEST - 30), 12):
    q = p.shore([x, LAG - 60], [0, 1])
    if q is not None:
        prom.append((x, q[1] - 14))
if len(prom) > 2:
    GS.append(t.street(prom, 5.0, "flagstone"))

fan = t.radial(
    CITY,
    BX,
    BZ,
    [-62, -44, -26, -8, 10, 28, 46, 64, 82, 100, 118, 136, 154, 172, 190, 208, 226],
    r0=86,
    r1=380,
    width=4.0,
    style="stone",
    bend=6,
    seed=4,
)
rings = []
for r, seed in ((122, 5), (164, 6), (210, 7), (260, 8)):
    rings += t.ring(
        CITY,
        BX,
        BZ,
        r,
        r * 0.86,
        width=3.2,
        style="stone",
        a0=-80,
        a1=240,
        seed=seed,
        wobble=0.05,
    )


NARROW = Polygon(
    [ag(-40, 20), ag(40, 20), (CCX + QUAY_R + 4, HZ - 20), (CCX - QUAY_R - 4, HZ - 20)]
).buffer(0)
narrow = t.lanes(
    NARROW,
    angle=104,
    major=17,
    minor=(16, 28),
    warp=(3, 60),
    wiggle=1.2,
    major_width=2.6,
    minor_width=2.4,
    seed=21,
)


MEG = Polygon(
    [(W + 6, NW - 8), (W + 6, WALL_N + 10), (-180, -300), (-100, -390), (120, -396)]
    + [(p.shore([0, z], [1, 0])[0] - 10, z) for z in range(-390, int(NW) - 8, 30)]
    + [(100, NW - 14), (40, NW - 30), (-60, NW - 30), (-120, NW - 8)]
).buffer(0)

MEG_WALL = [(W, WALL_N), (-196, -296), (-110, -380), (-92, -398)]
t.wall("megara_wall", MEG_WALL, closed=False, tower_spacing=56)
meg_lanes = []
meg_lanes += [
    t.street(
        [(-160, NW), (-170, -180), (-150, -260), (-110, -330), (-40, -380)],
        4.0,
        "stone",
    )
]
meg_lanes += [
    t.street(
        [(0, NW - 22), (-10, -180), (30, -260), (80, -330), (120, -390)], 4.0, "stone"
    )
]
meg_lanes += [
    t.street(
        [(-170, -180), (-80, -200), (-10, -180), (80, -170), (150, -160)], 4.0, "stone"
    )
]
meg_lanes += [
    t.street([(-150, -260), (-60, -290), (30, -260), (120, -250)], 4.0, "stone")
]


for pts in (
    [(-166, -186), (-120, -228), (-60, -270), (-14, -300)],
    [(-6, -176), (40, -214), (76, -252), (110, -258)],
):
    for px, pz in (pts[0], pts[-1]):
        p.lake(px, pz, 9, 9, height=-0.3, shape="round", calm=True)
        t.loop(
            [
                (px + 7.5 * math.cos(a), pz + 7.5 * math.sin(a))
                for a in [2 * math.pi * k / 16 for k in range(16)]
            ],
            2.2,
        )
    p.river(spline(pts, 8), 3.0, -0.25, calm=True)

VILLAS = [
    (-200, -200, 20),
    (-120, -250, -10),
    (-30, -230, 5),
    (50, -210, 15),
    (100, -290, -20),
    (-60, -330, 30),
    (20, -320, 0),
    (-140, -170, 40),
    (140, -200, 0),
    (60, -370, -15),
]
for k, (vx, vz, rot) in enumerate(VILLAS):
    if MEG.contains(Point(vx, vz)):
        t.villa(
            f"villa_{k}",
            vx,
            vz,
            rot,
            seed=200 + k,
            grove=(("palm_tree", 3), ("olive_tree", 3), ("plant", 1)),
        )

t.pool_garden(
    "oasis",
    70,
    -148,
    r=18,
    pool=8,
    seed=61,
    ring="palm_tree",
    kinds=(("palm_tree", 3), ("plant", 3), ("olive_tree", 1)),
)
t.pool_garden(
    "garden_of_tanit",
    -96,
    -152,
    r=14,
    seed=62,
    ring="cypress_tree",
    kinds=(("plant", 4), ("palm_tree", 1)),
)
for k, (fx, fz, r) in enumerate(
    [
        (-200, -130, 0),
        (-80, -160, 90),
        (100, -240, 0),
        (-20, -280, 90),
        (60, -300, 0),
        (-120, -300, 90),
    ]
):

    for ox, oz in ((0, 0), (18, 0), (-18, 0), (0, 18), (0, -18), (22, 22), (-22, -22)):
        g = rect(fx + ox, fz + oz, 17.6, 17.6, 1, 0)
        if MEG.contains(g) and not t.roads.hits(g) and not t.solid.hits(g):
            t.building(f"megara_farm_{k}", "farm", fx + ox, fz + oz, r, pad=2)
            break


for k, (hx, hz, w, d, h) in enumerate(
    [(-20, -76, 70, 34, 2.4), (90, -70, 60, 30, 2.0)]
):
    p.rise(hx, hz, w, d, h, 0.5)
    NEC = blob(hx, hz, w / 2 - 6, d / 2 - 5, seed=50 + k)
    t.keep_out(NEC)
    for _ in range(26):
        x, z = hx + rng.uniform(-w / 2 + 8, w / 2 - 8), hz + rng.uniform(
            -d / 2 + 6, d / 2 - 6
        )
        if NEC.contains(Point(x, z)):
            p.prop(
                rng.choice(["ruins", "statue", "ruins", "cypress_tree"]),
                round(x, 1),
                round(z, 1),
                round(rng.uniform(0.6, 0.9), 2),
            )


rest_lanes = t.lanes(
    CITY.difference(blob(BX, BZ, 90, 74, wobble=0)),
    angle=40,
    major=23,
    minor=(26, 46),
    warp=(5, 110),
    seed=30,
    skip=0.2,
    clear_of_roads=9.0,
)


t.connect()
t.tidy_dead_ends(keep_near=[g for g in GATES.values()] + [EY])
t.connect()
t.connect(reach=70)
t.tidy_dead_ends(keep_near=[g for g in GATES.values()] + [EY])
left = t.connect(reach=70)
print("road networks:", left)
made = 0
for line in GS:
    if line is not None:
        made += t.frontage("great", line, 5.4, region=CITY, styles=PUNIC, alley=(5, 9))

for name, line, wd in (
    ("bazaar_stalls", bazaar, 6.0),
    ("harbour_stalls", harbour_street, 5.0),
):
    if line is not None:
        made += t.frontage(
            name,
            line,
            wd,
            region=CITY,
            kind="marketplace",
            styles=(("", 3), ("portico", 1)),
            alley=(2, 3),
            start=4.0,
        )

if stair is not None:
    made += t.frontage(
        "stair",
        stair,
        7.0,
        region=CITY,
        kind="marketplace",
        styles=(("portico", 1.0),),
        alley=(2, 4),
        start=6.0,
    )
made += t.build_along(
    "byrsa",
    terraces,
    region=CITY,
    styles=(("tenement", 3), ("", 2)),
    alley=(4, 7),
    stagger=0.8,
)
made += t.build_along(
    "narrow",
    narrow,
    region=NARROW,
    styles=(("tenement", 4), ("", 1)),
    alley=(3, 6),
    stagger=1.2,
)
made += t.build_along("fan", fan, region=CITY, styles=PUNIC, alley=(4, 8), stagger=1.0)
made += t.build_along(
    "ring", rings, region=CITY, styles=PUNIC, alley=(3, 7), stagger=1.0
)
made += t.build_along(
    "rest", rest_lanes, region=CITY, styles=PUNIC, alley=(4, 8), stagger=0.8
)

t.courts(
    CITY,
    density=0.010,
    kinds=(("palm_tree", 3), ("olive_tree", 3), ("plant", 4), ("cypress_tree", 1)),
    seed=2,
)

for line in meg_lanes:
    if line is not None:
        t.avenue(line, spacing=11, offset=4.2, kind="palm_tree", region=MEG)
t.scatter(
    MEG,
    900,
    (("olive_tree", 5), ("palm_tree", 3), ("plant", 3), ("cypress_tree", 1)),
    spacing=5.0,
    seed=60,
)

t.avenue(
    LineString([(W - 60, 126), (-400, 140)]), spacing=12, offset=6, kind="palm_tree"
)
t.avenue(
    LineString([(W - 60, -104), (-400, -110)]), spacing=12, offset=6, kind="palm_tree"
)
for k, (fx, fz, r) in enumerate(
    [(-330, 40, 0), (-360, 200, 90), (-320, -40, 90), (-380, -180, 0)]
):
    t.building(f"isthmus_farm_{k}", "farm", fx, fz, r, pad=2)
p.forest("isthmus_wood", -350, 260, 24)
t.connect(reach=80)
print("openings cut where streets meet porticoes:", len(t.clear_streets()))


p.root(
    "starting_resources",
    {"food": 500, "gold": 600, "iron": 150, "stone": 300, "wood": 400},
)
p.root(
    "theme",
    "Two harbours, the stepped Byrsa, the gardens of Megara and the wall across the isthmus.",
)
QUAY = (BWEST + 10, HZ - BD / 2 - 6.5)
t.building(
    "roman_siege_camp",
    "barracks",
    -370,
    100,
    90,
    max_population=120,
    player_id=2,
    nation="roman_republic",
)
p.add(
    "spawn",
    type="carthage_sword_commander",
    x=round(QUAY[0], 1),
    z=round(QUAY[1], 1),
    id="p1_commander",
    player_id=1,
    team_id=1,
    nation="carthage",
)

t.troops("sacred_band", "swordsman", AG[0] + 6, AG[1] - 9, count=4, facing=194, cols=4)

for k, (u, v) in enumerate(((-18, 9), (-6, 11))):
    x, z = ag(u, v)
    t.troops(f"agora_elephant_{k}", "elephant", x, z, count=1, facing=104)
x, z = ag(20, -8)
t.troops("agora_spears", "spearman", x, z, count=4, facing=194, cols=4, spacing=6)
t.troops(
    "quay_spears",
    "spearman",
    BWEST + 36,
    HZ - BD / 2 - 9,
    count=2,
    facing=180,
    cols=2,
    spacing=6,
)
t.troops("bazaar_guard", "swordsman", 60, 100, count=2, facing=60, cols=2, spacing=6)
t.troops(
    "stair_guard", "spearman", BX + 60, BZ + 19, count=2, facing=0, cols=2, spacing=6
)
t.troops(
    "eshmun_priests", "healer", BX - 12, BZ - 7, count=2, facing=90, cols=2, spacing=6
)
t.troops("isthmus_swords", "swordsman", W + 50, 30, count=8, facing=270, cols=4)
t.troops(
    "megara_horse", "horse_swordsman", -60, -200, count=4, facing=180, cols=2, spacing=9
)
t.troops("isthmus_spears", "spearman", W + 34, 60, count=6, facing=270, cols=3)
t.troops(
    "numidian_horse",
    "horse_swordsman",
    W + 34,
    -40,
    count=4,
    facing=270,
    cols=2,
    spacing=9,
)
t.troops("harbour_archers", "archer", X - 30, HZ - 46, count=2, facing=90, cols=2)
for x, z, r in (
    (AG[0] - 24, AG[1] + 6, 3.0),
    (74, 96, 3.0),
    (148, 150, 3.0),
    (66, 72, 3.0),
    (BX - 2, BZ + 6, 3.0),
    (-120, -220, 12),
    (40, -300, 12),
    (-330, 60, 12),
    (-340, -60, 10),
):
    p.add("wildlife", species="sheep", x=x, z=z, radius=r)
for x, z in ((AG[0], AG[1]), (CCX, HZ), (BX, BZ), (70, -148)):
    p.add("wildlife", species="birds", x=x, z=z, radius=30)
p.add(
    "spawn",
    type="roman_veteran_consul",
    x=-370,
    z=120,
    id="siege_commander",
    player_id=2,
    team_id=2,
    nation="roman_republic",
)
for name, (gx, gz) in GATES.items():
    vertical = gx in (W, W - 20, W - 38)
    pts = (
        [(gx + 5, gz - 2), (gx + 5, gz + 2)]
        if vertical
        else [(gx - 2, gz + 5), (gx + 2, gz + 5)]
    )
    p.folk(f"{name}_guard", "spearman", pts, radius=10)

print("streets into buildings:", t.check_streets())
walk = []
for a, b in ((QUAY, AG), (AG, (70, 76)), ((70, 76), (BX + 22, BZ))):
    pts, length = t.route(a, b)
    print("  leg", a, "->", b, round(length), "m")
    walk += pts if not walk else pts[1:]
walk += [[BX + 16, BZ], [BX + 10, BZ]]
json.dump(
    {"map": "assets/maps/map_carthage.json", "run": False, "points": walk},
    open("tools/arena/promos/city_film/carthage_walk.json", "w"),
    indent=1,
)
print("walk:", len(walk), "points", round(LineString(walk).length), "m")
n = p.write("tools/map_editor/plans/carthage.ops.json")
json.dump(
    {"size": SIZE},
    open(os.path.expanduser("~/.cache/soi-tools/city-plans/carthage.size.json"), "w"),
)
print(
    "carthage:",
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
