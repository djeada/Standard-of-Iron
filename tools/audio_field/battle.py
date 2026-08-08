"""Which CC0 recordings the battle stingers and beds are built from.

The third recipe table in this directory. `sources.py` describes looping
ambience, `oneshots.py` describes short hits cut out of a performance, and this
one describes the composed battle cues: a volley, a cavalry charge, a horn
order, a crowd. They are neither a single transient nor a loop, so they get
their own builder (`build_battle.py`) rather than being forced into either.

Why these exist at all: the thirty-two cues here were generated with Meta's
AudioCraft, whose model weights are CC BY-NC 4.0. That made "the game may not be
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

CC0 = "CC0 1.0"

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


HORN_VIKING = tdc(
    "Horns",
    "HORNS/TRADITIONAL/HORNTrad-Samsung Galaxy Smartphone, "
    "CU_Viking War_Nicholas Judy_TDC.wav",
)
"""A blown war horn. The only period-plausible horn in the collection."""

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

GALLOP_01 = tdc(
    "Footsteps",
    "FOOTSTEPS/HORSE/FEETHors-Samsung Galaxy Smartphone_Horse Galloping, "
    "Coconut Shells, Looped 01_Nicholas Judy_TDC.wav",
)
GALLOP_02 = tdc(
    "Footsteps",
    "FOOTSTEPS/HORSE/FEETHors-Samsung Galaxy Smartphone_Horse Galloping, "
    "Coconut Shells, Looped 02_Nicholas Judy_TDC.wav",
)
GALLOP_03 = tdc(
    "Footsteps",
    "FOOTSTEPS/HORSE/FEETHors-Samsung Galaxy Smartphone_Horse Galloping, "
    "Coconut Shells, Looped 03_Nicholas Judy_TDC.wav",
)
"""Coconut-shell gallops. This is how horses have been done since radio drama;
the shells are the instrument, the same way the synthesised cues use struck
wood. Three separate performances, so a charge can be built from three horses
rather than one horse three times."""

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
CLANG_02 = tdc(
    "Metal",
    "METAL/IMPACT/METLImpt-Blue Snowball Microphone_Metal, Clang 02_Nicholas Judy_TDC.wav",
)
CLANG_DULL = tdc(
    "Metal",
    "METAL/IMPACT/METLImpt-Blue Snowball Microphone_"
    "Metal, Clang, Dull, Quiet_Nicholas Judy_TDC.wav",
)

DOG_SNARL = tdc(
    "Animals",
    "ANIMALS/DOG/ANMLDog-Samsung Galaxy Smartphone, CU_Aggessive Dog Barks, "
    "Snarls, Inside Window_The Designer's Choice_GNRL1.wav",
)
"""A dog is Canis lupus familiaris -- the same species as the wolf -- which is
the reasoning the existing wildlife cues already use."""

WIND_TREES = tdc(
    "Wind",
    "WIND/VEGETATION/WINDVege-Samsung Galaxy Smartphone, "
    "CU_Thru Trees, Rustling, Faint Crickets_Nicholas Judy_TDC.wav",
)


TARGET_RMS_DB: dict[str, float] = {
    "enemy_spotted_horn": -12.7,
    "enemy_reinforcements_warning": -12.0,
    "reinforcements_arrived": -11.1,
    "population_limit_horn": -14.7,
    "low_resources_click": -14.0,
    "battlefield_crowd_chaos": -14.2,
    "battlefield_distant_mass_01": -15.7,
    "battlefield_distant_mass_02": -13.8,
    "aftermath_battlefield": -14.1,
    "army_march_dirt_mass": -14.8,
    "army_retreat_panic": -15.3,
    "soldiers_victory_cheer": -14.9,
    "carthage_prepare_battle": -13.7,
    "archer_volley_many": -18.2,
    "archers_shooting_close": -20.1,
    "arrows_fast_flybys_lr": -21.2,
    "arrows_many_overhead": -20.0,
    "arrows_overhead_ambience": -24.4,
    "arrows_overhead_dark": -14.6,
    "arrows_whistle_snap_impact": -18.9,
    "arrows_impact_shields_dirt": -19.7,
    "roman_cavalry_charge": -13.0,
    "numidian_cavalry_chase": -15.2,
    "horse_gallop_close_pass": -12.6,
    "roman_shield_wall_impact": -16.2,
    "gladius_shield_impacts_close": -14.2,
    "spearmen_formation_advance": -19.2,
    "javelin_throw_whoosh": -15.5,
    "roman_war_horns_orders": -18.8,
    "elephant_charge_carthage": -15.1,
    "elephant_panic": -10.3,
    "wolf_bite_snap": -14.0,
}
"""Measured RMS of the AudioCraft file each cue replaces, in dBFS.

Effect cues are deliberately *not* loudness-normalised at runtime -- see
docs/AUDIO_MASTERING.md, "Loudness is matched within a category, not across
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
the set, because AudioCraft produced a dense five-second wall rather than a bed.
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
    """A single blown horn, optionally answered by more horns behind it."""
    return Cue(
        path=path,
        seconds=seconds,
        layers=[
            Layer(
                url=HORN_VIKING[0],
                origin=HORN_VIKING[1],
                licence=CC0,
                start=start,
                gain=gain,
                highpass=70.0,
                lowpass=lowpass,
                ranks=ranks,
                rank_falloff=0.6,
                rank_lowpass=0.75,
            ),
        ],
        attack=0.02,
        release=release,
        notes=notes,
    )


CUES: dict[str, Cue] = {
    "enemy_spotted_horn": horn(
        "sfx/alerts/enemy_spotted_horn",
        1.6,
        0.30,
        notes="One flat blast. The shortest of the horn family, because it "
        "fires the instant a scout sees something.",
    ),
    "enemy_reinforcements_warning": horn(
        "sfx/alerts/enemy_reinforcements_warning",
        2.6,
        0.30,
        lowpass=2600.0,
        ranks=(0.62,),
        release=0.5,
        notes="Two horns, the second darker and behind the first: something "
        "is coming that you cannot see yet.",
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
    "population_limit_horn": horn(
        "sfx/alerts/population_limit_horn",
        1.4,
        0.30,
        gain=0.85,
        lowpass=3200.0,
        notes="Clipped short. A refusal, not an announcement.",
    ),
    "low_resources_click": Cue(
        path="sfx/alerts/low_resources_click",
        seconds=0.5,
        layers=[
            Layer(
                url=CLANG_DULL[0],
                origin=CLANG_DULL[1],
                licence=CC0,
                start=0.08,
                gain=0.7,
                highpass=300.0,
                lowpass=5200.0,
            ),
        ],
        attack=0.002,
        release=0.22,
        notes="A dull struck-metal tick. The only alert that is not a horn, "
        "because it fires while the player is reading a number.",
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
    "battlefield_distant_mass_02": Cue(
        path="sfx/combat/battlefield_distant_mass_02",
        seconds=7.0,
        layers=[
            Layer(
                url=CROWD[0],
                origin=CROWD[1],
                licence=CC0,
                start=9.0,
                gain=0.72,
                highpass=150.0,
                lowpass=950.0,
                shelf_db=-6.0,
                loop_source=True,
                ranks=(1.7, 3.7),
                rank_falloff=0.72,
            ),
            Layer(
                url=MARCH_GRASS[0],
                origin=MARCH_GRASS[1],
                licence=CC0,
                start=2.0,
                gain=0.34,
                lowpass=1200.0,
                loop_source=True,
                ranks=(0.9,),
                rank_falloff=0.7,
            ),
        ],
        attack=0.9,
        release=1.2,
        notes="A second window of the same crowd so the two files are not the "
        "same sound twice; feet instead of wind underneath.",
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
                url=HORN_VIKING[0],
                origin=HORN_VIKING[1],
                licence=CC0,
                start=0.30,
                gain=0.9,
                highpass=70.0,
                lowpass=2800.0,
                ranks=(1.1,),
                rank_falloff=0.65,
            ),
        ],
        attack=0.3,
        release=1.0,
        notes="Horns over a held crowd. The army answering its own signal is "
        "the sound of a line forming rather than one fighting.",
    ),
    "archer_volley_many": Cue(
        path="sfx/combat/archer_volley_many",
        seconds=2.6,
        layers=[
            Layer(
                url=SWISH_BIG[0],
                origin=SWISH_BIG[1],
                licence=CC0,
                start=0.15,
                gain=1.0,
                highpass=260.0,
                ranks=(0.06, 0.13, 0.19, 0.27, 0.34),
                rank_falloff=0.85,
                rank_lowpass=0.92,
            ),
            Layer(
                url=SWISH_STICK[0],
                origin=SWISH_STICK[1],
                licence=CC0,
                start=0.1,
                gain=0.6,
                highpass=400.0,
                ranks=(0.09, 0.21),
                rank_falloff=0.85,
            ),
        ],
        attack=0.005,
        release=0.6,
        notes="A volley is one arrow fired six times inside a third of a "
        "second. Tight offsets so it reads as a release, not as six arrows.",
    ),
    "archers_shooting_close": Cue(
        path="sfx/combat/archers_shooting_close",
        seconds=2.0,
        layers=[
            Layer(
                url=SWISH_STICK[0],
                origin=SWISH_STICK[1],
                licence=CC0,
                start=0.1,
                gain=1.0,
                highpass=320.0,
                ranks=(0.11, 0.26),
                rank_falloff=0.8,
            ),
            Layer(
                url=CLOTH_SWOOSH[0],
                origin=CLOTH_SWOOSH[1],
                licence=CC0,
                start=0.2,
                gain=0.55,
                highpass=200.0,
            ),
        ],
        attack=0.004,
        release=0.4,
        notes="Fewer bows and nearer: the cloth layer is the bowstring against "
        "a sleeve, which is the part you only hear up close.",
    ),
    "arrows_fast_flybys_lr": Cue(
        path="sfx/combat/arrows_fast_flybys_lr",
        seconds=2.2,
        layers=[
            Layer(
                url=FLYBY[0],
                origin=FLYBY[1],
                licence=CC0,
                start=0.05,
                gain=1.0,
                highpass=380.0,
                ranks=(0.33, 0.71, 1.04),
                rank_falloff=0.82,
                rank_lowpass=0.9,
            ),
        ],
        attack=0.004,
        release=0.35,
        notes="Four passes at uneven spacing. Even spacing sounds mechanical.",
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
    "arrows_overhead_ambience": Cue(
        path="sfx/combat/arrows_overhead_ambience",
        seconds=5.0,
        layers=[
            Layer(
                url=SWISH_MED[0],
                origin=SWISH_MED[1],
                licence=CC0,
                start=0.1,
                gain=1.0,
                highpass=280.0,
                lowpass=5200.0,
                loop_source=True,
                ranks=(
                    0.19,
                    0.41,
                    0.67,
                    0.93,
                    1.21,
                    1.49,
                    1.79,
                    2.09,
                    2.41,
                    2.73,
                    3.07,
                    3.41,
                    3.77,
                    4.13,
                    4.51,
                ),
                rank_falloff=0.97,
                rank_lowpass=1.0,
            ),
        ],
        attack=0.5,
        release=1.2,
        notes="The sustained version: quieter, longer, and thinned out so it "
        "can sit under a fight for five seconds without drawing attention.",
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
    "arrows_whistle_snap_impact": Cue(
        path="sfx/combat/arrows_whistle_snap_impact",
        seconds=1.5,
        layers=[
            Layer(
                url=SWISH_STICK[0],
                origin=SWISH_STICK[1],
                licence=CC0,
                start=0.1,
                gain=0.9,
                highpass=450.0,
            ),
            Layer(
                url=BOXING[0],
                origin=BOXING[1],
                licence=CC0,
                start=0.35,
                gain=0.85,
                highpass=140.0,
                ranks=(0.14,),
                rank_falloff=0.8,
            ),
        ],
        attack=0.003,
        release=0.3,
        notes="The one arrow cue that lands: a swish with an impact under its "
        "tail rather than a swish that fades out.",
    ),
    "arrows_impact_shields_dirt": Cue(
        path="sfx/combat/arrows_impact_shields_dirt",
        seconds=2.0,
        layers=[
            Layer(
                url=BOXING[0],
                origin=BOXING[1],
                licence=CC0,
                start=0.2,
                gain=1.0,
                highpass=120.0,
                lowpass=4200.0,
                ranks=(0.13, 0.29, 0.47),
                rank_falloff=0.84,
            ),
            Layer(
                url=BODYFALL_GRASS[0],
                origin=BODYFALL_GRASS[1],
                licence=CC0,
                start=0.3,
                gain=0.5,
                lowpass=1800.0,
                ranks=(0.21,),
                rank_falloff=0.8,
            ),
        ],
        attack=0.003,
        release=0.45,
        notes="Arrows arriving: hits on hide over hits in dirt, which is what "
        "a volley landing across a shield line and the ground around it is.",
    ),
    "roman_cavalry_charge": Cue(
        path="sfx/combat/roman_cavalry_charge",
        seconds=5.0,
        layers=[
            Layer(
                url=GALLOP_01[0],
                origin=GALLOP_01[1],
                licence=CC0,
                start=0.2,
                gain=1.0,
                lowpass=5000.0,
                loop_source=True,
                ranks=(0.23, 0.61),
                rank_falloff=0.8,
            ),
            Layer(
                url=GALLOP_02[0],
                origin=GALLOP_02[1],
                licence=CC0,
                start=0.5,
                gain=0.8,
                lowpass=3800.0,
                loop_source=True,
                ranks=(0.37,),
                rank_falloff=0.8,
            ),
            Layer(
                url=GALLOP_03[0],
                origin=GALLOP_03[1],
                licence=CC0,
                start=0.9,
                gain=0.62,
                lowpass=2800.0,
                loop_source=True,
            ),
        ],
        attack=0.25,
        release=0.9,
        notes="Three different gallop performances rather than one at three "
        "offsets: horses that are actually out of step with each other.",
    ),
    "numidian_cavalry_chase": Cue(
        path="sfx/combat/numidian_cavalry_chase",
        seconds=5.5,
        layers=[
            Layer(
                url=GALLOP_02[0],
                origin=GALLOP_02[1],
                licence=CC0,
                start=0.3,
                gain=1.0,
                highpass=90.0,
                lowpass=6000.0,
                loop_source=True,
                ranks=(0.19, 0.44, 0.79),
                rank_falloff=0.84,
            ),
            Layer(
                url=GALLOP_03[0],
                origin=GALLOP_03[1],
                licence=CC0,
                start=1.1,
                gain=0.7,
                lowpass=4200.0,
                loop_source=True,
                ranks=(0.31,),
                rank_falloff=0.8,
            ),
        ],
        attack=0.2,
        release=1.0,
        notes="Numidian horse was light and fast, so this is denser and "
        "brighter than the Roman charge and keeps its top end.",
    ),
    "horse_gallop_close_pass": Cue(
        path="sfx/combat/horse_gallop_close_pass",
        seconds=3.0,
        layers=[
            Layer(
                url=GALLOP_01[0],
                origin=GALLOP_01[1],
                licence=CC0,
                start=0.2,
                gain=1.1,
                highpass=70.0,
                loop_source=True,
                ranks=(0.07, 0.17, 0.29, 0.41, 0.55, 0.71, 0.89, 1.09),
                rank_falloff=0.94,
                rank_lowpass=1.0,
            ),
        ],
        attack=0.35,
        release=0.8,
        notes="One horse, undimmed and unlayered. The long attack and release "
        "are the pass: it arrives and it leaves.",
    ),
    "roman_shield_wall_impact": Cue(
        path="sfx/combat/roman_shield_wall_impact",
        seconds=2.2,
        layers=[
            Layer(
                url=BODYFALL_GRASS[0],
                origin=BODYFALL_GRASS[1],
                licence=CC0,
                start=0.15,
                gain=1.0,
                lowpass=2600.0,
                ranks=(0.05, 0.12),
                rank_falloff=0.85,
            ),
            Layer(
                url=CLANG_DULL[0],
                origin=CLANG_DULL[1],
                licence=CC0,
                start=0.08,
                gain=0.55,
                highpass=180.0,
                lowpass=3600.0,
                ranks=(0.07, 0.16),
                rank_falloff=0.8,
            ),
        ],
        attack=0.003,
        release=0.6,
        notes="A shield wall taking a charge is mass first and metal second, "
        "so the bodyfall is the loud layer and the clang only colours it.",
    ),
    "gladius_shield_impacts_close": Cue(
        path="sfx/combat/gladius_shield_impacts_close",
        seconds=2.4,
        layers=[
            Layer(
                url=SMACKS_RAPID[0],
                origin=SMACKS_RAPID[1],
                licence=CC0,
                start=0.1,
                gain=0.95,
                highpass=150.0,
                lowpass=5000.0,
            ),
            Layer(
                url=CLANG_THIN_01[0],
                origin=CLANG_THIN_01[1],
                licence=CC0,
                start=0.05,
                gain=0.7,
                highpass=280.0,
                ranks=(0.11, 0.27, 0.44),
                rank_falloff=0.82,
            ),
        ],
        attack=0.003,
        release=0.5,
        notes="Short sword on shield, worked fast. Thin clangs rather than the "
        "big ones: a gladius is a stabbing blade, not a bell.",
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
                url=HORN_VIKING[0],
                origin=HORN_VIKING[1],
                licence=CC0,
                start=0.30,
                gain=1.0,
                highpass=70.0,
                ranks=(0.74, 1.51),
                rank_falloff=0.62,
                rank_lowpass=0.7,
            ),
        ],
        attack=0.02,
        release=0.7,
        notes="Three blasts, each further off and darker than the last: an "
        "order being relayed down a line rather than one horn sounding.",
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
        seconds=1.0,
        layers=[
            Layer(
                url=DOG_SNARL[0],
                origin=DOG_SNARL[1],
                licence=CC0,
                start=1.4,
                gain=1.0,
                highpass=110.0,
                lowpass=6500.0,
            ),
        ],
        attack=0.003,
        release=0.25,
        notes="The sixth wildlife cue, joining the five already cut from real "
        "canid recordings. Pitched material from a dog for the same reason "
        "those are: it is the same species.",
    ),
}
