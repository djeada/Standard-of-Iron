# The cinematic trailer

`standard_of_iron_trailer.mp4` is built entirely from Standard of Iron's own
renderer, simulation and audio. Nothing in it is offline CGI: every image is a
deterministic arena scenario photographed by an authored camera, and every
sound is either the game's own library or synthesis in `scripts/trailer/dsp.py`.

## What was wrong with the previous cut

The September 2026 `trailer_v2.json` cut showed the right content and is the
basis of this one (the pitched battle, the commander, the town, the Iron
Sepulcher, the title), but it presented it as a feature list:

- a caption on nearly every shot (MASSIVE BATTLE, COMMAND IT., FIGHT IN IT.,
  BE THE COMMANDER, RAISE A CITY, PUT IT TO WORK, ...);
- 720p, flat midday light, washed-out haze, no grade;
- the RTS camera's angle for most shots, and HUD-heavy overhead footage;
- order markers and damage numbers inside supposedly cinematic shots;
- cameras that orbit and zoom because they can, eased to a stop on every key;
- music laid under the picture rather than driving the cut, and sound effects
  placed on every cut at one level;
- a stock-looking flame card for the title.

## Direction

One day of war. Dawn over a field where two armies wait; the morning battle;
the capital at peace; the same capital burning at dusk; a winter night where
the dead rise; and a final escalation that ends on the name.

- **Picture.** 1920x1080 at 24 fps, extracted 2.39:1. Arena renders 2x
  supersampled at 96 fps; the conform integrates three of every four
  sub-frames (a 270-degree shutter) for real motion blur. Long lenses do most of
  the work: the art style reads as toy-like in wide close-ups, and as an army
  when compressed. Units are about a metre tall, so "eye level" is 0.6-1.0 m.
- **Light.** Each shot sets its own sun and haze (`lighting` in the capture
  specs). The sun stays above 18 degrees on battle shots so shadows stay under
  the soldiers. Fog colour is matched to the sky so haze reads as air.
- **Grade.** Per sequence, in `cut.json` `looks`: `dawn` (warm haze, lifted
  blacks), `battle` (desaturated, firmer), `city` (clean warm morning), `dusk`
  (orange highlights, deep shadows) and `night` (half saturation, cool shadows,
  fire keeps its warmth). Halation, vignette and grain are part of each look.
- **Typography.** One card: the title, in the game's display face.
- **Narration.** None. The game has no recorded narrator, no machine voice here
  would meet the bar, and the images carry the story.
- **Sound.** Music is cut from the game's own score (ElevenLabs, commercial
  licence; see `THIRD_PARTY_LICENSES.md`) and drives the edit: the main theme
  under the dawn, *Dust of Cannae*'s percussion entrance on the first impact,
  *Sunlight on the Olive Groves* for the city, *Siege at Dawn*'s heartbeat as it
  burns, *Skeletons Awaken* with its hit on the rising, and Cannae's crest on the
  cut to the title. Effects are placed with perspective (pan, air absorption,
  reverb); the arena's own recorded mix sits under each battle shot; the trailer
  hits, sub drops, risers and swells are synthesised.

## Accuracy

Everything shown exists in the game: Rome and Carthage's troop types and
commanders (Scipio, Hannibal), formations, cavalry charges, Carthage's war
elephants, catapults firing flaming stones that set structures alight, Aurelia
Magna's builders and citizens, snow, and the Iron Sepulcher's skeletons and
grave priests. Rome fields no elephants in any shot; there are no rams or siege
towers, no storms and no voiced dialogue.

## Reproducing it

```sh
# 1. Build arena_app, then capture every film set on the real GPU (:0).
scripts/trailer/build.sh capture       # ~2.5 h on an RTX 5060
# 2. Conform, mix and deliver.
scripts/trailer/build.sh picture
scripts/trailer/build.sh sound
scripts/trailer/build.sh master
```

`PYTHON` must have the packages in `scripts/trailer/requirements.txt`.

| Stage | Source |
| --- | --- |
| Film sets | `tools/arena/arena_cinematic_scenarios.cpp` (`cine_field`, `cine_siege`, `cine_sepulcher`) and `promo_imperial_capital` |
| Camera and light | `tools/arena/promos/cinematic/capture_*.json` |
| Edit, looks, sound | `tools/arena/promos/cinematic/cut.json` |
| Picture | `scripts/trailer/conform.py`, `scripts/trailer/titles.py` |
| Sound | `scripts/trailer/mix.py`, `scripts/trailer/dsp.py` |
| Delivery and QC | `scripts/trailer/deliver.py` |

`deliver.py` refuses a file whose picture and sound lengths disagree, whose
loudness drifted in the AAC encode, whose true peak passes -1 dBTP, or which has
stray black or frozen frames or a flash series.
