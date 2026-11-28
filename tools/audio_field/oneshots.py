"""Which recordings the short combat and movement cues are cut from.

The companion to `sources.py`: that file describes looping beds, this one
describes one-shots. Both are cut from the same freely licensed material and
both are rebuilt by a script rather than hand-edited, so a cue can be retuned
by changing a number here instead of by opening an editor.

A `Take` names the source recordings one cue's variants come out of. Most
sources are a performance -- forty sword hits in a row, twenty footsteps -- so
the build finds the transients, ranks them by level, and keeps `count` of them
as numbered variants. `rank_from` skips the loudest when the top of the
performance is harder than the cue wants.
"""

from __future__ import annotations

from dataclasses import dataclass, field

ARCHIVE = "https://archive.org/download"
COMMONS = "https://upload.wikimedia.org/wikipedia/commons"

CC0 = "CC0 1.0"
PD_MARK = "CC Public Domain Mark 1.0"
PD = "Public domain"

TDC = "The Designer's Choice UCS Collection"
TDC_AUTHOR = "by Nicholas A. Judy"


def tdc(volume: str, path: str) -> tuple[str, str]:
    """A URL and its provenance, for one file inside a CC0 UCS volume."""
    quoted = path.replace("/", "%2F").replace(" ", "%20").replace(",", "%2C")
    quoted = quoted.replace("'", "%27")
    url = f"{ARCHIVE}/Designers-Choice-Collection-{volume}/{quoted}"
    leaf = path.rsplit("/", 1)[-1]
    for suffix in (
        "_Nicholas Judy_TDC.wav",
        "_The Designer's Choice_GNRL1.wav",
        "_The Designer's Choice_GNRL2.wav",
        "_The Designer's Choice_GNRL3.wav",
    ):
        leaf = leaf.replace(suffix, "")
    return url, f"{TDC} - {volume.upper()}, '{leaf}', {TDC_AUTHOR}"


def aporee(item: str, filename: str, place: str, title: str) -> tuple[str, str]:
    return (
        f"{ARCHIVE}/{item}/{filename}",
        f"radio aporee ::: maps, {place}, '{title}'",
    )


def commons(path: str, title: str) -> tuple[str, str]:
    return f"{COMMONS}/{path}", f"Wikimedia Commons, 'File:{title}'"


@dataclass(frozen=True)
class Detect:
    """Gate settings for finding hits in one particular performance.

    The defaults suit a hard impact against a quiet floor. Outdoor recordings
    need a tighter gate: their floor is an ambience, not silence, so a long
    `tail_ms` never sees enough quiet to close and the whole take reads as one
    enormous hit.
    """

    open_db: float = 18.0
    close_db: float = 6.0
    tail_ms: float = 400.0
    min_gap_ms: float = 60.0
    max_length_ms: float = 2500.0
    """Reject anything longer: the gate failed to close and this is not a hit."""


@dataclass(frozen=True)
class Take:
    """The source recordings one cue's variants are cut out of.

    Usually one recording holding a performance. Some library files are already
    a single isolated impact, though, so a take can list several and draw its
    variants across all of them.
    """

    sources: list[tuple[str, str]]
    """(url, origin) pairs, in the order variants are numbered."""

    licence: str

    prefix: str
    """Output path stem; variants get `_01`, `_02` … appended."""

    count: int
    length_ms: float

    detect: Detect = field(default_factory=Detect)
    rank_from: int = 0
    lead_ms: float = 8.0
    fade_in_ms: float = 4.0
    fade_out_ms: float = 40.0
    highpass: float = 0.0
    lowpass: float = 0.0
    speed: float = 1.0
    windows: tuple[float, ...] = ()
    """Optional explicit start offsets (seconds) for continuous field takes."""
    room_echo: bool = False
    """Convolve dry takes with a vaulted stone interior impulse response."""

    peak: float = 0.8
    """Normalisation target, linear.

    Not 1.0, and not even 0.9: Vorbis is lossy, so a sample that peaked at
    -1 dBFS going in can come out over full scale. -1.9 dBFS leaves the codec
    room to be wrong without the decoder clipping.
    """

    notes: str = ""


OUTDOOR = Detect(open_db=14.0, close_db=10.0, tail_ms=80.0, min_gap_ms=140.0)
"""For performances recorded outdoors, where the floor is an ambience."""


def board_drop(number: str) -> tuple[str, str]:
    return tdc(
        "Wood",
        "WOOD/IMPACT/WOODImpt-Samsung Galaxy Smartphone, "
        f"CU_Board Drop {number}_Nicholas Judy_TDC.wav",
    )


def metal_clang(name: str) -> tuple[str, str]:
    return tdc(
        "Metal",
        "METAL/IMPACT/METLImpt-Blue Snowball Microphone_"
        f"Metal, {name}_Nicholas Judy_TDC.wav",
    )


TAKES: dict[str, Take] = {
    "sword_hit": Take(
        sources=[
            tdc(
                "Weapons",
                "WEAPONS/SWORD/WEAPSwrd-Samsung Galaxy Smartphone, "
                "CU_Sword, Hits_Nicholas Judy_TDC.wav",
            )
        ],
        licence=CC0,
        prefix="sfx/combat/sword_hit",
        count=4,
        length_ms=340.0,
        highpass=110.0,
        notes="Blade on blade. Backs combat.hit.sword, which fires every 90 ms, "
        "so the tail is cut long before the ring dies away.",
    ),
    "blade_clash": Take(
        sources=[
            tdc(
                "Weapons",
                "WEAPONS/SWORD/WEAPSwrd-Samsung Galaxy Smartphone_"
                "Sword, Hits, Scrapes, Shings_Nicholas Judy_TDC.wav",
            )
        ],
        licence=CC0,
        prefix="sfx/combat/blade_clash",
        count=4,
        length_ms=380.0,
        rank_from=2,
        highpass=110.0,
        notes="Parries and scrapes rather than clean strikes. The two loudest "
        "are skipped: those are full-force hits, which the sword take covers.",
    ),
    "spear_impact": Take(
        sources=[board_drop("01"), board_drop("02")],
        licence=CC0,
        prefix="sfx/combat/spear_impact",
        count=2,
        length_ms=300.0,
        highpass=90.0,
        lowpass=9000.0,
        notes="A spear meets a shield, and a shield is a board -- wood, not "
        "metal. Each source file is already a single impact.",
    ),
    "arrow_impact": Take(
        sources=[board_drop("03"), board_drop("04"), board_drop("05")],
        licence=CC0,
        prefix="sfx/combat/arrow_impact",
        count=3,
        length_ms=220.0,
        highpass=140.0,
        notes="Shorter and brighter than the spear: an arrow bites where a "
        "spear shoves.",
    ),
    "stone_impact": Take(
        sources=[
            tdc(
                "Rocks",
                "ROCKS/CRASH & DEBRIS/ROCKCrsh-Samsung Galaxy Smartphone, "
                "CU_Small Stones, Kicked, X4_Nicholas Judy_TDC.wav",
            )
        ],
        licence=CC0,
        prefix="sfx/combat/stone_impact",
        count=3,
        length_ms=500.0,
        detect=OUTDOOR,
        highpass=80.0,
        notes="Siege shot landing. Longer than the blade cues because its cue "
        "has a 250 ms cooldown and can afford the debris.",
    ),
    "footstep_grass": Take(
        sources=[
            tdc(
                "Footsteps",
                "FOOTSTEPS/HUMAN/FEETHmn-MCU_Footsteps, On Grass_"
                "The Designer's Choice_GNRL2.wav",
            )
        ],
        licence=CC0,
        prefix="sfx/movement/footstep_grass",
        count=4,
        length_ms=260.0,
        detect=OUTDOOR,
        highpass=90.0,
        peak=0.62,
        notes="Four variants because a footstep cue fires twice a second, and "
        "one clip on repeat is a metronome rather than a person.",
    ),
    "footstep_stone": Take(
        sources=[
            tdc(
                "Footsteps",
                "FOOTSTEPS/HUMAN/FEETHmn-Samsung Galaxy Smartphone, "
                "CU_Footsteps, Rocky Surface_Nicholas Judy_TDC.wav",
            )
        ],
        licence=CC0,
        prefix="sfx/movement/footstep_stone",
        count=4,
        length_ms=260.0,
        detect=OUTDOOR,
        highpass=90.0,
        peak=0.62,
    ),
    "footstep_run": Take(
        sources=[
            tdc(
                "Footsteps",
                "FOOTSTEPS/HUMAN/FEETHmn-Samsung Galaxy Smartphone, "
                "MCU_Running, Rocky Road_Nicholas Judy_TDC.wav",
            )
        ],
        licence=CC0,
        prefix="sfx/movement/footstep_run",
        count=4,
        length_ms=220.0,
        detect=OUTDOOR,
        highpass=90.0,
        peak=0.66,
        notes="Harder and shorter than the walk: a run lands heavier and the "
        "next step is already on its way.",
    ),
    "footstep_gravel": Take(
        sources=[
            tdc(
                "Rocks",
                "ROCKS/CRASH & DEBRIS/ROCKCrsh-Samsung Galaxy Smartphone, "
                "CU_Small Stones, Kicked, X4_Nicholas Judy_TDC.wav",
            ),
            tdc(
                "Footsteps",
                "FOOTSTEPS/HUMAN/FEETHmn-Samsung Galaxy Smartphone, "
                "CU_Footsteps, Leaves_Nicholas Judy_TDC.wav",
            ),
        ],
        licence=CC0,
        prefix="sfx/movement/footstep_gravel",
        count=4,
        length_ms=250.0,
        detect=OUTDOOR,
        highpass=170.0,
        lowpass=6500.0,
        peak=0.58,
        notes="Walk on gravel or coarse sand for garden paths and Carthage plazas.",
    ),
    "footstep_stone_interior": Take(
        sources=[
            tdc(
                "Footsteps",
                "FOOTSTEPS/HUMAN/FEETHmn-Samsung Galaxy Smartphone, "
                "CU_Footsteps, Rocky Surface_Nicholas Judy_TDC.wav",
            )
        ],
        licence=CC0,
        prefix="sfx/movement/footstep_stone_interior",
        count=4,
        length_ms=460.0,
        detect=OUTDOOR,
        highpass=90.0,
        lowpass=5800.0,
        fade_out_ms=110.0,
        room_echo=True,
        peak=0.62,
        notes="Stone step convolved with vaulted masonry slap-echo for gatehouses "
        "and Colosseum tunnels.",
    ),
    "melee_swing": Take(
        sources=[
            tdc(
                "Swooshes",
                "SWOOSHES/SWISH/SWSH-Blue Snowball Microphone, "
                "CU_Stick, Small, Swishes, X4_Nicholas Judy_TDC.wav",
            ),
            tdc(
                "Swooshes",
                "SWOOSHES/SWISH/SWSH-Blue Snowball Microphone, "
                "MCU_Swishes, Medium Low_Nicholas Judy_TDC.wav",
            ),
        ],
        licence=CC0,
        prefix="sfx/combat/melee_swing",
        count=3,
        length_ms=260.0,
        highpass=160.0,
        lowpass=6500.0,
        peak=0.58,
        notes="Dry air-cut when the FPV commander swings a blade or spear.",
    ),
    "hawker_call": Take(
        sources=[
            aporee(
                "aporee_66484_76873",
                "2405111219BerlinZehlendorfErdbeeren.flac",
                "Zehlendorf, Berlin, Germany",
                "Strawberries market vendor",
            ),
            aporee(
                "aporee_32850_37750",
                "Marketchatter.mp3",
                "Dalyan, Muğla Province, Turkey",
                "Dalyan market chatter",
            ),
        ],
        licence=PD_MARK,
        prefix="sfx/city/hawker_call",
        count=6,
        length_ms=1100.0,
        fade_in_ms=25.0,
        fade_out_ms=180.0,
        highpass=180.0,
        lowpass=3600.0,
        windows=(6.5, 19.2, 34.0, 52.5, 71.0, 88.5),
        peak=0.46,
        notes="Short distant vendor calls, band-limited so no modern word jumps out.",
    ),
    "dog_bark_distant": Take(
        sources=[
            tdc(
                "Animals",
                "ANIMALS/DOG/ANMLDog-Samsung Galaxy Smartphone, "
                "Distant_Rottweiler Puppies, Howling, Barking, Thru Window_"
                "The Designer's Choice_GNRL1.wav",
            ),
            tdc(
                "Animals",
                "ANIMALS/DOG/ANMLDog-Samsung Galaxy Smartphone, "
                "CU_Large Dog, Barking_The Designer's Choice_GNRL1.wav",
            ),
        ],
        licence=CC0,
        prefix="sfx/city/dog_bark_distant",
        count=4,
        length_ms=850.0,
        fade_in_ms=12.0,
        fade_out_ms=140.0,
        highpass=160.0,
        lowpass=1750.0,
        windows=(1.05, 0.08, 4.35, 1.52),
        peak=0.42,
        notes="One or two barks rolled off above 1.75 kHz so they sit streets away.",
    ),
    "door_close": Take(
        sources=[
            tdc(
                "Doors",
                "DOORS/WOOD/DOORWood-Samsung Galaxy Smartphone, "
                "CU_Shed, Open, Close_Nicholas Judy_TDC.wav",
            ),
            tdc(
                "Doors",
                "DOORS/WOOD/DOORWood-Samsung Galaxy Smartphone, "
                "CU_Close, Reverberant_Nicholas Judy_TDC.wav",
            ),
            tdc(
                "Doors",
                "DOORS/WOOD/DOORWood-Samsung Galaxy Smartphone, "
                "CU_French Double Door, Open, Close_Nicholas Judy_TDC.wav",
            ),
            tdc(
                "Doors",
                "DOORS/ANTIQUE/DOORAntq-Samsung Galaxy Smartphone, "
                "CU_Wooden Treasure Chest Close_Nicholas Judy_TDC.wav",
            ),
        ],
        licence=CC0,
        prefix="sfx/city/door_close",
        count=4,
        length_ms=680.0,
        fade_out_ms=120.0,
        highpass=70.0,
        lowpass=4200.0,
        peak=0.55,
        notes="Wooden house and shop doors shutting.",
    ),
    "cart_wheels": Take(
        sources=[
            tdc(
                "Vehicles",
                "VEHICLES/WAGON/VEHWagn-CU_Stagecoach Pass By_Nicholas Judy_TDC.wav",
            )
        ],
        licence=CC0,
        prefix="sfx/city/cart_wheels",
        count=3,
        length_ms=3400.0,
        fade_in_ms=180.0,
        fade_out_ms=380.0,
        highpass=75.0,
        lowpass=3600.0,
        windows=(1.2, 5.4, 9.8),
        peak=0.48,
        notes="Wooden cart wheels rolling past on stone.",
    ),
    "forge_hammer": Take(
        sources=[
            aporee(
                "aporee_14355_16708",
                "HosingenNaturparkhausFamilienfestSchmiedNageln.mp3",
                "Hosingen, Luxembourg",
                "summer fair, forging stand",
            ),
            aporee(
                "aporee_26643_30744",
                "WeihnachtsmarktSiegburgDerSchmiedheiztseinFeuerundSchmiedet.mp3",
                "Siegburg, Germany",
                "Blacksmith on the medieval Christmas market",
            ),
        ],
        licence=PD_MARK,
        prefix="sfx/city/forge_hammer",
        count=4,
        length_ms=1850.0,
        fade_in_ms=18.0,
        fade_out_ms=220.0,
        highpass=120.0,
        lowpass=5800.0,
        windows=(46.2, 53.4, 62.0, 74.5),
        peak=0.52,
        notes="Three to five hammer strikes on an anvil for active barracks smithies.",
    ),
    "temple_bell": Take(
        sources=[
            tdc(
                "Bells",
                "BELLS/GONG/BELLGong-Blue Snowball Microphone, "
                "CU_Metal, Ringing, Various_The Designer's Choice_GNRL1.wav",
            ),
            tdc(
                "Bells",
                "BELLS/LARGE/BELLLrg-Samsung Galaxy Smartphone, "
                "CU_Rope Hung Bell, Ringing_Nicholas Judy_TDC.wav",
            ),
        ],
        licence=CC0,
        prefix="sfx/city/temple_bell",
        count=2,
        length_ms=2400.0,
        fade_in_ms=6.0,
        fade_out_ms=420.0,
        speed=0.78,
        highpass=95.0,
        lowpass=4200.0,
        windows=(0.15, 3.40),
        peak=0.46,
        notes="Bronze temple bell or gong strike with warm decay.",
    ),
    "water_splash": Take(
        sources=[
            tdc(
                "Water",
                "WATER/SPLASH/WATRSplsh-Samsung Galaxy Smartphone, "
                "CU_Water, Splash, Small_Nicholas Judy_TDC.wav",
            ),
            tdc(
                "Water",
                "WATER/SPLASH/WATRSplsh-Samsung Galaxy Smartphone, "
                "CU_Water, Jump In, Splash_Nicholas Judy_TDC.wav",
            ),
        ],
        licence=CC0,
        prefix="sfx/city/water_splash",
        count=3,
        length_ms=820.0,
        fade_in_ms=8.0,
        fade_out_ms=160.0,
        highpass=90.0,
        lowpass=5200.0,
        windows=(0.0, 2.65, 0.38),
        peak=0.46,
        notes="Bucket, oar or basin splash for quays and pool gardens.",
    ),
    "sheep_bleat_calm": Take(
        sources=[
            commons("1/13/Sheep_bleating.ogg", "Sheep bleating.ogg"),
            commons(
                "5/51/Corner_of_a_sheep_field_in_summer.ogg",
                "Corner of a sheep field in summer.ogg",
            ),
            aporee(
                "aporee_15277_17812",
                "graz.ogg",
                "Samaria Gorge, Crete, Greece",
                "sheep afternoon",
            ),
        ],
        licence=PD,
        prefix="sfx/wildlife/sheep_bleat_calm",
        count=6,
        length_ms=980.0,
        fade_in_ms=18.0,
        fade_out_ms=160.0,
        highpass=130.0,
        lowpass=4200.0,
        windows=(0.05, 1.4, 4.2, 9.6, 15.8, 23.4),
        peak=0.46,
        notes="Relaxed grazing sheep bleats rather than alarm calls.",
    ),
    "lamb_bleat": Take(
        sources=[
            commons("2/28/Sheep_bleat.ogg", "Sheep bleat.ogg"),
            commons("b/bc/Herd_of_goats_bleating.ogg", "Herd of goats bleating.ogg"),
        ],
        licence=CC0,
        prefix="sfx/wildlife/lamb_bleat",
        count=3,
        length_ms=780.0,
        fade_in_ms=14.0,
        fade_out_ms=140.0,
        speed=1.12,
        highpass=210.0,
        lowpass=4600.0,
        windows=(0.05, 1.8, 4.1),
        peak=0.42,
        notes="Higher-pitched lamb bleat.",
    ),
    "cattle_low": Take(
        sources=[
            tdc(
                "Animals",
                "ANIMALS/FARM/ANMLFarm-CU_Bull Growls, Bellows, Twice, Then Snorts_"
                "The Designer's Choice_GNRL1.wav",
            ),
            aporee(
                "aporee_56834_65011",
                "Herdofcowsearlyam.mp3",
                "Piirimäe, Estonia",
                "herd of cows in the early morning",
            ),
        ],
        licence=PD_MARK,
        prefix="sfx/wildlife/cattle_low",
        count=4,
        length_ms=1450.0,
        fade_in_ms=25.0,
        fade_out_ms=220.0,
        highpass=70.0,
        lowpass=2600.0,
        windows=(0.2, 2.5, 12.0, 28.5),
        peak=0.48,
        notes="Single cattle low for livestock pens and rural farms.",
    ),
    "elephant_rumble": Take(
        sources=[
            tdc(
                "Human",
                "HUMAN/BREATH/HMNBrth-Blue Snowball Microphone, "
                "MCU_Thru Didgeridoo, Elephant Like_Nicholas Judy_TDC.wav",
            ),
            tdc(
                "Animals",
                "ANIMALS/FARM/ANMLFarm-CU_Bull Growls, Bellows, Twice, Then Snorts_"
                "The Designer's Choice_GNRL1.wav",
            ),
        ],
        licence=CC0,
        prefix="sfx/wildlife/elephant_rumble",
        count=4,
        length_ms=1550.0,
        fade_in_ms=45.0,
        fade_out_ms=260.0,
        speed=0.56,
        highpass=35.0,
        lowpass=360.0,
        windows=(0.2, 1.8, 3.6, 5.4),
        peak=0.48,
        notes="Low idle rumble in an elephant yard or company, with no trumpet.",
    ),
    "elephant_trunk": Take(
        sources=[
            tdc(
                "Animals",
                "ANIMALS/FARM/ANMLFarm-CU_Bull Growls, Bellows, Twice, Then Snorts_"
                "The Designer's Choice_GNRL1.wav",
            ),
            tdc(
                "Animals",
                "ANIMALS/WILD/ANMLWild-CU_Elephant Trumpet_The Designer's Choice_GNRL1.wav",
            ),
        ],
        licence=CC0,
        prefix="sfx/wildlife/elephant_trunk",
        count=3,
        length_ms=720.0,
        fade_in_ms=16.0,
        fade_out_ms=150.0,
        speed=0.68,
        highpass=65.0,
        lowpass=920.0,
        windows=(4.6, 5.5, 0.12),
        peak=0.42,
        notes="Short, quiet idle trunk snort rather than a battle trumpet.",
    ),
    "gull": Take(
        sources=[
            tdc(
                "Birds",
                "BIRDS/SEA/BIRDSea-CU_Seagulls, Chirping, Crying, Distant Surf_"
                "The Designer's Choice_GNRL1.wav",
            ),
            aporee(
                "aporee_16431_19043",
                "galebiinstarci1.wav",
                "Rovinj, Croatia",
                "seagulls and old people",
            ),
        ],
        licence=CC0,
        prefix="sfx/wildlife/gull",
        count=4,
        length_ms=920.0,
        detect=OUTDOOR,
        fade_in_ms=14.0,
        fade_out_ms=160.0,
        highpass=240.0,
        lowpass=4800.0,
        peak=0.44,
        notes="Single gull calls over harbours and river quays.",
    ),
    "pigeon_flutter": Take(
        sources=[
            tdc(
                "Wings",
                "WINGS/BIRD/WINGBird-Blue Snowball Microphone, "
                "MCU_Pigeon, Flap Away, Simulated_Nicholas Judy_TDC.wav",
            ),
            tdc(
                "Wings",
                "WINGS/BIRD/WINGBird-Blue Snowball Microphone, "
                "MCU_Pigeon, Flap By, Simulated_Nicholas Judy_TDC.wav",
            ),
            aporee(
                "aporee_56822_64997",
                "PIGEONS14522.mp3",
                "radio aporee",
                "Pigeons and Others",
            ),
        ],
        licence=CC0,
        prefix="sfx/wildlife/pigeon_flutter",
        count=3,
        length_ms=820.0,
        fade_in_ms=10.0,
        fade_out_ms=150.0,
        highpass=140.0,
        lowpass=5800.0,
        windows=(0.05, 0.10, 14.0),
        peak=0.46,
        notes="Pigeon wings taking off in squares and amphitheatres.",
    ),
    "cricket": Take(
        sources=[
            tdc(
                "Ambiences",
                "AMBIENCES/INSECT/AMBInsc-Samsung Galaxy Smartphone, "
                "CU_Crickets, Evening_Nicholas Judy_TDC.wav",
            )
        ],
        licence=CC0,
        prefix="sfx/wildlife/cricket",
        count=2,
        length_ms=1750.0,
        fade_in_ms=80.0,
        fade_out_ms=240.0,
        highpass=1800.0,
        lowpass=6200.0,
        windows=(2.0, 9.5),
        peak=0.34,
        notes="Short night cricket phrase for dusk and night garden emitters.",
    ),
}


RATE = 48000
QUALITY = 5
"""One notch above the beds. These are short enough that the size cost is
nothing, and they are the cues the player hears most often."""
