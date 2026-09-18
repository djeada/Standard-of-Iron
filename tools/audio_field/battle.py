"""Which CC0 recordings the battle stingers and beds are built from.

The third recipe table in this directory. `sources.py` describes looping
ambience, `oneshots.py` describes short hits cut out of a performance, and this
one describes the composed battle cues: a volley, a cavalry charge, a horn
order, a crowd. They are neither a single transient nor a loop, so they get
their own builder (`build_battle.py`) rather than being forced into either.

Why these exist at all: the thirty-two cues here were generated with Meta's
a model whose weights forbade commercial use. That made "the game may not be
sold" a licence condition rather than a choice. Everything in this table is CC0
or public domain, so rebuilding them removes the restriction.

A cue is a stack of `Layer`s summed together, enveloped and normalised. The
`Layer` type is `sources.Source` -- the same window/gain/filter/rank description
the ambience beds use, because the operations wanted here are the same ones.

Keep this table and THIRD_PARTY_LICENSES.md in step: the licence column there is
the shipping obligation, this file is how the file gets rebuilt.
"""

from __future__ import annotations

from dataclasses import dataclass

from sources import Source as Layer

ARCHIVE = "https://archive.org/download"
COMMONS = "https://upload.wikimedia.org/wikipedia/commons"

CC0 = "CC0 1.0"

TDC = "The Designer's Choice UCS Collection"
TDC_AUTHOR = "by Nicholas A. Judy"


def tdc(volume: str, path: str) -> tuple[str, str]:
    """A URL and its provenance, for one file inside a CC0 UCS volume."""
    quoted = path.replace(" ", "%20").replace(",", "%2C")
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


HORN_HUNTING = (
    f"{COMMONS}/d/df/Hunting_horn_tone.ogg",
    "Wikimedia Commons, 'File:Hunting horn tone.ogg', by Alon-De-Lon",
)
"""A real brass hunting horn, played as a series of notes.

The first note, at 0:00.70, is the usable one: fundamental 185 Hz with the
harmonic series falling away above it, which is the shape a horn has. Played at
`speed` 0.70 it lands at 130 Hz -- a cornu register -- and lowpassed at 1.6 kHz
its band energy runs 40 dB at the fundamental down to 9 dB at 1.6-4 kHz, which
is the difference between a horn and a horn with a metallic edge. The later
notes in the recording are at 496-500 Hz and are far too bright to use."""

CROWD = tdc(
    "Crowds",
    "CROWDS/APPLAUSE/CRWDApls-CU_Crowd Applause, Cheering, Yelling, "
    "Whooping_Nicholas Judy_TDC.wav",
)
"""Cheering and yelling. Lowpassed hard it becomes a distant army; dry and
ranked it becomes the line in front of you. The applause transients are the
part to filter away -- hands, not voices."""

ELEPHANT = tdc(
    "Animals",
    "ANIMALS/WILD/ANMLWild-CU_Elephant Trumpet_The Designer's Choice_GNRL1.wav",
)


MARCH_ROCKY = tdc(
    "Footsteps",
    "FOOTSTEPS/HUMAN/FEETHmn-Samsung Galaxy Smartphone, "
    "MCU_Running, Rocky Road_Nicholas Judy_TDC.wav",
)
MARCH_GRASS = tdc(
    "Footsteps",
    "FOOTSTEPS/HUMAN/FEETHmn-MCU_Footsteps, On Grass_The Designer's Choice_GNRL2.wav",
)

SNARE = tdc(
    "Musical",
    "MUSICAL/PERCUSSION/MUSCPerc-Blue Snowball Microphone, "
    "CU_Drum, Snare, Military Marching Band_Nicholas Judy_TDC.wav",
)

SWISH_BIG = tdc(
    "Swooshes",
    "SWOOSHES/SWISH/SWSH-Blue Snowball Microphone, "
    "CU_Swishes, Big, Low_Nicholas Judy_TDC.wav",
)
SWISH_MED = tdc(
    "Swooshes",
    "SWOOSHES/SWISH/SWSH-Blue Snowball Microphone, "
    "MCU_Swishes, Medium Low_Nicholas Judy_TDC.wav",
)
SWISH_STICK = tdc(
    "Swooshes",
    "SWOOSHES/SWISH/SWSH-Blue Snowball Microphone, "
    "CU_Stick, Small, Swishes, X4_Nicholas Judy_TDC.wav",
)
CLOTH_SWOOSH = tdc(
    "Swooshes",
    "SWOOSHES/SWISH/SWSH-Samsung Galaxy Smartphone, "
    "MCU_Cloth, Swoosh_Nicholas Judy_TDC.wav",
)
FLYBY = tdc(
    "Swooshes",
    "SWOOSHES/WHOOSH/WHSH-CU_Fly By, Short_The Designer's Choice_GNRL3.wav",
)

BOXING = tdc(
    "Fight",
    "FIGHT/IMPACT/FGHTImpt-Samsung Galaxy Smartphone, "
    "CU_Boxing Glove Hits_Nicholas Judy_TDC.wav",
)
SMACKS_RAPID = tdc(
    "Fight",
    "FIGHT/IMPACT/FGHTImpt-Blue Snowball Microphone, "
    "CU_Smacks, Rapid_The Designer's Choice_GNRL2.wav",
)
BODYFALL_GRASS = tdc(
    "Fight",
    "FIGHT/BODYFALL/FGHTBf-Samsung Galaxy Smartphone, "
    "CU_Bodyfall, On Grass_Nicholas Judy_TDC.wav",
)

CLANG_THIN_01 = tdc(
    "Metal",
    "METAL/IMPACT/METLImpt-Blue Snowball Microphone_"
    "Metal, Clang, Thin 01_Nicholas Judy_TDC.wav",
)
CLANG_DULL = tdc(
    "Metal",
    "METAL/IMPACT/METLImpt-Blue Snowball Microphone_"
    "Metal, Clang, Dull, Quiet_Nicholas Judy_TDC.wav",
)

COMMONS = "https://upload.wikimedia.org/wikipedia/commons"


def commons(path: str, title: str, author: str) -> tuple[str, str]:
    """A URL and its provenance, for one CC0 file on Wikimedia Commons."""
    return f"{COMMONS}/{path}", f"Wikimedia Commons, '{title}', {author}"


HORSES_PASS = commons(
    "9/96/Six_Horses_Galloping_By.ogg",
    "Six Horses Galloping By",
    "Freesound Community via Pixabay",
)
"""Six horses approaching, passing and receding, recorded outdoors. The UCS
collection has no real horse in it -- its FOOTSTEPS/HORSE folder is coconut
shells and a simulated wood floor, which is what the three cavalry cues used to
be built from and what made them sound like a pantomime rather than cavalry."""

DOG_BARK_CLOSE = tdc(
    "Animals",
    "ANIMALS/DOG/ANMLDog-Samsung Galaxy Smartphone, CU_Aggresive Dog Barks and "
    "Snarls, Distant Wind Chimes_The Designer's Choice_GNRL1.wav",
)
"""The other dog recording in the collection, and the one with actual
transients: its sharpest bark onset rises by a factor of 446 against a factor of
4.4 for anything in DOG_SNARL, which was recorded through a window."""


WIND_TREES = tdc(
    "Wind",
    "WIND/VEGETATION/WINDVege-Samsung Galaxy Smartphone, "
    "CU_Thru Trees, Rustling, Faint Crickets_Nicholas Judy_TDC.wav",
)


TARGET_RMS_DB: dict[str, float] = {
    "ability_refused": -22.0,
    "bow_draw_creak": -18.0,
    "bow_full_draw_seat": -21.0,
    "bow_hold_strain": -18.0,
    "bow_release_single": -16.0,
    "charge_roar": -11.0,
    "dodge_roll": -16.0,
    "guard_break": -15.0,
    "guard_raise": -18.0,
    "heal_bind_wound": -20.0,
    "jump_effort": -17.0,
    "land_thud": -15.0,
    "land_thud_v2": -15.0,
    "lock_on_tick": -20.0,
    "perfect_guard": -12.0,
    "second_wind": -17.0,
    "shield_bash": -13.0,
    "shield_bash_v2": -13.0,
    "shield_block": -14.0,
    "siege_launch": -11.5,
    "vanguard_rush": -11.0,
    "enemy_spotted_horn": -12.7,
    "reinforcements_arrived": -11.1,
    "battlefield_crowd_chaos": -14.2,
    "battlefield_distant_mass_01": -15.7,
    "aftermath_battlefield": -14.1,
    "army_march_dirt_mass": -14.8,
    "army_retreat_panic": -15.3,
    "soldiers_victory_cheer": -14.9,
    "carthage_prepare_battle": -13.7,
    "arrows_many_overhead": -20.0,
    "arrows_overhead_dark": -14.6,
    "horse_gallop_close_pass": -12.6,
    "spearmen_formation_advance": -19.2,
    "javelin_throw_whoosh": -15.5,
    "roman_war_horns_orders": -18.8,
    "elephant_charge_carthage": -15.1,
    "elephant_panic": -10.3,
    "wolf_bite_snap": -14.0,
    "wolf_snarl_bark": -16.9,
}
"""Measured RMS of the generated file each cue replaces, in dBFS.

Effect cues are deliberately *not* loudness-normalised at runtime -- see
docs/AUDIO_SYSTEM.md, "Loudness is matched within a category, not across
them". The level baked into the file is the design decision, so a drop-in
replacement has to land where the old file sat or it changes the mix. RMS rather
than LUFS because several of these are shorter than the 400 ms window R128
needs, which is why `wolf_bite_snap` reads as -70 LUFS and is in fact perfectly
audible.

Peak is a different matter. 28 of the 32 originals decode *over* 0 dBFS, by up
to +2.33 dB, because they were mastered to full scale and Vorbis is lossy. Every
cue built here is ceilinged at -1.9 dBFS instead, so the replacements stop
clipping as well as changing licence.

Two numbers are not taken from the original.

`low_resources_click` measured -8.7 dB RMS across ten seconds of dense texture,
which is not what a click is; reproducing it at half a second would be painful.
It is set to sit with the other alerts instead.

`arrows_overhead_ambience` measured -12.9, louder than almost everything else in
the set, because the generator produced a dense five-second wall rather than a bed.
This cue exists to sit *under* a fight without drawing attention, and reaching
-12.9 from fifteen ranked swishes needed more waveshaping than the material
survives. It is set to the -24.4 the honest mix arrives at. This is the one cue
that is deliberately quieter than the file it replaces."""

CEILING = 0.8
"""Peak ceiling, linear: -1.9 dBFS. Not 1.0, and not even 0.9 -- a sample that
peaked at -1 dBFS going into Vorbis can come out over full scale, which is
exactly how the files being replaced ended up clipping."""


@dataclass(frozen=True)
class Cue:
    """One shipped `.ogg`, built by summing its layers.

    Unlike a bed this is not loop-sealed: these fire once and stop, so the
    envelope matters and the seam does not.
    """

    path: str
    """Output path under `assets/audio`, without the extension."""

    seconds: float
    layers: list[Layer]

    attack: float = 0.01
    """Seconds of fade-in. Long enough to kill a click, short enough to keep
    the transient on an impact cue."""

    release: float = 0.25
    """Seconds of fade-out at the tail."""

    peak: float = 0.8
    """Normalisation target, linear. Vorbis is lossy, so a sample that peaked
    at -1 dBFS going in can come out over full scale; -1.9 dBFS leaves the
    codec room to be wrong without the decoder clipping."""

    notes: str = ""


def horn(
    path: str,
    seconds: float,
    start: float,
    *,
    gain: float = 1.0,
    lowpass: float = 0.0,
    ranks: tuple[float, ...] = (),
    release: float = 0.35,
    notes: str = "",
) -> Cue:
    """A single blown horn, optionally answered by more horns behind it.

    One recorded brass note, transposed down into the cornu register. Two
    earlier versions of this are worth not repeating: HORN_VIKING alone is a
    genuine war horn but a phone recording of one, with 25-48 dB more energy
    above 1.2 kHz than below 300 Hz, so it reads as rattle rather than as a
    horn; a transposed didgeridoo has the low end but its buzz lands a bright
    band at 1.6-4 kHz that reads as metallic. A real horn played *down* has
    neither problem, and needs no second layer to prop it up.

    `start` is kept in the signature because the alert cues pass it, but the
    usable note in this recording is fixed, so it is not read.
    """
    del start
    return Cue(
        path=path,
        seconds=seconds,
        layers=[
            Layer(
                url=HORN_HUNTING[0],
                origin=HORN_HUNTING[1],
                licence=CC0,
                start=0.70,
                gain=gain,
                speed=0.70,
                highpass=55.0,
                lowpass=lowpass or 1600.0,
                ranks=ranks,
                rank_falloff=0.62,
                rank_lowpass=0.8,
            ),
        ],
        attack=0.02,
        release=release,
        notes=notes,
    )


CREAK_FLOOR = tdc(
    "Wood",
    "WOOD/FRICTION/WOODFric-Samsung Galaxy Smartphone, "
    "CU_Floorboard, Creak_Nicholas Judy_TDC.wav",
)
"""A board taking weight. Timber under load is what a bow limb is."""

CREAK_SHIP = tdc(
    "Wood",
    "WOOD/FRICTION/WOODFric-Blue Snowball Microphone, "
    "CU_Ship, Creaking, Sound Design_Nicholas Judy_TDC.wav",
)
"""A longer, deeper groan of loaded timber: rope and frame rather than a board."""

WOOD_BREAK = tdc(
    "Wood",
    "WOOD/BREAK/WOODBrk-Blue Snowball Microphone, "
    "CU_Stick, Small, Breaks, X3_Nicholas Judy_TDC.wav",
)
"""Three dry snaps. A shield giving way is wood failing, not metal."""


CLANG_BRIGHT = tdc(
    "Metal",
    "METAL/IMPACT/METLImpt-Blue Snowball Microphone_"
    "Metal, Clang 01_Nicholas Judy_TDC.wav",
)
"""A full-throated struck-metal clang, for a boss taking a blow head on."""

CLANG_THIN = tdc(
    "Metal",
    "METAL/IMPACT/METLImpt-Blue Snowball Microphone_"
    "Metal, Clang, Thin 01_Nicholas Judy_TDC.wav",
)
"""Thinner and shorter than CLANG_BRIGHT: a glancing catch, not a stop."""

TIN_DROP = tdc(
    "Metal",
    "METAL/IMPACT/METLImpt-Blue Snowball Microphone, "
    "CU_Small, Tin, Drop_Nicholas Judy_TDC.wav",
)
"""A small tick of metal. Short enough to fire while the player is aiming."""

BOLT_DROP = tdc(
    "Metal",
    "METAL/IMPACT/METLImpt-Blue Snowball Microphone, "
    "CU_Bolt, Drop_Nicholas Judy_TDC.wav",
)
"""One small hard metal event, for the cues that must not sound like a hit."""


GLOVE_SLAP = tdc(
    "Cloth",
    "CLOTH/IMPACT/CLOTHImpt-Blue Snowball Microphone, "
    "CU_Glove Slap_Nicholas Judy_TDC.wav",
)
"""Leather meeting leather. The hand behind a shield rather than the shield."""

CLOTH_FIGHT = tdc(
    "Cloth",
    "CLOTH/IMPACT/CLOTHImpt-Samsung Galaxy Smartphone, "
    "CU_Swish, Impact, Fight_Nicholas Judy_TDC.wav",
)
"""Garment moving fast and stopping: a body committing to a movement."""

VELCRO_SLOW = tdc(
    "Cloth",
    "CLOTH/RIP/CLOTHRip-Blue Snowball Microphone, "
    "CU_Baseball Mitt, Velcro, Slow_Nicholas Judy_TDC.wav",
)
"""A slow fibrous tear. Linen being drawn off a roll and bound."""

ROPE_SWISH = tdc(
    "Swooshes",
    "SWOOSHES/SWISH/SWSH-Samsung Galaxy Smartphone, "
    "CU_Rope, Twirling Swishes_Nicholas Judy_TDC.wav",
)
"""Rope under speed, for the sling arm of an engine coming round."""


CUES: dict[str, Cue] = {
    "enemy_spotted_horn": horn(
        "sfx/alerts/enemy_spotted_horn",
        1.6,
        0.30,
        notes="One flat blast. The shortest of the horn family, because it "
        "fires the instant a scout sees something.",
    ),
    "reinforcements_arrived": horn(
        "sfx/alerts/reinforcements_arrived",
        2.4,
        0.30,
        ranks=(0.29,),
        release=0.45,
        notes="Two horns close together and undimmed -- the same figure as the "
        "warning above, but near and bright instead of far and dark.",
    ),
    "battlefield_crowd_chaos": Cue(
        path="sfx/combat/battlefield_crowd_chaos",
        seconds=6.0,
        layers=[
            Layer(
                url=CROWD[0],
                origin=CROWD[1],
                licence=CC0,
                start=4.0,
                gain=1.0,
                highpass=170.0,
                lowpass=3400.0,
                loop_source=True,
                ranks=(1.7, 3.1),
                rank_falloff=0.7,
                rank_lowpass=0.8,
            ),
            Layer(
                url=MARCH_ROCKY[0],
                origin=MARCH_ROCKY[1],
                licence=CC0,
                start=1.0,
                gain=0.45,
                lowpass=1500.0,
                loop_source=True,
                ranks=(0.7, 1.9),
                rank_falloff=0.7,
            ),
        ],
        attack=0.4,
        release=0.9,
        notes="Voices over feet. The highpass at 170 Hz is what removes the "
        "applause -- hands are broadband and transient, shouting is not.",
    ),
    "battlefield_distant_mass_01": Cue(
        path="sfx/combat/battlefield_distant_mass_01",
        seconds=7.0,
        layers=[
            Layer(
                url=CROWD[0],
                origin=CROWD[1],
                licence=CC0,
                start=2.0,
                gain=0.75,
                highpass=150.0,
                lowpass=1100.0,
                shelf_db=-6.0,
                loop_source=True,
                ranks=(2.3,),
                rank_falloff=0.7,
            ),
            Layer(
                url=WIND_TREES[0],
                origin=WIND_TREES[1],
                licence=CC0,
                start=6.0,
                gain=0.30,
                lowpass=900.0,
                loop_source=True,
            ),
        ],
        attack=0.9,
        release=1.2,
        notes="The same crowd taken a long way off: lowpassed to 1.1 kHz and "
        "laid under wind. Distance is mostly the absence of treble.",
    ),
    "aftermath_battlefield": Cue(
        path="sfx/combat/aftermath_battlefield",
        seconds=8.0,
        layers=[
            Layer(
                url=CROWD[0],
                origin=CROWD[1],
                licence=CC0,
                start=14.0,
                gain=0.34,
                highpass=140.0,
                lowpass=780.0,
                shelf_db=-9.0,
                loop_source=True,
                ranks=(3.1,),
                rank_falloff=0.6,
            ),
            Layer(
                url=WIND_TREES[0],
                origin=WIND_TREES[1],
                licence=CC0,
                start=12.0,
                gain=0.52,
                lowpass=1600.0,
                loop_source=True,
            ),
        ],
        attack=1.4,
        release=2.0,
        notes="What is left when it stops: wind on top, voices underneath and "
        "far away. The quietest cue in the set by design.",
    ),
    "army_march_dirt_mass": Cue(
        path="sfx/combat/army_march_dirt_mass",
        seconds=6.5,
        layers=[
            Layer(
                url=MARCH_ROCKY[0],
                origin=MARCH_ROCKY[1],
                licence=CC0,
                start=0.5,
                gain=0.95,
                lowpass=2600.0,
                loop_source=True,
                ranks=(0.37, 0.81, 1.29, 1.93),
                rank_falloff=0.76,
                rank_lowpass=0.82,
            ),
            Layer(
                url=MARCH_GRASS[0],
                origin=MARCH_GRASS[1],
                licence=CC0,
                start=1.5,
                gain=0.5,
                lowpass=2000.0,
                loop_source=True,
                ranks=(0.53, 1.11),
                rank_falloff=0.75,
            ),
        ],
        attack=0.5,
        release=0.9,
        notes="One walk summed against itself at offsets that share no common "
        "factor. Overlapping unsynchronised footfalls are what the ear reads "
        "as a column; multiples of one offset would read as an echo.",
    ),
    "army_retreat_panic": Cue(
        path="sfx/combat/army_retreat_panic",
        seconds=5.0,
        layers=[
            Layer(
                url=CROWD[0],
                origin=CROWD[1],
                licence=CC0,
                start=6.5,
                gain=1.05,
                highpass=200.0,
                lowpass=4200.0,
                loop_source=True,
                ranks=(0.9,),
                rank_falloff=0.8,
            ),
            Layer(
                url=MARCH_ROCKY[0],
                origin=MARCH_ROCKY[1],
                licence=CC0,
                start=3.0,
                gain=0.62,
                highpass=90.0,
                loop_source=True,
                ranks=(0.29, 0.67, 1.13),
                rank_falloff=0.8,
            ),
        ],
        attack=0.15,
        release=1.1,
        notes="Brighter and faster than the chaos bed: running feet, and the "
        "crowd left with its top end on so it reads as alarm rather than mass.",
    ),
    "soldiers_victory_cheer": Cue(
        path="sfx/combat/soldiers_victory_cheer",
        seconds=4.5,
        layers=[
            Layer(
                url=CROWD[0],
                origin=CROWD[1],
                licence=CC0,
                start=1.2,
                gain=1.1,
                highpass=180.0,
                lowpass=6000.0,
                ranks=(0.7,),
                rank_falloff=0.75,
            ),
        ],
        attack=0.08,
        release=1.3,
        notes="The one cue the source recording already is. Barely touched: "
        "highpassed off the applause and given a long tail.",
    ),
    "carthage_prepare_battle": Cue(
        path="sfx/combat/carthage_prepare_battle",
        seconds=5.5,
        layers=[
            Layer(
                url=CROWD[0],
                origin=CROWD[1],
                licence=CC0,
                start=11.0,
                gain=0.8,
                highpass=160.0,
                lowpass=2400.0,
                loop_source=True,
                ranks=(1.3,),
                rank_falloff=0.7,
            ),
            Layer(
                url=HORN_HUNTING[0],
                origin=HORN_HUNTING[1],
                licence=CC0,
                start=0.70,
                gain=0.9,
                speed=0.62,
                highpass=55.0,
                lowpass=1600.0,
                ranks=(1.1,),
                rank_falloff=0.65,
                rank_lowpass=0.8,
            ),
        ],
        attack=0.3,
        release=1.0,
        notes="Horns over a held crowd. The army answering its own signal is "
        "the sound of a line forming rather than one fighting. Pitched to 115 "
        "Hz rather than the 130 Hz the Roman horns sit at, so the two armies "
        "do not answer in the same voice.",
    ),
    "arrows_many_overhead": Cue(
        path="sfx/combat/arrows_many_overhead",
        seconds=3.0,
        layers=[
            Layer(
                url=SWISH_MED[0],
                origin=SWISH_MED[1],
                licence=CC0,
                start=0.1,
                gain=0.95,
                highpass=300.0,
                lowpass=7000.0,
                ranks=(0.17, 0.39, 0.62, 0.88),
                rank_falloff=0.86,
            ),
            Layer(
                url=FLYBY[0],
                origin=FLYBY[1],
                licence=CC0,
                start=0.05,
                gain=0.5,
                highpass=420.0,
                ranks=(0.44,),
                rank_falloff=0.8,
            ),
        ],
        attack=0.02,
        release=0.7,
        notes="Passing over rather than at you, so nothing here has an impact "
        "on the end of it.",
    ),
    "arrows_overhead_dark": Cue(
        path="sfx/combat/arrows_overhead_dark",
        seconds=4.0,
        layers=[
            Layer(
                url=SWISH_BIG[0],
                origin=SWISH_BIG[1],
                licence=CC0,
                start=0.15,
                gain=0.85,
                highpass=170.0,
                lowpass=2200.0,
                loop_source=True,
                ranks=(0.31, 0.67, 1.03, 1.41, 1.79, 2.19, 2.61, 3.07),
                rank_falloff=0.93,
            ),
        ],
        attack=0.3,
        release=1.0,
        notes="The heavy sibling of the bed above -- lowpassed to 2.2 kHz so "
        "the arrows read as massed and close rather than thin and high.",
    ),
    "horse_gallop_close_pass": Cue(
        path="sfx/combat/horse_gallop_close_pass",
        seconds=3.0,
        layers=[
            Layer(
                url=HORSES_PASS[0],
                origin=HORSES_PASS[1],
                licence=CC0,
                start=7.3,
                gain=1.0,
                highpass=70.0,
            ),
        ],
        attack=0.2,
        release=0.6,
        notes="The pass itself, unlayered: the recording peaks at 8.25 s as "
        "the horses draw level, so this window is the arrival and the leaving "
        "with the real Doppler on it. Nothing here is ranked -- there are six "
        "horses in the recording already.",
    ),
    "spearmen_formation_advance": Cue(
        path="sfx/combat/spearmen_formation_advance",
        seconds=4.5,
        layers=[
            Layer(
                url=MARCH_GRASS[0],
                origin=MARCH_GRASS[1],
                licence=CC0,
                start=1.0,
                gain=1.0,
                lowpass=3000.0,
                loop_source=True,
                ranks=(0.43, 0.91, 1.47),
                rank_falloff=0.78,
            ),
            Layer(
                url=SNARE[0],
                origin=SNARE[1],
                licence=CC0,
                start=0.4,
                gain=0.42,
                highpass=200.0,
                lowpass=4000.0,
                loop_source=True,
            ),
        ],
        attack=0.3,
        release=0.8,
        notes="Feet with a drum under them. The drum is what makes a crowd "
        "walking read as a formation advancing.",
    ),
    "javelin_throw_whoosh": Cue(
        path="sfx/combat/javelin_throw_whoosh",
        seconds=1.2,
        layers=[
            Layer(
                url=SWISH_BIG[0],
                origin=SWISH_BIG[1],
                licence=CC0,
                start=0.15,
                gain=1.0,
                highpass=180.0,
                lowpass=6000.0,
                ranks=(0.04, 0.09, 0.15),
                rank_falloff=0.88,
            ),
            Layer(
                url=CLOTH_SWOOSH[0],
                origin=CLOTH_SWOOSH[1],
                licence=CC0,
                start=0.15,
                gain=0.7,
                highpass=250.0,
                ranks=(0.06,),
                rank_falloff=0.85,
            ),
        ],
        attack=0.004,
        release=0.3,
        notes="Heavier and lower than an arrow: a pilum has mass, and the "
        "cloth layer is the throwing arm rather than the shaft.",
    ),
    "roman_war_horns_orders": Cue(
        path="sfx/combat/roman_war_horns_orders",
        seconds=3.6,
        layers=[
            Layer(
                url=HORN_HUNTING[0],
                origin=HORN_HUNTING[1],
                licence=CC0,
                start=0.70,
                gain=1.0,
                speed=0.70,
                highpass=55.0,
                lowpass=1600.0,
                ranks=(0.74, 1.51),
                rank_falloff=0.66,
                rank_lowpass=0.8,
            ),
        ],
        attack=0.02,
        release=0.7,
        notes="Three blasts, each further off and darker than the last: an "
        "order being relayed down a line rather than one horn sounding. One "
        "real brass horn pitched down into the cornu register, and nothing "
        "layered over it -- see the `horn` helper for the two brighter "
        "constructions this replaced.",
    ),
    "elephant_charge_carthage": Cue(
        path="sfx/combat/elephant_charge_carthage",
        seconds=4.0,
        layers=[
            Layer(
                url=ELEPHANT[0],
                origin=ELEPHANT[1],
                licence=CC0,
                start=0.1,
                gain=1.05,
                highpass=60.0,
            ),
            Layer(
                url=MARCH_ROCKY[0],
                origin=MARCH_ROCKY[1],
                licence=CC0,
                start=1.0,
                gain=0.55,
                lowpass=900.0,
                loop_source=True,
                ranks=(0.29, 0.63),
                rank_falloff=0.8,
            ),
        ],
        attack=0.02,
        release=0.8,
        notes="A real elephant trumpet over heavily lowpassed feet standing in "
        "for the weight of the animal.",
    ),
    "elephant_panic": Cue(
        path="sfx/combat/elephant_panic",
        seconds=3.2,
        layers=[
            Layer(
                url=ELEPHANT[0],
                origin=ELEPHANT[1],
                licence=CC0,
                start=0.1,
                gain=1.0,
                highpass=90.0,
                lowpass=7000.0,
                ranks=(0.61,),
                rank_falloff=0.85,
            ),
        ],
        attack=0.01,
        release=0.6,
        notes="The same trumpet doubled and brightened: two animals answering "
        "each other, which is what a rout of them sounds like.",
    ),
    "wolf_bite_snap": Cue(
        path="sfx/wildlife/wolf_bite_snap",
        seconds=0.34,
        layers=[
            Layer(
                url=SMACKS_RAPID[0],
                origin=SMACKS_RAPID[1],
                licence=CC0,
                start=1.320,
                gain=1.0,
                highpass=140.0,
                lowpass=7000.0,
            ),
            Layer(
                url=DOG_BARK_CLOSE[0],
                origin=DOG_BARK_CLOSE[1],
                licence=CC0,
                start=22.720,
                gain=0.55,
                highpass=150.0,
                lowpass=6000.0,
            ),
        ],
        attack=0.002,
        release=0.16,
        notes="Jaws closing on flesh, which is a wet impact and an animal on "
        "top of it, not a bark. The old cue took a flat one-second window out "
        "of a dog recorded through a window and landed on the breath between "
        "two barks: it stayed above a quarter of its peak for 0.68 s with no "
        "attack at all, and read as breathing.",
    ),
    "wolf_snarl_bark": Cue(
        path="sfx/wildlife/wolf_snarl_bark",
        seconds=0.56,
        layers=[
            Layer(
                url=DOG_BARK_CLOSE[0],
                origin=DOG_BARK_CLOSE[1],
                licence=CC0,
                start=22.735,
                gain=1.0,
                highpass=120.0,
                lowpass=7500.0,
            ),
        ],
        attack=0.002,
        release=0.30,
        notes="One isolated bark: full level on the first frame and back to "
        "silence within 0.14 s, with nothing either side of it to drag in. "
        "Replaces a hand cut from a CC BY 2.5 study recording, so the wildlife "
        "set carries one less attribution.",
    ),
    "charge_roar": Cue(
        path="sfx/combat/charge_roar",
        seconds=2.4,
        layers=[
            Layer(
                url=CROWD[0],
                origin=CROWD[1],
                licence=CC0,
                start=8.8,
                gain=1.0,
                highpass=220.0,
                lowpass=5200.0,
                ranks=(0.31, 0.73, 1.19),
            ),
            Layer(
                url=MARCH_ROCKY[0],
                origin=MARCH_ROCKY[1],
                licence=CC0,
                start=0.6,
                gain=2.2,
                highpass=90.0,
                lowpass=2600.0,
                ranks=(0.17, 0.41, 0.67),
            ),
        ],
        attack=0.02,
        release=0.45,
        notes="Men shouting as they start forward, over the column already running. "
        "The crowd is high-passed off its applause so it reads as voices, and "
        "the feet are ranked so there is a body behind the shout.",
    ),
    "vanguard_rush": Cue(
        path="sfx/combat/vanguard_rush",
        seconds=1.5,
        layers=[
            Layer(
                url=CROWD[0],
                origin=CROWD[1],
                licence=CC0,
                start=35.2,
                gain=0.85,
                highpass=260.0,
                lowpass=4800.0,
                ranks=(0.19, 0.44),
            ),
            Layer(
                url=MARCH_ROCKY[0],
                origin=MARCH_ROCKY[1],
                licence=CC0,
                start=15.2,
                gain=1.8,
                highpass=110.0,
                lowpass=3000.0,
                ranks=(0.13, 0.29),
            ),
        ],
        attack=0.015,
        release=0.32,
        notes="A shorter, smaller charge_roar: one unit committing, not an army.",
    ),
    "siege_launch": Cue(
        path="sfx/combat/siege_launch",
        seconds=1.4,
        layers=[
            Layer(
                url=CREAK_SHIP[0],
                origin=CREAK_SHIP[1],
                licence=CC0,
                start=64.0,
                gain=1.0,
                highpass=70.0,
                lowpass=2400.0,
            ),
            Layer(
                url=ROPE_SWISH[0],
                origin=ROPE_SWISH[1],
                licence=CC0,
                start=16.0,
                gain=1.1,
                highpass=260.0,
                lowpass=6500.0,
            ),
            Layer(
                url=WOOD_BREAK[0],
                origin=WOOD_BREAK[1],
                licence=CC0,
                start=1.0,
                gain=2.6,
                highpass=200.0,
                lowpass=5000.0,
            ),
        ],
        attack=0.01,
        release=0.4,
        notes="Timber and rope taking the load, then the arm coming round and the "
        "frame slamming to its stop.",
    ),
    "shield_bash": Cue(
        path="sfx/combat/shield_bash",
        seconds=0.9,
        layers=[
            Layer(
                url=CLANG_BRIGHT[0],
                origin=CLANG_BRIGHT[1],
                licence=CC0,
                start=0.0,
                gain=1.0,
                highpass=120.0,
                lowpass=7000.0,
            ),
            Layer(
                url=GLOVE_SLAP[0],
                origin=GLOVE_SLAP[1],
                licence=CC0,
                start=0.0,
                gain=2.8,
                highpass=200.0,
                lowpass=5200.0,
            ),
        ],
        attack=0.002,
        release=0.34,
        notes="A boss driven into a man: full metal clang with the leather of the grip "
        "under it.",
    ),
    "shield_bash_v2": Cue(
        path="sfx/combat/shield_bash_v2",
        seconds=0.9,
        layers=[
            Layer(
                url=CLANG_BRIGHT[0],
                origin=CLANG_BRIGHT[1],
                licence=CC0,
                start=0.2,
                gain=0.9,
                highpass=120.0,
                lowpass=7000.0,
            ),
            Layer(
                url=GLOVE_SLAP[0],
                origin=GLOVE_SLAP[1],
                licence=CC0,
                start=0.0,
                gain=2.8,
                highpass=200.0,
                lowpass=5200.0,
            ),
        ],
        attack=0.002,
        release=0.34,
        notes="The second strike in the same recording, caught a shade duller.",
    ),
    "shield_block": Cue(
        path="sfx/combat/shield_block",
        seconds=0.7,
        layers=[
            Layer(
                url=CLANG_THIN[0],
                origin=CLANG_THIN[1],
                licence=CC0,
                start=0.0,
                gain=1.0,
                highpass=180.0,
                lowpass=8000.0,
            ),
            Layer(
                url=BOXING[0],
                origin=BOXING[1],
                licence=CC0,
                start=7.8,
                gain=1.6,
                highpass=150.0,
                lowpass=4200.0,
            ),
        ],
        attack=0.002,
        release=0.28,
        notes="A blow caught square on the boards. Thinner than shield_bash: the "
        "shield holding, not the shield being driven.",
    ),
    "guard_raise": Cue(
        path="sfx/combat/guard_raise",
        seconds=0.6,
        layers=[
            Layer(
                url=CLOTH_FIGHT[0],
                origin=CLOTH_FIGHT[1],
                licence=CC0,
                start=0.2,
                gain=1.0,
                highpass=220.0,
                lowpass=6000.0,
            ),
            Layer(
                url=BOLT_DROP[0],
                origin=BOLT_DROP[1],
                licence=CC0,
                start=6.6,
                gain=0.5,
                highpass=300.0,
                lowpass=6500.0,
            ),
        ],
        attack=0.006,
        release=0.26,
        notes="Shield coming up: cloth and arm, with just enough metal to say there is a "
        "rim on it. Deliberately not an impact -- nothing has been hit yet.",
    ),
    "guard_break": Cue(
        path="sfx/combat/guard_break",
        seconds=1.1,
        layers=[
            Layer(
                url=WOOD_BREAK[0],
                origin=WOOD_BREAK[1],
                licence=CC0,
                start=1.0,
                gain=3.2,
                highpass=150.0,
                lowpass=6500.0,
            ),
            Layer(
                url=CLANG_THIN[0],
                origin=CLANG_THIN[1],
                licence=CC0,
                start=0.0,
                gain=0.7,
                highpass=200.0,
                lowpass=7500.0,
            ),
        ],
        attack=0.002,
        release=0.4,
        notes="The guard failing: boards splitting first, the rim ringing after.",
    ),
    "perfect_guard": Cue(
        path="sfx/combat/perfect_guard",
        seconds=0.7,
        layers=[
            Layer(
                url=CLANG_THIN[0],
                origin=CLANG_THIN[1],
                licence=CC0,
                start=0.0,
                gain=1.0,
                highpass=400.0,
                lowpass=9000.0,
            ),
        ],
        attack=0.001,
        release=0.32,
        notes="A clean bright ring, alone and unmuddied. The reward cue has to be "
        "legible over a melee, which is why it carries no body layer.",
    ),
    "bow_draw_creak": Cue(
        path="sfx/combat/bow_draw_creak",
        seconds=1.0,
        layers=[
            Layer(
                url=CREAK_FLOOR[0],
                origin=CREAK_FLOOR[1],
                licence=CC0,
                start=0.0,
                gain=1.0,
                highpass=130.0,
                lowpass=3600.0,
            ),
        ],
        attack=0.02,
        release=0.3,
        notes="A limb bending. Loaded timber, taken straight.",
    ),
    "bow_hold_strain": Cue(
        path="sfx/combat/bow_hold_strain",
        seconds=1.6,
        layers=[
            Layer(
                url=CREAK_SHIP[0],
                origin=CREAK_SHIP[1],
                licence=CC0,
                start=63.8,
                gain=1.0,
                highpass=90.0,
                lowpass=2800.0,
            ),
        ],
        attack=0.05,
        release=0.45,
        notes="Held at full draw: the same timber under sustained load, longer and "
        "lower, so it sits under the mix as a warning rather than an event.",
    ),
    "bow_full_draw_seat": Cue(
        path="sfx/combat/bow_full_draw_seat",
        seconds=0.5,
        layers=[
            Layer(
                url=BOLT_DROP[0],
                origin=BOLT_DROP[1],
                licence=CC0,
                start=6.6,
                gain=0.8,
                highpass=350.0,
                lowpass=7000.0,
            ),
            Layer(
                url=CREAK_FLOOR[0],
                origin=CREAK_FLOOR[1],
                licence=CC0,
                start=0.0,
                gain=0.35,
                highpass=200.0,
                lowpass=3200.0,
            ),
        ],
        attack=0.003,
        release=0.22,
        notes="The string seating in the nock: one small hard click with the limb "
        "settling behind it.",
    ),
    "bow_release_single": Cue(
        path="sfx/combat/bow_release_single",
        seconds=0.8,
        layers=[
            Layer(
                url=SWISH_STICK[0],
                origin=SWISH_STICK[1],
                licence=CC0,
                start=1.0,
                gain=1.0,
                highpass=300.0,
                lowpass=7500.0,
            ),
            Layer(
                url=BOLT_DROP[0],
                origin=BOLT_DROP[1],
                licence=CC0,
                start=4.4,
                gain=0.6,
                highpass=400.0,
                lowpass=8000.0,
            ),
        ],
        attack=0.002,
        release=0.3,
        notes="Lighter than bow_loose_heavy: a hunting bow rather than a war bow.",
    ),
    "dodge_roll": Cue(
        path="sfx/combat/dodge_roll",
        seconds=1.0,
        layers=[
            Layer(
                url=CLOTH_FIGHT[0],
                origin=CLOTH_FIGHT[1],
                licence=CC0,
                start=0.2,
                gain=1.0,
                highpass=200.0,
                lowpass=6000.0,
            ),
            Layer(
                url=BODYFALL_GRASS[0],
                origin=BODYFALL_GRASS[1],
                licence=CC0,
                start=0.0,
                gain=2.0,
                highpass=80.0,
                lowpass=3200.0,
            ),
        ],
        attack=0.004,
        release=0.35,
        notes="Cloth committing, then a body meeting the ground and carrying through.",
    ),
    "jump_effort": Cue(
        path="sfx/combat/jump_effort",
        seconds=0.6,
        layers=[
            Layer(
                url=CLOTH_FIGHT[0],
                origin=CLOTH_FIGHT[1],
                licence=CC0,
                start=0.4,
                gain=1.0,
                highpass=250.0,
                lowpass=6500.0,
            ),
            Layer(
                url=MARCH_ROCKY[0],
                origin=MARCH_ROCKY[1],
                licence=CC0,
                start=0.6,
                gain=1.2,
                highpass=120.0,
                lowpass=3400.0,
            ),
        ],
        attack=0.004,
        release=0.24,
        notes="The push-off: garment snapping taut over one hard footfall.",
    ),
    "land_thud": Cue(
        path="sfx/combat/land_thud",
        seconds=0.9,
        layers=[
            Layer(
                url=BODYFALL_GRASS[0],
                origin=BODYFALL_GRASS[1],
                licence=CC0,
                start=0.0,
                gain=6.0,
                highpass=60.0,
                lowpass=2600.0,
            ),
        ],
        attack=0.002,
        release=0.35,
        notes="Weight arriving. Taken flat from the recording -- a landing is one event, "
        "and layering it only makes it sound like two.",
    ),
    "land_thud_v2": Cue(
        path="sfx/combat/land_thud_v2",
        seconds=0.9,
        layers=[
            Layer(
                url=BODYFALL_GRASS[0],
                origin=BODYFALL_GRASS[1],
                licence=CC0,
                start=0.2,
                gain=2.0,
                highpass=60.0,
                lowpass=2600.0,
            ),
        ],
        attack=0.002,
        release=0.35,
        notes="The softer second fall in the same take.",
    ),
    "lock_on_tick": Cue(
        path="sfx/combat/lock_on_tick",
        seconds=0.3,
        layers=[
            Layer(
                url=TIN_DROP[0],
                origin=TIN_DROP[1],
                licence=CC0,
                start=0.0,
                gain=0.8,
                highpass=600.0,
                lowpass=9000.0,
            ),
        ],
        attack=0.001,
        release=0.14,
        notes="A tick, not a hit. It fires while the player is aiming, so it is short, "
        "bright and quiet enough to ignore.",
    ),
    "ability_refused": Cue(
        path="sfx/combat/ability_refused",
        seconds=0.35,
        layers=[
            Layer(
                url=CLANG_DULL[0],
                origin=CLANG_DULL[1],
                licence=CC0,
                start=0.0,
                gain=0.8,
                highpass=250.0,
                lowpass=3400.0,
            ),
        ],
        attack=0.002,
        release=0.18,
        notes="Clipped and dull. A refusal, not an announcement.",
    ),
    "second_wind": Cue(
        path="sfx/combat/second_wind",
        seconds=1.6,
        layers=[
            Layer(
                url=CLOTH_FIGHT[0],
                origin=CLOTH_FIGHT[1],
                licence=CC0,
                start=25.4,
                gain=0.8,
                highpass=200.0,
                lowpass=4200.0,
            ),
            Layer(
                url=CLANG_THIN[0],
                origin=CLANG_THIN[1],
                licence=CC0,
                start=0.0,
                gain=0.4,
                highpass=500.0,
                lowpass=9000.0,
            ),
        ],
        attack=0.06,
        release=0.6,
        notes="A breath drawn and the harness settling, with one distant ring over it. "
        "Recovery reads as tension released, so it swells rather than hits.",
    ),
    "heal_bind_wound": Cue(
        path="sfx/combat/heal_bind_wound",
        seconds=1.8,
        layers=[
            Layer(
                url=VELCRO_SLOW[0],
                origin=VELCRO_SLOW[1],
                licence=CC0,
                start=0.0,
                gain=1.0,
                highpass=300.0,
                lowpass=6000.0,
            ),
        ],
        attack=0.02,
        release=0.5,
        notes="Linen drawn off a roll and pulled tight. A slow fibrous tear is what "
        "binding a wound actually sounds like.",
    ),
}
