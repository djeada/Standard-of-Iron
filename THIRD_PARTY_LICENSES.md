# Third-Party Software Licenses

This document lists the licenses of third-party software used in Standard of Iron.

## Qt Framework

**License:** GNU Lesser General Public License v3 (LGPL v3)  
**Website:** https://www.qt.io  
**License Text:** https://www.gnu.org/licenses/lgpl-3.0.html  
**Source Code:** https://www.qt.io/download-open-source

### LGPL v3 Compliance

Standard of Iron complies with the LGPL v3 requirements for using Qt:

1. **Dynamic Linking**: Qt is dynamically linked to the application, not statically linked.
    - On Windows: Qt DLLs are deployed alongside the executable using `windeployqt`
    - On Linux: Qt shared libraries (.so) are linked dynamically
    - On macOS: Qt frameworks are linked dynamically

2. **No Modifications**: We do not modify Qt source code. If modifications were made, they would be released under LGPL v3.

3. **License Notice**: LGPL v3 attribution is provided in:
    - This document (THIRD_PARTY_LICENSES.md)
    - README.md (License section)
    - In-game Settings panel (About section)

4. **User Re-linking**: Users can replace the Qt libraries with their own versions because Qt is dynamically linked. This is automatic with dynamic linking - users simply replace the Qt DLLs/shared libraries in the application directory.

### Qt Components Used

- QtCore
- QtGui
- QtWidgets
- QtOpenGL
- QtQuick
- QtQml
- QtQuickControls2
- QtSql
- QtMultimedia

### Verification

To verify dynamic linking:

**Windows:**

```powershell
dumpbin /DEPENDENTS standard_of_iron.exe | findstr Qt
```

**Linux:**

```bash
ldd standard_of_iron | grep Qt
```

**macOS:**

```bash
otool -L standard_of_iron | grep Qt
```

All commands should show Qt libraries as external dependencies, confirming dynamic linking.

## Mesa 3D Windows software renderer

The Windows package includes the 64-bit `opengl32.dll` WGL loader (renamed to
`opengl32sw.dll` for Qt) and `libgallium_wgl.dll` from
[mesa-dist-win 26.1.6](https://github.com/pal1000/mesa-dist-win/releases/tag/26.1.6).
The release workflow records and verifies the upstream archive's SHA-256 before
copying either binary. `GALLIUM_DRIVER=llvmpipe` selects Mesa's CPU renderer.

Mesa's core library and llvmpipe are predominantly MIT licensed; individual
source files retain their own SPDX notices. See the
[Mesa licence index](https://docs.mesa3d.org/license.html) and
[Mesa source](https://gitlab.freedesktop.org/mesa/mesa). The Windows build and
deployment project is also MIT licensed:

> MIT License
>
> Copyright (c) 2017-2020 pal1000
>
> Permission is hereby granted, free of charge, to any person obtaining a copy
> of this software and associated documentation files (the "Software"), to deal
> in the Software without restriction, including without limitation the rights
> to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
> copies of the Software, and to permit persons to whom the Software is
> furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all
> copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
> AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
> LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
> OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
> SOFTWARE.

## Audio Assets

Every entry in `assets/audio/audio_manifest.json` carries a `provenance` block
naming its origin and licence, so the question "may we ship this" is answerable
per file without reading the prose below. The prose stays authoritative for the
exact wording an attribution needs. `assets/audio/audio_provenance_baseline.json`
lists the files whose rights are not written down yet;
`scripts/audio_provenance.py --check`, which `make audio-check` runs, fails when
a file arrives that is on neither list.

### Synthesised cue sounds

The 66 sound effects listed with `"source": "synth"` in
`assets/audio/audio_manifest.json` are generated from the recipes in
`tools/audio_synth/` by `make audio-assets`. They are original work produced by
this repository's own code: nothing is sampled, and no third-party recording or
library is involved. No attribution or licence obligation attaches to them.

Covers the `ui.*`, `order.*`, `state.*` families in full, plus the build and
alert cues. See `tools/audio_synth/README.md`.

Nothing under `sfx/combat/` is synthesised any more. The twenty-one cues that
were — the shield and guard set, the commander's bow, the movement cues, the
siege pair and the charge — are now cut from CC0 recordings like the rest of
the combat audio, and are listed under the composed battle cues below.

### Wildlife effects (`assets/audio/sfx/wildlife/`)

Four of the six files here are cut from the recordings below; the other two,
`wolf_bite_snap.ogg` and `wolf_snarl_bark.ogg`, are CC0 and listed under the
composed cues. Synthesised
versions were tried first and abandoned: measured against this repository's own
`combat/elephant_charge_carthage.ogg`, the generated growl scored **0.994
cosine similarity on its band profile** — it was, spectrally, an elephant, and
listeners heard exactly that. Real canid growls sit far lower (about 60% of
their energy below 160 Hz) and score 0.71-0.87 against the same reference.

| File                    | Origin                                                                                                                  | Licence       |
| ----------------------- | ----------------------------------------------------------------------------------------------------------------------- | ------------- |
| `wolf_howl_distant.ogg` | Wikimedia Commons, [`File:Wolf howls.ogg`](https://commons.wikimedia.org/wiki/File:Wolf_howls.ogg), seconds 13.35-17.55 | Public domain |
| `wolf_howl_near.ogg`    | the same recording, seconds 20.05-23.65                                                                                 | Public domain |
| `wolf_growl_low.ogg`    | PLOS ONE study audio S3, large-dog growl (see below)                                                                    | CC BY 2.5     |
| `wolf_pack_attack.ogg`  | the same recording layered with study audio S2, small-dog growl                                                         | CC BY 2.5     |

**Required attribution** for the two growl-derived cues:

> Growl recordings from Faragó T, Pongrácz P, Miklósi Á, Huber L, Virányi Z,
> Range F (2010), "Dogs' Expectation about Signalers' Body Size by Virtue of
> Their Growls", PLOS ONE, DOI 10.1371/journal.pone.0015175, supporting audio
> S2 and S3, used under [CC BY 2.5](https://creativecommons.org/licenses/by/2.5).
> Pitched down, trimmed, layered and loudness-normalised for this project.

`wolf_snarl_bark.ogg` used to be a third cue from this recording. It is now
composed from a CC0 dog bark instead, so two files rather than three carry the
attribution above.

A dog is _Canis lupus familiaris_ — the same species as the wolf — so its growl
is the correct sound rather than an approximation. Note that supporting audio S1
from the same paper is **Brownian noise**, a synthetic control from the
experiment, not an animal at all; check the per-file description before using
anything from a study dataset.

The wolf-howl extracts are trimmed, high-passed at 90 Hz, faded and
loudness-normalised; no other material is mixed into them. Public-domain
material carries no attribution obligation, but the source is recorded here so
the provenance of every shipped file is answerable.

### Human death cry (`assets/audio/sfx/combat/human_death_cry.ogg`)

| File                  | Origin                                                                         | Licence                |
| --------------------- | ------------------------------------------------------------------------------ | ---------------------- |
| `human_death_cry.ogg` | Internet Archive item [`male_scream`](https://archive.org/details/male_scream) | Public Domain Mark 1.0 |

Trimmed of its lead-in, high-passed at 90 Hz, faded and loudness-normalised;
nothing is mixed into it.

### Recorded combat and movement cues (`assets/audio/sfx/`)

Thirty-one short one-shots, all cut from **The Designer's Choice UCS
Collection** — original recordings by Nicholas A. Judy, released CC0 1.0 and
explicitly cleared for commercial use. Each file is a single hit or footstep
sliced out of a longer performance by `tools/audio_field/build_oneshots.py`;
`tools/audio_field/oneshots.py` records which volume, how many variants, and
the shaping applied.

| Files                            | Source volume and recording                                                                                       |
| -------------------------------- | ----------------------------------------------------------------------------------------------------------------- |
| `combat/sword_hit_01..04`        | [WEAPONS](https://archive.org/details/Designers-Choice-Collection-Weapons), `WEAPSwrd-…CU_Sword, Hits`            |
| `combat/blade_clash_01..04`      | the same volume, `WEAPSwrd-…Sword, Hits, Scrapes, Shings`                                                         |
| `combat/spear_impact_01..02`     | [WOOD](https://archive.org/details/Designers-Choice-Collection-Wood), `WOODImpt-…CU_Board Drop 01` and `02`       |
| `combat/arrow_impact_01..03`     | the same volume, `CU_Board Drop 03`, `04` and `05`                                                                |
| `combat/armour_hit_01..03`       | [METAL](https://archive.org/details/Designers-Choice-Collection-Metal), `METLImpt-…Metal, Clang/Clank, Thin`      |
| `combat/stone_impact_01..03`     | [ROCKS](https://archive.org/details/Designers-Choice-Collection-Rocks), `ROCKCrsh-…CU_Small Stones, Kicked`       |
| `movement/footstep_grass_01..04` | [FOOTSTEPS](https://archive.org/details/Designers-Choice-Collection-Footsteps), `FEETHmn-MCU_Footsteps, On Grass` |
| `movement/footstep_stone_01..04` | the same volume, `FEETHmn-…CU_Footsteps, Rocky Surface`                                                           |
| `movement/footstep_run_01..04`   | the same volume, `FEETHmn-…MCU_Running, Rocky Road`                                                               |

CC0 requires no attribution. It is recorded here for the same reason as
everything else in this file: the provenance of every shipped sample should be
answerable.

These replaced the five-second mass-battle recordings that had been standing in
for individual impacts. Those recordings are still shipped, now bound to the
cues that actually want a whole battle — the volley, charge and ambient-state
stingers — rather than to cues that fire every ninety milliseconds.

### Recorded ambience beds (`assets/audio/ambience/`)

Thirteen of the twenty-one looping beds are cut from recordings; the other
eight are generated by `tools/audio_synth` and carry no obligation. One of the
thirteen is the project's own recording and carries no obligation either. The
windows, gains and filtering are in `tools/audio_field/sources.py`, and
`make audio-field-ambience` rebuilds any bed from the sources below.

| Bed                                             | Source                                                                                                                                                        | Licence                |
| ----------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------- |
| `alpine_mountain_pass`                          | radio aporee ::: maps, [Pic du Canigou, France](https://archive.org/details/aporee_65562_75723)                                                               | Public Domain Mark 1.0 |
| `mediterranean_plains`                          | radio aporee ::: maps, [Dörflis, Naturpark Haßberge, Germany](https://archive.org/details/aporee_57775_66141), over a second window of the Canigou recording  | Public Domain Mark 1.0 |
| `forest_ambush`                                 | radio aporee ::: maps, [Planina Razor, Tolmin, Slovenia](https://archive.org/details/aporee_57134_65382), over the wind bed below                             | Public Domain Mark 1.0 |
| `river_crossing`                                | radio aporee ::: maps, [Joneliškės, Lithuania](https://archive.org/details/aporee_48945_55734)                                                                | Public Domain Mark 1.0 |
| `mountain_camp_night`                           | radio aporee ::: maps, [Borsuki, Poland](https://archive.org/details/aporee_54442_62367), over the fire below                                                 | Public Domain Mark 1.0 |
| —                                               | Wikimedia Commons, [`File:Dry grass burning in open fireplace.ogg`](https://commons.wikimedia.org/wiki/File:Dry_grass_burning_in_open_fireplace.ogg), by ezwa | Public domain          |
| `storm`                                         | Own recording, Karlsruhe thunderstorm, 16 July 2026, in `tools/audio_field/recordings/`                                                                       | Own work               |
| `camp_fire_night`                               | Wikimedia Commons, [`File:Dry grass burning in open fireplace.ogg`](https://commons.wikimedia.org/wiki/File:Dry_grass_burning_in_open_fireplace.ogg), by ezwa | Public domain          |
| `weather_rain`                                  | [The Designer's Choice UCS Collection — RAIN](https://archive.org/details/Designers-Choice-Collection-Rain), by Nicholas A. Judy                              | CC0 1.0                |
| `weather_snow`                                  | [The Designer's Choice UCS Collection — WIND](https://archive.org/details/Designers-Choice-Collection-Wind), by Nicholas A. Judy                              | CC0 1.0                |
| `battlefield_dry_wind_distant_march_01` / `_02` | radio aporee ::: maps, ['dry hillside, parched wind'](https://archive.org/details/aporee_67722_78391), under a column layered from the CC0 FOOTSTEPS volume   | PD Mark 1.0 / CC0 1.0  |
| `desert_army_march`                             | radio aporee ::: maps, [Cafe Tissardmine, Morocco, 'Desert Wind'](https://archive.org/details/aporee_44293_50371), plus the same column                       | PD Mark 1.0 / CC0 1.0  |
| `roman_road`                                    | the CC0 FOOTSTEPS volume over the dry-hillside wind                                                                                                           | CC0 1.0 / PD Mark 1.0  |
| `forest_ambush` (wind)                          | the same WIND collection, `WINDVege-…Thru Trees, Rustling, Faint Crickets`                                                                                    | CC0 1.0                |

None of these licences requires attribution. They are recorded because the
provenance of every shipped file should be answerable, and because the beds are
committed rather than rendered, so nothing else in the tree says where they came
from.

Each bed is cut to a 22-second window, high-passed, shelved where the source was
too bright for a bed, loop-sealed and levelled to −19.3 LUFS. The Designer's
Choice collections are the uploader's own recordings, released for commercial
use; the aporee recordings are the contributors' own, released into the public
domain through the aporee project.

### Music (`assets/audio/music/`)

The twenty tracks that used to sit here have been **replaced**. The thirty tracks now
shipped were generated for this project in August and September 2026 with **ElevenLabs**,
under a licence held by the project author that permits commercial use.

| Directory   | Tracks | Covers                                                                  |
| ----------- | -----: | ----------------------------------------------------------------------- |
| `menu/`     |      3 | the main theme, its alternate and the Iron Kingdom anthem               |
| `campaign/` |      2 | the campaign map                                                        |
| `base/`     |     12 | nine peaceful beds and three tense ones, including two four-minute beds |
| `combat/`   |      7 | three neutral, two Roman, two Carthaginian                              |
| `stingers/` |      6 | victory, Carthaginian triumph, three defeats and a retreat signal       |

Selection is entirely tag-driven from `assets/audio/audio_manifest.json` — `screen_context`
for the frontend, `ambient_state` in mission, `faction` to bias a nation's own music. No
track id appears in C++ or QML, so the set can be replaced again without touching code.

Format matches what the mixer wants and what every earlier track used: Vorbis, 32 kHz, mono,
`-q:a 4`. Level is the part that is easy to get wrong — `game/audio/audio_mastering.cpp`
normalises music to −15 LUFS but with only ±6 dB of authority, further capped by
`ceiling_db − peak + MAX_LIMITING_DB`, so a −1 dBTP file gets at most 4 dB of lift. A track
baked below about −19 LUFS plays under the rest of the library for good.
`tools/audio_import/import_music.py` is the recipe that lands them there, and its `BATCHES`
table records what each import renamed each master to. The September 2026 batch
(`2026-09-iron`) added three: the `menu/` anthem, a third tense base bed and a third neutral
combat track.

#### What that means for distribution

The restriction that the previous model weights placed on the whole game is gone with those
tracks. No shipped audio now limits how the game is distributed.

The prompts and generation settings were not kept alongside the renders, so these files are
committed artefacts rather than reproducible ones — unlike the synthesised cues, there is no
recipe in the tree that regenerates them from nothing. The 48 kHz stereo FLAC masters they
were baked from are held outside the repository; the shipped files are 32 kHz mono, so a
re-bake at any other rate, or in stereo, needs those masters back.

### Generated cues (`assets/audio/sfx/`, `source: generated`)

Forty one-shots were regenerated in August and September 2026 with **ElevenLabs**, under a licence
held by the project author that permits commercial use, replacing the synthesised cues that measured as oscillators rather
than recordings (spectral flatness 0.024 against 0.336 for the recorded material, and almost
no energy above 6 kHz). They carry `"source": "generated"` in
`assets/audio/audio_manifest.json`, alongside the existing `synth` and `field` values.

`tools/audio_import/import_cues.py` is the import recipe and the record of what became what.
It trims each render from its loudest transient outward — these arrive padded to a round
duration with the sound somewhere inside — matches the RMS of the file it replaces so the
existing mix is unchanged, ceilings at −1.9 dBFS, and encodes to 48 kHz mono Vorbis `-q:a 4`.
A plan entry can ask for `"whole"` instead of the trim, which imports the render at its
delivered length; `combat.heal` and `order.commander_rally` are both sustained pieces where
the approach and the decay are the sound, so both are imported whole.

Twenty-two files were deleted rather than replaced: surplus variants whose cue now uses one
good recording, and the cues that were folded into a shared sound. One refusal now answers
`command.refuse`, `build.placement_rejected` and `combat.ability_refused`; one bell answers
`build.unit_ready`, `build.construction_complete` and `alert.objective_complete`. Pausing and
resuming the game are deliberately silent and their cues are gone entirely.

### Voices (`assets/audio/voices/`)

All thirty-one voice lines were **recorded by Adam Djellouli**, the project
author, and are original work created for this project. No third-party
recording, voice library, or speech-synthesis service is involved, and no
performer's rights other than the author's own attach to them.

| Directory     | Lines | Covers                                          |
| ------------- | ----: | ----------------------------------------------- |
| `roman/`      |    12 | one acknowledgement line per Roman unit type    |
| `carthage/`   |    16 | the Carthaginian unit types, plus named leaders |
| `commanders/` |     3 | Marcellus, Scipio, Fabius Maximus               |

They are covered by the same MIT licence as the rest of the repository and carry
no attribution obligation or usage restriction. Unlike the music above, they
place no limit on commercial distribution.

### Composed battle cues (`assets/audio/sfx/`)

Fifty-nine cues — the alerts, the mass-battle beds, the arrow and volley
layers, the unit stingers, the shield, guard, bow, movement and siege sets, and
`wolf_bite_snap` — were **generated by an earlier model and have been replaced
with CC0 material**. They were the second half
of a licence restriction that no longer applies to anything shipped; the music
that was the other half has since been replaced too.

They are composed rather than extracted: a Punic-war battlefield is not
something anyone holds a field recording of, so each cue is a stack of CC0
recordings summed together, offset, filtered and enveloped.
`tools/audio_field/battle.py` is the recipe — which recordings, which windows,
what shaping — and `tools/audio_field/build_battle.py` renders it.

| Cues                                                                                                                                                    | Built from                                                                                                                                                                                                                                              |
| ------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| the four `alerts/` horns, `roman_war_horns_orders`, the horn layer of `carthage_prepare_battle`                                                         | Wikimedia Commons, [`File:Hunting horn tone.ogg`](https://commons.wikimedia.org/wiki/File:Hunting_horn_tone.ogg) — one real brass note played at 0.70× into the cornu register, or 0.62× for Carthage so the two armies do not answer in the same voice |
| `battlefield_crowd_chaos`, `_distant_mass_01`/`_02`, `aftermath_battlefield`, `army_retreat_panic`, `soldiers_victory_cheer`, `carthage_prepare_battle` | CROWDS, `CRWDApls-…Crowd Applause, Cheering, Yelling, Whooping`, high-passed off the applause and lowpassed for distance                                                                                                                                |
| `army_march_dirt_mass`, `spearmen_formation_advance`                                                                                                    | FOOTSTEPS, `FEETHmn-…Running, Rocky Road` and `…On Grass`, summed against themselves at offsets sharing no common factor; MUSICAL, `MUSCPerc-…Drum, Snare, Military Marching Band`                                                                      |
| the eight arrow and volley cues                                                                                                                         | SWOOSHES, `SWSH-…Swishes, Big, Low`, `…Medium Low`, `…Stick, Small, Swishes, X4`, `…Cloth, Swoosh`, `WHSH-…Fly By, Short`                                                                                                                               |
| `roman_shield_wall_impact`, `gladius_shield_impacts_close`, `arrows_impact_shields_dirt`                                                                | FIGHT, `FGHTImpt-…Boxing Glove Hits`, `…Smacks, Rapid`, `FGHTBf-…Bodyfall, On Grass`; METAL, `METLImpt-…Metal, Clang, Thin 01` and `…Clang, Dull, Quiet`                                                                                                |
| `roman_cavalry_charge`, `numidian_cavalry_chase`, `horse_gallop_close_pass`                                                                             | Wikimedia Commons, [`File:Six Horses Galloping By.ogg`](https://commons.wikimedia.org/wiki/File:Six_Horses_Galloping_By.ogg) — three windows of one real pass: the approach, the moment level, and the far side (see below)                             |
| `elephant_charge_carthage`, `elephant_panic`                                                                                                            | ANIMALS, `ANMLWild-CU_Elephant Trumpet`                                                                                                                                                                                                                 |
| `wolf_bite_snap`                                                                                                                                        | FIGHT, `FGHTImpt-…Smacks, Rapid` for the wet impact, under ANIMALS, `ANMLDog-…Aggresive Dog Barks and Snarls, Distant Wind Chimes` for the animal on top of it                                                                                          |
| `wolf_snarl_bark`                                                                                                                                       | ANIMALS, `ANMLDog-…Aggresive Dog Barks and Snarls, Distant Wind Chimes`, one isolated bark — the same species reasoning the other wildlife cues use                                                                                                     |
| `low_resources_click`                                                                                                                                   | METAL, `METLImpt-…Metal, Clang, Dull, Quiet`                                                                                                                                                                                                            |
| `charge_roar`, `vanguard_rush`                                                                                                                          | CROWDS, the same applause recording high-passed off its claps, over FOOTSTEPS, `FEETHmn-…Running, Rocky Road` ranked at incommensurate offsets so a shout has a column behind it                                                                        |
| `siege_impact`, `siege_launch`                                                                                                                          | METAL, `METLImpt-…Bucket, Drop` for the mass; WOOD, `WOODBrk-…Branch, Snaps, Crackles` and `…Stick, Small, Breaks`; ROCKS, `ROCKCrsh-…Small Stones, Kicked`; WOOD, `WOODFric-…Ship, Creaking` and SWOOSHES, `SWSH-…Rope, Twirling` for the arm          |
| `shield_bash`, `shield_block`, `guard_break`, `guard_raise`, `perfect_guard`                                                                            | METAL, `METLImpt-…Metal, Clang 01` and `…Clang, Thin 01`; CLOTH, `CLOTHImpt-…Glove Slap` and `…Swish, Impact, Fight`; WOOD, `WOODBrk-…Stick, Small, Breaks` for the guard failing; FIGHT, `FGHTImpt-…Boxing Glove Hits`                                 |
| the five `bow_` cues                                                                                                                                    | WOOD, `WOODFric-…Floorboard, Creak` and `…Ship, Creaking` for a limb under load; SWOOSHES, `SWSH-…Stick, Small, Swishes` and `WHSH-…Fly By, Short`; METAL, `METLImpt-…Bolt, Drop` for the nock                                                          |
| `dodge_roll`, `jump_effort`, `land_thud`                                                                                                                | CLOTH, `CLOTHImpt-…Swish, Impact, Fight`; FIGHT, `FGHTBf-…Bodyfall, On Grass`; FOOTSTEPS, `FEETHmn-…Running, Rocky Road`                                                                                                                                |
| `second_wind`, `heal_bind_wound`, `lock_on_tick`, `ability_refused`                                                                                     | CLOTH, `CLOTHRip-…Baseball Mitt, Velcro, Slow` for linen being bound; METAL, `METLImpt-…Small, Tin, Drop` and `…Metal, Clang, Dull, Quiet`                                                                                                              |

Every source above except the horses is from **The Designer's Choice UCS
Collection**, original recordings by Nicholas A. Judy, released **CC0 1.0** and
explicitly cleared for commercial use — the same collection the sword hits and
footsteps already come from.

The horns come from **Wikimedia Commons,
[`File:Hunting horn tone.ogg`](https://commons.wikimedia.org/wiki/File:Hunting_horn_tone.ogg)**,
by Alon-De-Lon, released **CC0 1.0** — one real brass horn, first note only,
played at 0.70× so its 185 Hz fundamental lands at 130 Hz. The Designer's Choice
collection does hold a genuine war horn (HORNS, `HORNTrad-…CU_Viking War`) and it
was used here first, but it is a phone recording carrying 25–48 dB more energy
above 1.2 kHz than below 300 Hz, which reads as rattle rather than as a horn.
Nothing shipped draws on it any more — `carthage_prepare_battle` was the last
cue holding it and was rebuilt on the brass note too. A transposed didgeridoo was tried in between and rejected for the opposite
reason: it has the low end, but its buzz puts a bright band at 1.6–4 kHz that
reads as metallic. Measured on the shipped files the brass note runs 23–29 dB
_down_ in that band.

The three cavalry cues come from **Wikimedia Commons,
[`File:Six Horses Galloping By.ogg`](https://commons.wikimedia.org/wiki/File:Six_Horses_Galloping_By.ogg)**,
by the Freesound Community via Pixabay, released **CC0 1.0**. That collection has
no real horse in it: its `FOOTSTEPS/HORSE` folder is coconut shells and a
simulated wood floor, which is what these three cues were built from and why they
sounded like a pantomime rather than cavalry. The recording is one pass — six
horses closing, drawing level and running on — so the charge takes the approach,
the close pass takes the moment they are level, and the chase takes the far side,
with the real Doppler already in the material.

CC0 requires no attribution; all of it is recorded here because the provenance of
every shipped file should be answerable.

Two things were fixed in passing, because the replacements had to be measured
against the originals anyway:

- **28 of the 32 originals decoded over 0 dBFS**, by up to +2.33 dB. They were
  mastered to full scale and Vorbis is lossy, so they clipped on playback. Every
  rebuilt cue is ceilinged at −1.9 dBFS.
- **The alert cues were all exactly 10.0 seconds and the combat cues exactly
  5.0**, which was the generator's default length rather than a decision about
  how long a horn should sound. They are now as long as they need to be, from
  0.5 s for the resource click to 8 s for the aftermath bed.

Effect cues are deliberately not loudness-normalised at runtime — the level in
the file is the design decision (see `docs/AUDIO_MASTERING.md`). Each
replacement is therefore matched to the RMS of the file it replaces, so the mix
is unchanged.

Every track under `assets/audio/sfx/` now carries a `source` tag in the
manifest — `synth` for the 106 generated cues, `field` for the 69 cut or
composed from recordings — so "where did this sound come from" is answerable
without reading this file.

### Where the audio actually ships

`assets/audio/music`, `assets/audio/voices` and part of `assets/audio/sfx` are
redistributed inside the binary via `assets.qrc`. The ambience beds are not
embedded, but they are installed alongside the binary and redistributed just the
same.

## Bundled Libraries

Two single-header libraries are vendored under `third_party/` and compiled
directly into the game binary, so their terms travel with every release
package.

| Library                                           | Version               | Licence                                                | Used for                       |
| ------------------------------------------------- | --------------------- | ------------------------------------------------------ | ------------------------------ |
| [miniaudio](https://github.com/mackron/miniaudio) | v0.11.23 (2025-09-11) | Public domain (Unlicense) **or** MIT-0, at your option | audio device output and mixing |
| [stb_vorbis](https://github.com/nothings/stb)     | v1.22                 | Public domain (Unlicense) **or** MIT, at your option   | Ogg Vorbis decoding            |

**This project takes the public-domain option for both.** Neither library then
requires attribution or a licence notice in the binary. They are recorded here
for the same reason as everything else in this file: what ships should be
answerable. If a downstream redistributor prefers the MIT option instead, the
full licence text sits at the bottom of each vendored header.

Neither library is modified. Both are permissive enough to impose no
restriction on commercial use — the only such restriction in this project comes
from the music above.

## Bundled Fonts

Typography ships in `assets/fonts/` rather than being requested from the host,
so that the game and its promotional captures letter identically on every
machine. Files there are redistributed inside the game package.

| Font                  | Licence | Copyright                                                       | Used for                                     |
| --------------------- | ------- | --------------------------------------------------------------- | -------------------------------------------- |
| EB Garamond 12 Bold   | OFL-1.1 | 2010–2012 Georg Duffner, http://www.georgduffner.at/ebgaramond/ | body serif, caption fallback, glyph coverage |
| Standard Iron Display | OFL-1.1 | 2026 Standard of Iron contributors                              | titles, headings, big numbers, reel captions |

The OFL-1.1 text for EB Garamond travels with the file as
`assets/fonts/OFL-EBGaramond.txt`, as that licence requires. Standard Iron
Display is original to this project and released under the same licence so it
can be redistributed on the same terms as everything it sits beside; its text
is at `assets/fonts/OFL-StandardIronDisplay.txt`.

Neither font restricts commercial use.
