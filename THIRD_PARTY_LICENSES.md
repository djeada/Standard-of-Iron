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

Added because the existing `combat/troops_death_scream.ogg` does not read as a
human being killed: its dominant partial sits at 810-880 Hz and **holds there
for the whole cue** rather than falling and breaking up, which is horn
behaviour, and listeners heard it as a trumpet. That asset is still what the
game plays for unit deaths and has the same problem.

### Music, ambience, voices and the remaining effects

**Provenance not yet recorded.** The music, ambience and voice tracks, and the
combat and alert recordings that predate the synthesised set, are not covered
by this file. Whoever sourced them should record here, per asset or per batch:
the origin, the licence, and whether commercial use and redistribution are
granted. If any of it came from a generative audio service, note the service
and the plan the output was produced under, since commercial rights usually
depend on the tier.

This matters before shipping: `assets/audio/music`, `assets/audio/ambience`,
`assets/audio/voices` and part of `assets/audio/sfx` are redistributed inside
the binary via `assets.qrc`.
