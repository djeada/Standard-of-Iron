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

The fight arrives fast. A horn and one hit over the elephants on the ridge, the
two commanders, the Numidians riding past, and at nine seconds the elephants
drive into the Roman line. From there: scale and command, the commander in the
melee, the city being built, the same kind of city set alight, the Iron
Sepulcher's dead rising in the snow, an accelerating montage, and the flame card.

- **Picture.** 1920x1080 at 24 fps, extracted 2.39:1 (the end card opens to the
  full frame). Arena renders 2x supersampled at 96 fps; the conform integrates
  three of every four sub-frames for real motion blur. Long lenses carry the
  close action; units are about a metre tall, so "eye level" is 0.6-1.0 m.
- **Light.** The game's own lighting profiles (a warm Mediterranean afternoon,
  dusk over Aurelia Magna, the Iron Sepulcher's night), not trailer haze: an
  A/B against the game's own screenshots showed overrides turning it grey.
- **Grade.** Enhances rather than drains the game's colour: `sun`, `city`,
  `dusk`, `night` and `fire` looks in `cut.json`, each with a gentle S-curve,
  split toning, halation, vignette and grain.
- **Overlays.** `scripts/trailer/fx.py` renders embers, backlit dust and warm
  light-leak plates, screened over the footage per shot; impacts get a decaying
  shake and a short exposure flash (`hits`).
- **Typography.** Six short captions that say what the game is (Rome against
  Carthage, command armies of thousands, fight at the front as your commander,
  build the city, set cities ablaze, the Iron Sepulcher), each with a text
  stinger in the mix; the title and call to action (free and open source, the
  GitHub address, Windows / macOS / Linux) over the arena's flame card.
- **Narration.** None: the captions carry the information.
- **Sound.** The game's own score cut to drive the edit: _Last Defensive Wall_'s
  drum ostinato under the open, _Dust of Cannae_'s percussion on the first
  impact, the main theme's full statement for the city and the burning,
  _Skeletons Awaken_'s hit on the rising, Cannae's crest into the flame card and
  the main theme's final hits under the title. Effects are placed with
  perspective; the arena's recorded mix sits under each shot; hits, risers and
  swells are synthesised.

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
scripts/trailer/build.sh capture       # ~2 h on an RTX 5060
# 2. Conform, mix and deliver.
scripts/trailer/build.sh picture
scripts/trailer/build.sh sound
scripts/trailer/build.sh master
```

`PYTHON` must have the packages in `scripts/trailer/requirements.txt`.

| Stage              | Source                                                                                                                  |
| ------------------ | ----------------------------------------------------------------------------------------------------------------------- |
| Film sets          | `tools/arena/arena_cinematic_scenarios.cpp` (`cine_field`, `cine_siege`, `cine_sepulcher`) and `promo_imperial_capital` |
| Camera and light   | `tools/arena/promos/cinematic/capture_*.json`                                                                           |
| Edit, looks, sound | `tools/arena/promos/cinematic/cut.json`                                                                                 |
| Picture            | `scripts/trailer/conform.py`, `scripts/trailer/titles.py`                                                               |
| Sound              | `scripts/trailer/mix.py`, `scripts/trailer/dsp.py`                                                                      |
| Delivery and QC    | `scripts/trailer/deliver.py`                                                                                            |

`deliver.py` refuses a file whose picture and sound lengths disagree, whose
loudness drifted in the AAC encode, whose true peak passes -1 dBTP, or which has
stray black or frozen frames or a flash series.
