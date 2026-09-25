# Store page draft

A working draft for the Steamworks store page (App 5129960). Every claim below
is checked against the repository as of the Steam-readiness branch. Anything
marked **confirm** needs the author's answer before submission.

## Short description (≤ 300 characters)

> Command thousands of soldiers in the Second Punic War. Grow a colony into a
> city, march Rome's legions or Carthage's host against each other, take the
> field yourself as a commander in close combat, and face the Iron Sepulcher,
> the dead that rise from cursed ground.

## About this game

**Rome and Carthage at the edge of empire.**

Standard of Iron is a real-time strategy game about scale. Two hosts of
thousands meet across one field, and every soldier in every cohort is on the
ground: shields locked, spears levelled, horse wheeling round the flank.

**Command the line, or be in it.** Order cohorts, formations and cavalry wings
from above. Then drop into your commander, whether Scipio, Hannibal or four
others, and fight in person. Chain strikes, heavy blows and specials. Rush the
enemy general, rally your men, and turn a wavering line with your aura.

**Build the place worth defending.** Found a colony, set builders to timber,
stone and iron, and grow homes, markets, farms, temples, barracks and towers
into a working city. Its people fill the streets by day and light the torches
at night.

**The Second Punic War.** An eight-mission campaign follows the war from the
Rhône crossing and the Alps to Trebia, Trasimene, Cannae and Zama. Standalone
missions and skirmish battles fill the space around it.

**The dead do not rest.** In the Iron Sepulcher's ground, skeleton warbands and
fire-casting grave priests wake when an army walks too close, in wave after
wave, through snow and river mist.

## Feature list (shipping only)

- Battles with thousands of individually rendered soldiers
- Direct-control commander combat: combos, specials, lock-on, rally and aura
- Six historical commanders across Rome and Carthage
- City building and a full economy: gathering, construction and recruitment
- An eight-mission Second Punic War campaign, plus standalone missions and
  skirmish
- The Iron Sepulcher: an undead faction that rises from cursed zones
- Weather and light: snow, rain, river mist, and a day/night cycle
- Interface in eight languages

Do **not** add: multiplayer, controller support, achievements, Steam Workshop,
or mod support. None of them ship.

## Languages

Interface in English, German, Spanish, Polish, Portuguese (Brazil), Russian,
Turkish and Arabic. Every one of the eight translation files is 100% complete
(3,118 strings each).

Full audio: **confirm**. The voice lines are the author's own recordings; tick
"full audio" only for the language they are in. Subtitles: **confirm** that
voice lines are subtitled before ticking the subtitles column.

## System requirements

Measured or read from the build; see the notes after the table.

|                    | Windows                                    | Linux / SteamOS                                                                |
| ------------------ | ------------------------------------------ | ------------------------------------------------------------------------------ |
| OS                 | Windows 10 or 11, 64-bit                   | glibc 2.35 or newer, 64-bit (Ubuntu 22.04+, Debian 12+, Fedora 36+, SteamOS 3) |
| Graphics           | OpenGL 3.3 Core; OpenGL 4.5 recommended    | same                                                                           |
| Storage            | 1 GB available space                       | 1 GB available space                                                           |
| Processor / memory | **confirm** after measuring a large battle | same                                                                           |

- **Storage:** the unpacked v0.1.0 depots are 357 MB (Windows) and 321 MB
  (Linux).
- **Graphics:** OpenGL 3.3 Core is the renderer's floor on every platform.
  Drivers with 4.3+ (compute, SSBO, indirect draw) get the faster crowd path
  automatically. Windows also ships a Mesa llvmpipe software renderer as a
  fallback.
- **Linux:** the Linux binary needs glibc 2.35 and GCC 12's libstdc++
  (`GLIBCXX_3.4.30`), and the depot does not bundle them. Older distributions
  cannot start it. It runs under Steam Linux Runtime 3.0 (sniper); the v0.1.0
  depot passed the full release self-test there.
- **macOS:** not offered until the build is Developer-ID-signed and notarized
  (steam/README.md).

## Tags (suggested)

Real-Time Strategy, Strategy, Historical, City Builder, Medieval/Ancient
(Rome), Action RPG elements, Large-Scale Battles, Singleplayer, Dark Fantasy.

## Content Survey notes

- Violence: stylised battlefield combat, with blood effects on hits and
  corpses that remain on the field. **confirm** the exact wording against the
  survey's categories.
- **Pre-generated AI content: audio.** 30 music tracks and 44 sound effects
  (ElevenLabs), disclosed in steam/README.md.
- **Pre-generated AI content: art.** The three menu and load-screen paintings,
  `assets/visuals/load_screen_*.png`, are AI-generated (ChatGPT image
  generation) and must be disclosed. **confirm** the provenance of the
  emblems, the `standard_of_iron.png` banner, and the commander portraits.
- No content is generated at runtime, and the game makes no network requests.

## Steam Deck

Not claimed yet. At 1280×800 the menu and HUD render, but:

- the main menu list scrolls, so the last entry sits below the fold;
- the HUD order buttons truncate their labels ("At…", "G…", "Pa…");
- the camera help card overlaps the minimap;
- there is no controller path, which Deck Verified requires.
