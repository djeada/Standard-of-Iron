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

## Audio Assets

### Synthesised cue sounds

The 60 sound effects listed with `"source": "synth"` in
`assets/audio/audio_manifest.json` are generated from the recipes in
`tools/audio_synth/` by `make audio-assets`. They are original work produced by
this repository's own code: nothing is sampled, and no third-party recording or
library is involved. No attribution or licence obligation attaches to them.

Covers the `ui.*`, `order.*`, `state.*` families in full, plus the build,
alert and commander-combat cues. See `tools/audio_synth/README.md`.

### Wildlife effects (`assets/audio/sfx/wildlife/`)

All five are cut from recordings of real animals. Synthesised versions were
tried first and abandoned: measured against this repository's own
`combat/elephant_charge_carthage.ogg`, the generated growl scored **0.994
cosine similarity on its band profile** — it was, spectrally, an elephant, and
listeners heard exactly that. Real canid growls sit far lower (about 60% of
their energy below 160 Hz) and score 0.71-0.87 against the same reference.

| File                    | Origin                                                                                                                  | Licence       |
| ----------------------- | ----------------------------------------------------------------------------------------------------------------------- | ------------- |
| `wolf_howl_distant.ogg` | Wikimedia Commons, [`File:Wolf howls.ogg`](https://commons.wikimedia.org/wiki/File:Wolf_howls.ogg), seconds 13.35-17.55 | Public domain |
| `wolf_howl_near.ogg`    | the same recording, seconds 20.05-23.65                                                                                 | Public domain |
| `wolf_growl_low.ogg`    | PLOS ONE study audio S3, large-dog growl (see below)                                                                    | CC BY 2.5     |
| `wolf_snarl_bark.ogg`   | the same recording, a 0.56 s burst                                                                                      | CC BY 2.5     |
| `wolf_pack_attack.ogg`  | the same recording layered with study audio S2, small-dog growl                                                         | CC BY 2.5     |

**Required attribution** for the three growl-derived cues:

> Growl recordings from Faragó T, Pongrácz P, Miklósi Á, Huber L, Virányi Z,
> Range F (2010), "Dogs' Expectation about Signalers' Body Size by Virtue of
> Their Growls", PLOS ONE, DOI 10.1371/journal.pone.0015175, supporting audio
> S2 and S3, used under [CC BY 2.5](https://creativecommons.org/licenses/by/2.5).
> Pitched down, trimmed, layered and loudness-normalised for this project.

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

Eleven of the nineteen looping beds are cut from field recordings; the other
eight are generated by `tools/audio_synth` and carry no obligation. The
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

### Music, voices and the remaining effects

**Provenance not yet recorded.** The music and voice tracks, and the combat and
alert recordings that predate the synthesised set, are not covered by this file.
Whoever sourced them should record here, per asset or per batch: the origin, the
licence, and whether commercial use and redistribution are granted. If any of it
came from a generative audio service, note the service and the plan the output
was produced under, since commercial rights usually depend on the tier.

This matters before shipping: `assets/audio/music`, `assets/audio/voices` and
part of `assets/audio/sfx` are redistributed inside the binary via `assets.qrc`.
The ambience beds are not embedded, but they are installed alongside the binary
and redistributed just the same.
