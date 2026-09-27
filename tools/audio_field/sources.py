"""Where every recorded ambience bed comes from and how it is cut.

One entry per bed in `assets/audio/ambience`. The entry is the whole recipe:
the public-domain or CC0 recording it is cut from, the window inside that
recording, and the shaping applied afterwards. Nothing here is a stock library
purchase and nothing is synthesised -- see tools/audio_synth for the generated
cue set, which is a separate thing with separate rules.

Keep this table and THIRD_PARTY_LICENSES.md in step: the licence column there
is the shipping obligation, this file is how the file gets rebuilt.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

RECORDINGS = Path(__file__).resolve().parent / "recordings"


def own(name: str) -> str:
    """A `file://` URL for a recording this project made itself.

    Most layers here are fetched from an archive, which is what makes a bed
    rebuildable anywhere. A recording the project owns has no such URL, so it
    is committed under `tools/audio_field/recordings` and addressed locally.
    `urllib` opens `file://` like any other scheme, so the builder needs no
    special case.
    """
    return (RECORDINGS / name).as_uri()


@dataclass(frozen=True)
class Source:
    """One recording a bed draws on."""

    url: str
    """Direct download for the original, unedited file."""

    origin: str
    """Human-readable provenance, repeated in THIRD_PARTY_LICENSES.md."""

    licence: str

    start: float
    """Seconds into the source where the usable window begins."""

    gain: float = 1.0
    """Linear gain applied before layers are summed."""

    highpass: float = 0.0
    lowpass: float = 0.0

    shelf_db: float = 0.0
    """High shelf, in dB, above `shelf_hz`.

    Field recordings of birds, insects and rain carry most of their energy in
    2-6 kHz, which is exactly the band `AmbienceAssetsTest` requires a bed to
    stay out of -- a bed lives under combat and speech for a whole mission, so
    it may not compete there. The shelf is how a bright recording is made to
    sit down, and the body layers below are how it keeps sounding like a place
    rather than a muffled one.
    """

    shelf_hz: float = 2000.0

    ranks: tuple[float, ...] = ()
    """Extra copies of this layer, offset by these many seconds each.

    A marching column is the one thing no public-domain library holds: field
    recordings are of one person walking. Summing the same walk against itself
    at incommensurate offsets is what turns it into a body of men -- the ear
    reads overlapping unsynchronised footfalls as a crowd, which is also why
    the offsets must not be multiples of each other. Each copy is quieter and
    darker than the last, standing in for distance.
    """

    rank_falloff: float = 0.75
    """Gain multiplier applied per extra rank."""

    rank_lowpass: float = 0.8
    """Cutoff multiplier applied per extra rank, so distant ranks lose their top."""

    speed: float = 1.0
    """Playback-rate multiplier, applied before every filter.

    Pitch and length move together, as they do when a recording is played
    faster -- there is no time-stretch here and none is wanted. What it is for
    is transposing a real instrument into the register the cue needs: the only
    horn in the CC0 libraries with a genuine low body is a didgeridoo sitting
    an octave and a half under a war horn, and a war horn is a didgeridoo
    played fast. Because it runs first, `highpass` and `lowpass` are cutoffs in
    the *output* register rather than the source's.
    """

    loop_source: bool = False
    """Repeat the source to fill the bed instead of running out.

    The footstep libraries hold ten to sixteen seconds of walking and a bed
    runs twenty-two. Without this the layer simply stops, and a mix is only as
    long as its shortest layer.
    """

    delay: float = 0.0
    """Seconds into a composed cue before this layer enters.

    Only `build_battle.py` reads it. A bed is a loop and has no "into"; a cue
    is a short sequence -- a hinge creak and then the slam -- and the second
    event has to land where the first one leaves off.
    """

    length: float = 0.0
    """Seconds of the source to use in a composed cue, or 0.0 for all of it.

    Also `build_battle.py` only. A recording rarely stops where the cue needs
    it to: a door creak that runs on under the slam reads as two doors.
    """


@dataclass(frozen=True)
class Bed:
    """One shipped `.ogg`, built by summing its layers."""

    seconds: float
    layers: list[Source]
    fade: float = 1.5
    """Seconds of tail folded back over the head to seal the loop."""

    notes: str = ""


ARCHIVE = "https://archive.org/download"
COMMONS = "https://upload.wikimedia.org/wikipedia/commons"

CANIGOU = f"{ARCHIVE}/aporee_65562_75723/tramontanesurlescaillouxausommetduCanigou.flac"
CANIGOU_ORIGIN = (
    "radio aporee ::: maps, Pic du Canigou, France, "
    "'Sound of Wind on top of the Canigou Mountain'"
)

WIND_THRU_TREES = (
    f"{ARCHIVE}/Designers-Choice-Collection-Wind/"
    "WIND%2FVEGETATION%2FWINDVege-Samsung%20Galaxy%20Smartphone%2C%20"
    "CU_Thru%20Trees%2C%20Rustling%2C%20Faint%20Crickets_Nicholas%20Judy_TDC.wav"
)
WIND_THRU_TREES_ORIGIN = (
    "The Designer's Choice UCS Collection - WIND, "
    "'WINDVege-Samsung Galaxy Smartphone, CU_Thru Trees, Rustling, "
    "Faint Crickets', by Nicholas A. Judy"
)

MARCH_FEET = (
    f"{ARCHIVE}/Designers-Choice-Collection-Footsteps/"
    "FOOTSTEPS%2FHUMAN%2FFEETHmn-Samsung%20Galaxy%20Smartphone%2C%20"
    "MCU_Running%2C%20Rocky%20Road_Nicholas%20Judy_TDC.wav"
)
MARCH_FEET_ORIGIN = (
    "The Designer's Choice UCS Collection - FOOTSTEPS, "
    "'FEETHmn-…MCU_Running, Rocky Road', by Nicholas A. Judy"
)

WALK_FEET = (
    f"{ARCHIVE}/Designers-Choice-Collection-Footsteps/"
    "FOOTSTEPS%2FHUMAN%2FFEETHmn-Samsung%20Galaxy%20Smartphone%2C%20"
    "CU_Footsteps%2C%20Rocky%20Surface_Nicholas%20Judy_TDC.wav"
)
WALK_FEET_ORIGIN = (
    "The Designer's Choice UCS Collection - FOOTSTEPS, "
    "'FEETHmn-…CU_Footsteps, Rocky Surface', by Nicholas A. Judy"
)

DRY_HILLSIDE = f"{ARCHIVE}/aporee_67722_78391/woodedhillside.mp3"
DRY_HILLSIDE_ORIGIN = "radio aporee ::: maps, 'dry hillside, parched wind'"

DESERT_WIND = f"{ARCHIVE}/aporee_44293_50371/sandstorm.mp3"
DESERT_WIND_ORIGIN = "radio aporee ::: maps, Cafe Tissardmine, Morocco, 'Desert Wind'"

COLUMN_OFFSETS = (0.37, 0.91, 1.63, 2.29)
"""Rank offsets for a marching column, in seconds.

Deliberately not multiples of one another: equal spacing would land the copies
in step and produce one very loud walker rather than several quiet ones.
"""

HOSINGEN_FORGE = (
    f"{ARCHIVE}/aporee_14355_16708/HosingenNaturparkhausFamilienfestSchmiedNageln.mp3"
)
HOSINGEN_FORGE_ORIGIN = (
    "radio aporee ::: maps, Hosingen, Luxembourg, 'summer fair, forging stand'"
)
SIEGBURG_SMITH = (
    f"{ARCHIVE}/aporee_26643_30744/"
    "WeihnachtsmarktSiegburgDerSchmiedheiztseinFeuerundSchmiedet.mp3"
)
SIEGBURG_SMITH_ORIGIN = (
    "radio aporee ::: maps, Siegburg, Germany, "
    "'Blacksmith on the medieval Christmas market'"
)
CAMPFIRE_ROLLEY = f"{ARCHIVE}/aporee_51228_58465/20201004RolleyLakePark.mp3"
CAMPFIRE_ROLLEY_ORIGIN = (
    "radio aporee ::: maps, Rolley Lake Trail, Mission, BC, Canada, 'Campfire at dusk'"
)
CAMPFIRE_BURYANEK = (
    f"{ARCHIVE}/aporee_57170_65434/CampFireBuryanekStateRecreationArea202206052045.mp3"
)
CAMPFIRE_BURYANEK_ORIGIN = (
    "radio aporee ::: maps, Buryanek State Recreation Area, 'Camp Fire'"
)
CRACKLING_FIRE = (
    f"{ARCHIVE}/aporee_27776_32009/soundcamp2015dartingtoncracklingfire.mp3"
)
CRACKLING_FIRE_ORIGIN = (
    "radio aporee ::: maps, Dartington, Devon, UK, "
    "'soundcamp2015dartington crackling fire'"
)
FES_MEDINA = f"{ARCHIVE}/aporee_69516_80936/FESdeambulation13h05.wav"
FES_MEDINA_ORIGIN = (
    "radio aporee ::: maps, Fès, Morocco, 'Médina de Fes Déambulation - Ambiance'"
)
FES_MARBLE = f"{ARCHIVE}/aporee_69606_81036/FESMarbremomes.wav"
FES_MARBLE_ORIGIN = "radio aporee ::: maps, Fès, Morocco, 'les tailleurs sur marbre'"
GHANA_CARPENTER = f"{ARCHIVE}/aporee_61721_71007/BirdsandaCarpenterGhana.wav"
GHANA_CARPENTER_ORIGIN = (
    "radio aporee ::: maps, Koforidua, Ghana, 'Birds and a hammering Carpenter'"
)
DALYAN_MARKET = f"{ARCHIVE}/aporee_32850_37750/Marketchatter.mp3"
DALYAN_MARKET_ORIGIN = (
    "radio aporee ::: maps, Dalyan, Muğla Province, Turkey, 'Dalyan market chatter'"
)
MACHAR_BONFIRE = (
    f"{ARCHIVE}/aporee_32724_37608/MacharDumpJuly232016bonfiregullscrows.mp3"
)
MACHAR_BONFIRE_ORIGIN = (
    "radio aporee ::: maps, Machar municipal landfill, Canada, 'Refuse pile burning'"
)
TYNEHAM_CROWS = f"{ARCHIVE}/aporee_34467_39627/JackdawsRooksandCrows.WAV"
TYNEHAM_CROWS_ORIGIN = "radio aporee ::: maps, Tyneham, UK, 'Jackdaws, Rooks and Crows'"
ROVINJ_GULLS = f"{ARCHIVE}/aporee_16431_19043/galebiinstarci1.wav"
ROVINJ_GULLS_ORIGIN = (
    "radio aporee ::: maps, Rovinj, Croatia, 'seagulls and old people'"
)
CALETTA_MASTS = f"{ARCHIVE}/aporee_63402_72940/mastalines.mp3"
CALETTA_MASTS_ORIGIN = (
    "radio aporee ::: maps, Piazza del Porto, La Caletta, Italy, 'noon port ambience'"
)
CAPE_CLEAR_HARBOUR = f"{ARCHIVE}/aporee_50821_57980/CapeCleareditedraw.wav"
CAPE_CLEAR_HARBOUR_ORIGIN = (
    "radio aporee ::: maps, Oileán Chléire, Ireland, 'North Harbour, Cape Clear Island'"
)

PD_MARK = "CC Public Domain Mark 1.0"
CC0 = "CC0 1.0"
PD = "Public domain"
OWN_WORK = "Own recording (project-owned, no third-party obligation)"


BEDS: dict[str, Bed] = {
    "alpine_mountain_pass": Bed(
        seconds=22.0,
        notes="Tramontane over loose stone at 2784 m -- the gusts are the source's own.",
        layers=[
            Source(
                url=CANIGOU,
                origin=CANIGOU_ORIGIN,
                licence=PD_MARK,
                start=123.0,
                highpass=80.0,
            )
        ],
    ),
    "mediterranean_plains": Bed(
        seconds=22.0,
        notes="Open summer meadow: insects and distant birds over the wind that "
        "is actually moving the grass. The wind is a different window of the "
        "Canigou recording from the one the mountain bed uses -- share a window "
        "and a march between the two biomes gives the trick away.",
        layers=[
            Source(
                url=f"{ARCHIVE}/aporee_57775_66141/DrflisWiese0622.mp3",
                origin="radio aporee ::: maps, Dörflis, Naturpark Haßberge, Germany, "
                "'Wild Meadow Summer'",
                licence=PD_MARK,
                start=111.0,
                gain=0.8,
                highpass=70.0,
                shelf_db=-11.0,
            ),
            Source(
                url=CANIGOU,
                origin=CANIGOU_ORIGIN,
                licence=PD_MARK,
                start=80.0,
                gain=0.9,
                highpass=70.0,
                lowpass=1200.0,
            ),
        ],
    ),
    "forest_ambush": Bed(
        seconds=22.0,
        notes="Birds close and far over wind in the canopy. The birds are "
        "shelved down hard: they own 2-6 kHz, which is where combat lives.",
        layers=[
            Source(
                url=f"{ARCHIVE}/aporee_57134_65382/Ptikipriizviru.mp3",
                origin="radio aporee ::: maps, Planina Razor, Tolmin, Slovenia, "
                "'Birds in forest'",
                licence=PD_MARK,
                start=342.0,
                gain=0.8,
                highpass=70.0,
                shelf_db=-11.0,
            ),
            Source(
                url=WIND_THRU_TREES,
                origin=WIND_THRU_TREES_ORIGIN,
                licence=CC0,
                start=1.0,
                gain=0.7,
                highpass=70.0,
                lowpass=1600.0,
            ),
        ],
    ),
    "river_crossing": Bed(
        seconds=22.0,
        notes="Shallow river under a bridge -- broadband and steady, which is "
        "what makes it loop without a seam.",
        layers=[
            Source(
                url=f"{ARCHIVE}/aporee_48945_55734/joneliskisunderbridge.flac",
                origin="radio aporee ::: maps, Joneliškės, Lithuania, 'river Viesa'",
                licence=PD_MARK,
                start=15.0,
                highpass=60.0,
                shelf_db=-6.0,
            )
        ],
    ),
    "storm": Bed(
        seconds=22.0,
        notes="Steady storm rain with close drop impacts, cut from the "
        "project's own thunderstorm recording. It runs 16.6 s against a 22 s "
        "bed, so the layer loops. Real rain carries most of its energy in "
        "2-6 kHz and this one is no exception; the shelf pulls it under the "
        "body so a camp fire can still be heard underneath it.",
        layers=[
            Source(
                url=own("storm.ogg"),
                origin="Own recording, Karlsruhe, 16 July 2026",
                licence=OWN_WORK,
                start=0.6,
                gain=1.0,
                highpass=90.0,
                lowpass=3600.0,
                shelf_db=-7.0,
                loop_source=True,
            )
        ],
    ),
    "camp_fire_night": Bed(
        seconds=22.0,
        notes="A hearth close enough to hear individual cracks. The same "
        "public-domain fireplace recording that sits under mountain_camp_night, "
        "but forward rather than underneath. The source runs 25.5 s, so a 22 s "
        "bed has almost no room to choose a window: this starts at 3.4 s and "
        "runs to the end, which is the largest offset available from the 1.0 s "
        "the other bed uses and keeps the two from cracking in unison when "
        "they play together. The recording is quiet, so the builder applies "
        "about 30 dB of make-up.",
        layers=[
            Source(
                url=f"{COMMONS}/d/d8/Dry_grass_burning_in_open_fireplace.ogg",
                origin="Wikimedia Commons, "
                "File:Dry grass burning in open fireplace.ogg, by ezwa",
                licence=PD,
                start=3.4,
                gain=1.0,
                highpass=80.0,
                lowpass=3200.0,
                shelf_db=-4.0,
            )
        ],
    ),
    "mountain_camp_night": Bed(
        seconds=22.0,
        notes="Night crickets with a fire worked in underneath. The fire is the "
        "body here, which is why it runs at full gain and the crickets do not. "
        "It starts at 1 s because the recording is only 25 s long and a mix is "
        "as long as its shortest layer.",
        layers=[
            Source(
                url=f"{ARCHIVE}/aporee_54442_62367/"
                "835aABBORSUKImidnightcricketssnoringandunknownpitchMixPre353.flac",
                origin="radio aporee ::: maps, Pod Lipą, Borsuki, Poland, "
                "'(835a AB) midnight crickets'",
                licence=PD_MARK,
                start=80.0,
                gain=0.8,
                highpass=90.0,
                shelf_db=-9.0,
            ),
            Source(
                url=f"{COMMONS}/d/d8/Dry_grass_burning_in_open_fireplace.ogg",
                origin="Wikimedia Commons, "
                "File:Dry grass burning in open fireplace.ogg, by ezwa",
                licence=PD,
                start=1.0,
                gain=1.0,
                highpass=90.0,
                lowpass=2400.0,
            ),
        ],
    ),
    "battlefield_dry_wind_distant_march_01": Bed(
        seconds=22.0,
        notes="Dry wind over open ground with a column somewhere off to one "
        "side. This is the bed a mission falls back to, so it is the one the "
        "player hears most: it has to stay out of the way.",
        layers=[
            Source(
                url=DRY_HILLSIDE,
                origin=DRY_HILLSIDE_ORIGIN,
                licence=PD_MARK,
                start=222.0,
                highpass=70.0,
            ),
            Source(
                url=MARCH_FEET,
                origin=MARCH_FEET_ORIGIN,
                licence=CC0,
                start=1.0,
                gain=0.5,
                highpass=90.0,
                lowpass=2200.0,
                ranks=COLUMN_OFFSETS,
                loop_source=True,
            ),
        ],
    ),
    "battlefield_dry_wind_distant_march_02": Bed(
        seconds=22.0,
        notes="The same place, a different hour: a slower column further off.",
        layers=[
            Source(
                url=DRY_HILLSIDE,
                origin=DRY_HILLSIDE_ORIGIN,
                licence=PD_MARK,
                start=300.0,
                highpass=70.0,
            ),
            Source(
                url=WALK_FEET,
                origin=WALK_FEET_ORIGIN,
                licence=CC0,
                start=0.5,
                gain=0.42,
                highpass=90.0,
                lowpass=1800.0,
                ranks=COLUMN_OFFSETS,
                loop_source=True,
            ),
        ],
    ),
    "desert_army_march": Bed(
        seconds=22.0,
        notes="Carthage on the move. The wind is a real Moroccan sandstorm, "
        "which carries far more low end than the Pyrenean wind the other beds "
        "use -- that difference is the whole point of a separate desert bed.",
        layers=[
            Source(
                url=DESERT_WIND,
                origin=DESERT_WIND_ORIGIN,
                licence=PD_MARK,
                start=191.0,
                highpass=70.0,
            ),
            Source(
                url=MARCH_FEET,
                origin=MARCH_FEET_ORIGIN,
                licence=CC0,
                start=2.0,
                gain=0.42,
                highpass=100.0,
                lowpass=2000.0,
                ranks=COLUMN_OFFSETS,
                loop_source=True,
            ),
        ],
    ),
    "roman_road": Bed(
        seconds=22.0,
        notes="A column on a paved road: closer and harder underfoot than the "
        "battlefield beds, with the wind pulled back so the marching leads.",
        layers=[
            Source(
                url=WALK_FEET,
                origin=WALK_FEET_ORIGIN,
                licence=CC0,
                start=0.5,
                gain=0.72,
                highpass=90.0,
                lowpass=3200.0,
                ranks=COLUMN_OFFSETS,
                loop_source=True,
            ),
            Source(
                url=DRY_HILLSIDE,
                origin=DRY_HILLSIDE_ORIGIN,
                licence=PD_MARK,
                start=222.0,
                gain=0.6,
                highpass=70.0,
                lowpass=1400.0,
            ),
        ],
    ),
    "weather_rain": Bed(
        seconds=22.0,
        notes="Light steady rain, no thunder: this layers over whatever biome "
        "bed is already playing, so it must not carry events of its own.",
        layers=[
            Source(
                url=f"{ARCHIVE}/Designers-Choice-Collection-Rain/"
                "RAIN%2FGENERAL%2FRAIN-Samsung%20Galaxy%20Smartphone%2C%20"
                "CU_Raining_Nicholas%20Judy_TDC.wav",
                origin="The Designer's Choice UCS Collection - RAIN, "
                "'RAIN-Samsung Galaxy Smartphone, CU_Raining', by Nicholas A. Judy",
                licence=CC0,
                start=35.0,
                highpass=70.0,
                shelf_db=-9.0,
            )
        ],
    ),
    "weather_snow": Bed(
        seconds=22.0,
        notes="Blizzard rather than falling snow. Snow itself is silent; the "
        "wind carrying it is the sound the player is owed.",
        layers=[
            Source(
                url=f"{ARCHIVE}/Designers-Choice-Collection-Wind/"
                "WIND%2FTURBULENT%2FWINDTurb-CU_Blizzard%2C%20"
                "Old%20Recording_Nicholas%20Judy_TDC.wav",
                origin="The Designer's Choice UCS Collection - WIND, "
                "'WINDTurb-CU_Blizzard, Old Recording', by Nicholas A. Judy",
                licence=CC0,
                start=23.0,
                highpass=80.0,
                lowpass=5000.0,
                shelf_db=-15.0,
            )
        ],
    ),
    "roman_army_camp_01": Bed(
        seconds=22.0,
        notes="A smith working small plates in a tent with people talking round "
        "him, a camp fire, and a market's chatter pulled far back as the rest "
        "of the camp.",
        layers=[
            Source(
                url=HOSINGEN_FORGE,
                origin=HOSINGEN_FORGE_ORIGIN,
                licence=PD_MARK,
                start=45.0,
                highpass=80.0,
                shelf_db=-6.0,
            ),
            Source(
                url=CAMPFIRE_ROLLEY,
                origin=CAMPFIRE_ROLLEY_ORIGIN,
                licence=PD_MARK,
                start=36.0,
                gain=0.8,
                highpass=60.0,
                shelf_db=-4.0,
            ),
            Source(
                url=DALYAN_MARKET,
                origin=DALYAN_MARKET_ORIGIN,
                licence=PD_MARK,
                start=26.0,
                gain=0.5,
                highpass=120.0,
                lowpass=900.0,
            ),
        ],
    ),
    "roman_army_camp_02": Bed(
        seconds=22.0,
        notes="The second camp is a smith heating his forge with a foot bellows "
        "at a medieval market, over a second fire and the first bed's smith "
        "pulled back. Different recordings from _01 throughout, so the two "
        "variants never share an event.",
        layers=[
            Source(
                url=SIEGBURG_SMITH,
                origin=SIEGBURG_SMITH_ORIGIN,
                licence=PD_MARK,
                start=21.0,
                highpass=80.0,
                shelf_db=-6.0,
            ),
            Source(
                url=CAMPFIRE_BURYANEK,
                origin=CAMPFIRE_BURYANEK_ORIGIN,
                licence=PD_MARK,
                start=181.0,
                gain=1.2,
                highpass=60.0,
                shelf_db=-4.0,
            ),
            Source(
                url=HOSINGEN_FORGE,
                origin=HOSINGEN_FORGE_ORIGIN,
                licence=PD_MARK,
                start=69.0,
                gain=0.5,
                highpass=80.0,
                lowpass=1500.0,
            ),
        ],
    ),
    "carthage_war_camp_01": Bed(
        seconds=22.0,
        notes="A North African street with no engine in it -- the Fes medina "
        "is closed to cars -- over a fire, with stone carvers tapping further "
        "off. The medina carries no low rumble at all, which is rare in a "
        "street recording and why it was chosen.",
        layers=[
            Source(
                url=FES_MEDINA,
                origin=FES_MEDINA_ORIGIN,
                licence=PD_MARK,
                start=63.0,
                highpass=80.0,
                lowpass=2500.0,
                shelf_db=-4.0,
            ),
            Source(
                url=CAMPFIRE_BURYANEK,
                origin=CAMPFIRE_BURYANEK_ORIGIN,
                licence=PD_MARK,
                start=204.0,
                gain=1.5,
                highpass=60.0,
                shelf_db=-4.0,
            ),
            Source(
                url=FES_MARBLE,
                origin=FES_MARBLE_ORIGIN,
                licence=PD_MARK,
                start=80.0,
                gain=0.4,
                highpass=100.0,
                lowpass=3000.0,
                shelf_db=-6.0,
            ),
        ],
    ),
    "carthage_war_camp_02": Bed(
        seconds=22.0,
        notes="The medina again at a later window, a closer crackling fire, "
        "and the bellows smith far back.",
        layers=[
            Source(
                url=FES_MEDINA,
                origin=FES_MEDINA_ORIGIN,
                licence=PD_MARK,
                start=110.0,
                highpass=80.0,
                lowpass=2500.0,
                shelf_db=-4.0,
            ),
            Source(
                url=CRACKLING_FIRE,
                origin=CRACKLING_FIRE_ORIGIN,
                licence=PD_MARK,
                start=18.0,
                gain=0.8,
                highpass=60.0,
                shelf_db=-6.0,
            ),
            Source(
                url=SIEGBURG_SMITH,
                origin=SIEGBURG_SMITH_ORIGIN,
                licence=PD_MARK,
                start=33.0,
                gain=0.4,
                highpass=80.0,
                lowpass=1500.0,
            ),
        ],
    ),
    "siege_camp": Bed(
        seconds=22.0,
        notes="Works going on: a carpenter hammering, a smith, stone being cut. "
        "The carpenter was recorded under singing birds, so that layer is "
        "shelved hard -- the birds own 2-6 kHz.",
        layers=[
            Source(
                url=GHANA_CARPENTER,
                origin=GHANA_CARPENTER_ORIGIN,
                licence=PD_MARK,
                start=23.0,
                highpass=100.0,
                shelf_db=-9.0,
            ),
            Source(
                url=HOSINGEN_FORGE,
                origin=HOSINGEN_FORGE_ORIGIN,
                licence=PD_MARK,
                start=57.0,
                gain=0.7,
                highpass=80.0,
                shelf_db=-6.0,
            ),
            Source(
                url=FES_MARBLE,
                origin=FES_MARBLE_ORIGIN,
                licence=PD_MARK,
                start=105.0,
                gain=0.5,
                highpass=100.0,
                lowpass=3000.0,
                shelf_db=-6.0,
            ),
        ],
    ),
    "burning_village_aftermath": Bed(
        seconds=22.0,
        notes="A large bonfire of wood and brush burning out in the open, crows "
        "and rooks over it, and wind. The bonfire is the steadiest recording "
        "in the set -- under 1 dB of variation per second -- so the loop "
        "carries no event to give it away.",
        layers=[
            Source(
                url=MACHAR_BONFIRE,
                origin=MACHAR_BONFIRE_ORIGIN,
                licence=PD_MARK,
                start=32.0,
                highpass=60.0,
                shelf_db=-6.0,
            ),
            Source(
                url=TYNEHAM_CROWS,
                origin=TYNEHAM_CROWS_ORIGIN,
                licence=PD_MARK,
                start=155.0,
                gain=0.5,
                highpass=150.0,
                shelf_db=-6.0,
            ),
            Source(
                url=CANIGOU,
                origin=CANIGOU_ORIGIN,
                licence=PD_MARK,
                start=20.0,
                gain=0.6,
                highpass=70.0,
                lowpass=1200.0,
            ),
        ],
    ),
    "mediterranean_harbor": Bed(
        seconds=22.0,
        notes="Gulls over a fish market's leftovers with old men talking and "
        "small waves, halyards rattling against masts, and a quieter harbour "
        "at dusk behind. The masts were recorded in strong wind, so that "
        "layer keeps only what is above 300 Hz.",
        layers=[
            Source(
                url=ROVINJ_GULLS,
                origin=ROVINJ_GULLS_ORIGIN,
                licence=PD_MARK,
                start=145.0,
                highpass=70.0,
                shelf_db=-6.0,
            ),
            Source(
                url=CALETTA_MASTS,
                origin=CALETTA_MASTS_ORIGIN,
                licence=PD_MARK,
                start=37.0,
                gain=0.6,
                highpass=300.0,
                shelf_db=-6.0,
            ),
            Source(
                url=CAPE_CLEAR_HARBOUR,
                origin=CAPE_CLEAR_HARBOUR_ORIGIN,
                licence=PD_MARK,
                start=128.0,
                gain=0.6,
                highpass=100.0,
            ),
        ],
    ),
    "mediterranean_city_market": Bed(
        seconds=22.0,
        notes="A Turkish weekly market's chatter and commerce in front, the Fes "
        "medina behind it, and stone carvers at work somewhere off the square.",
        layers=[
            Source(
                url=DALYAN_MARKET,
                origin=DALYAN_MARKET_ORIGIN,
                licence=PD_MARK,
                start=7.0,
                highpass=80.0,
                shelf_db=-6.0,
            ),
            Source(
                url=FES_MEDINA,
                origin=FES_MEDINA_ORIGIN,
                licence=PD_MARK,
                start=150.0,
                gain=0.6,
                highpass=80.0,
                lowpass=2500.0,
            ),
            Source(
                url=FES_MARBLE,
                origin=FES_MARBLE_ORIGIN,
                licence=PD_MARK,
                start=48.0,
                gain=0.3,
                highpass=100.0,
                lowpass=3000.0,
                shelf_db=-6.0,
            ),
        ],
    ),
}


TARGET_LUFS = -19.3
"""Matches the beds this set replaced, so mission mixes need no retuning."""

PEAK_CEILING = 0.5
"""Same headroom the synthesised beds leave.

Decode-time mastering (docs/AUDIO_SYSTEM.md) expects room to work in. A bed
delivered near full scale makes its limiter pull several dB, which is audible
as the bed ducking under itself, so loudness gives way to the ceiling when the
two disagree.
"""

RATE = 48000
QUALITY = 4
