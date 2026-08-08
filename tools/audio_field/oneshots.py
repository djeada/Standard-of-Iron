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

CC0 = "CC0 1.0"

TDC = "The Designer's Choice UCS Collection"
TDC_AUTHOR = "by Nicholas A. Judy"


def tdc(volume: str, path: str) -> tuple[str, str]:
    """A URL and its provenance, for one file inside a CC0 UCS volume."""
    quoted = path.replace("/", "%2F").replace(" ", "%20").replace(",", "%2C")
    quoted = quoted.replace("'", "%27")
    url = f"{ARCHIVE}/Designers-Choice-Collection-{volume}/{quoted}"
    leaf = path.rsplit("/", 1)[-1].replace("_Nicholas Judy_TDC.wav", "")
    return url, f"{TDC} - {volume.upper()}, '{leaf}', {TDC_AUTHOR}"


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
    fade_out_ms: float = 40.0
    highpass: float = 0.0
    lowpass: float = 0.0

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
    "armour_hit": Take(
        sources=[
            metal_clang("Clang, Thin 01"),
            metal_clang("Clang, Thin 02"),
            metal_clang("Clank, Thin"),
        ],
        licence=CC0,
        prefix="sfx/combat/armour_hit",
        count=3,
        length_ms=300.0,
        highpass=140.0,
        notes="Backs combat.hit.generic, the fallback for units whose type has "
        "no dedicated family.",
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
}


RATE = 48000
QUALITY = 5
"""One notch above the beds. These are short enough that the size cost is
nothing, and they are the cues the player hears most often."""
