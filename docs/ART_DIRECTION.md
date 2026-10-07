# Art direction: sunlit fresco

Standard of Iron should look like a Pompeian wall painting come to life: a
warm, painted Mediterranean world in low sun. The stylised, toy-like
(Playmobil) people stay. The magic comes from light, not from saturated
effects. This page is the reference for any change to colour, lighting,
scatter or set dressing. Check a change against these rules and against the
look survey (see the end of this page) before it lands.

## The six rules

1. **One palette from the period.** Ochre, terracotta, olive, limestone,
   bronze, madder red, Tyrian purple, lapis. No pinks, no sky blues, no
   neon.
2. **Team colour is a dye, not a body paint.** It lives in cloth, crests,
   cloaks and banners at a deep, wool-like value. Skin, metal and wood never
   take the team tint.
3. **Light is the magic.** The RTS camera almost never sees the sky, so the
   ground carries the mood: low sun, long shadows, drifting cloud shadows,
   pools of firelight.
4. **True scale.** Everything is sized against a soldier: about 1.08 world
   units tall, with 1 unit ≈ 1.67 m.
5. **Places, not scatter.** Ruins, statues, campfires, carts and tents belong
   to a camp, a road, a settlement or a ruin field. A lone prop in an open
   field is a bug.
6. **Old, dark and infernal magic.** The forbidden cults burn: black basalt,
   obsidian and bronze split by crimson-ember fissures that glow by day and
   blaze at night. Ember gold stays with gods and treasure, ghost silver with
   the dead. A deep violet-black is allowed only as a faint halo in the
   shadow of a fissure. The glow belongs to the object (cracks, a hearth, a
   pool of firelight on the ground), never to floating motes, and it never
   turns electric violet or neon.

## Palette

| Name         | sRGB      | Use                                |
| ------------ | --------- | ---------------------------------- |
| Ochre        | `#C9A24A` | dry grass, Punic plaster accents   |
| Terracotta   | `#B8583A` | roof tiles, pottery                |
| Olive        | `#7D8A4A` | foliage, Mediterranean grass       |
| Limestone    | `#E6DCC4` | plaster, marble, statues           |
| Bronze       | `#A8783C` | helmets, fittings, selection rings |
| Madder       | `#9A2B23` | Roman dye                          |
| Tyrian       | `#5B2A5E` | Carthaginian dye                   |
| Lapis        | `#2F4F7A` | accents, Punic doors               |
| Ghost silver | `#CFD8CF` | the dead, cult glyphs at night     |
| Ember gold   | `#F0B552` | altars, gold veins, firelight      |
| Hellfire     | `#E0461A` | shrine fissures, cult hearths      |
| Blood iron   | `#B0121A` | magical iron veins and crystals    |
| Basalt       | `#2A2629` | shrines, cult monoliths            |

## Biomes stay distinct

One grading pipeline, three families of maps. The grade must never flatten
these into one look.

| Family               | Maps                                        | Ground                              | Trees                                | Light                                |
| -------------------- | ------------------------------------------- | ----------------------------------- | ------------------------------------ | ------------------------------------ |
| Dry Mediterranean    | Cannae, Trasimene, Rhône, Zama, Sallow Ford | gold, sun-bleached grass, warm soil | olive, cypress; palms only in Africa | `mediterranean_summer`, `canyon_dry` |
| Mountain             | Crossing the Alps, Victumulae, Trebia       | snow, cold rock, dark soil          | pine                                 | `alpine_clear`, `river_mist`         |
| Lush European fields | Ticino, Campania, Aurelia Magna, Pinewater  | deep green grass, dark fertile soil | pine, cypress; olive in the south    | `delta_haze`, `mediterranean_summer` |

Locked maps prefer a low sun: 07:00 to 09:30, or 16:00 to 18:00.

## Light and atmosphere

- **Haze is distance.** Fog and the sky's horizon share one colour,
  `environment_horizon_haze()`: the lighting profile's fog colour blended 42%
  toward the sky. Ground fog is a light veil (`k_ground_fog_max_opacity`
  0.24), not a cream wash.
- **Cloud shadows** drift over everything that calls
  `apply_directional_shadow()`: terrain, grass, trees, buildings and
  characters. Strength comes from the profile's `cloud_cover`, at about 24% on
  a clear day.
- **The map edge is a horizon.** Boundary mountains (`horizon_dressing`) fade
  into the same haze with distance. Below the horizon, the sky shades into
  distant land instead of a flat grey band.
- **The grade's shadows stay warm.** The split-tone shadow colour is only
  slightly cool, so plaster and sand keep their warmth.

## People and creatures

- Team cloth is clamped in OKLCH to lightness 0.40–0.52 and chroma at most
  0.17 (`render/palette.cpp`). That turns pure red into madder and blue into
  lapis.
- An elephant's hide is a dusty warm grey and gets real fill on its shadow
  side, so it never turns into a black silhouette.
- Killed soldiers that are thrown tilt back during flight and land in the
  death clip's pose. They do not cartwheel.

## Towns

- Each building instance hashes its world origin into a plaster tint (white,
  ochre, rose, pale yellow or warm grey) and a roof-tile value. The mesh is
  shared and the variety comes free.
- At night, dark cedar doors and shutters on about three houses in four spill
  warm lamplight. This costs no local-light slots.
- Carthaginian plaster is whitewash, not grey stone.
- Aurelia Magna's lawns carry about 350 garden cypresses and olives, grouped
  in pockets of roughly 14 units and kept clear of every building, wall, road and
  spawn (planted from the existing map with a one-off clearance pass).

## Campfires

- A campfire is a hearth about 1.7 m across (`FireCampRenderer::hearth_radius`,
  0.5 units times the prop's `scale`): ten bedded river stones, five logs
  leaning into a tepee with charred inner ends, and a small woodpile outside
  the ring. The flame is a little shorter than a soldier, never a bonfire.
- Every stone, log foot and woodpile end samples the ground under itself and
  stones tilt to the slope. Placing the whole hearth at the lowest point of the
  prop's footprint buried the uphill side, and on steep campaign slopes the
  entire fire, so nothing may be laid out flat from one height.
- The flame, the ash-and-ember bed and a faint smoke wisp are one instanced
  draw. The bed is a disc fitted to the ground plane under the hearth (slope
  and bulge travel in the instance), dark ash with glowing cracks so the fire
  reads at RTS zoom by day as well as by night.
- Firelight pools at full strength only at night; by day it is cut to about a
  third so a camp does not paint a yellow halo on sunlit grass. Terrain chunks
  must carry real bounds: the per-chunk local-light mask is built from them, and
  with empty bounds only lights near the world origin ever reached the ground.

## The dark world inside the ancient one

- **Shrines** are tophet altars of the burning god. A horned black-basalt
  altar holds a smouldering hearth, and an obsidian baetyl rises out of the
  fire, crowned with bronze bull horns. Four leaning basalt fangs and a ring
  of jagged teeth surround it, with small stelae at its foot. Molten
  crimson-ember fissures run through the stone. They are hottest at the base,
  and on the platform they radiate from the altar, so from above the shrine
  reads as a burning star. The altar faces carry a glowing sign of Tanit and
  the fangs carry dim Punic letters. The stone wears dried blood-ochre stains
  and ash. Everything beats with a slow heartbeat and stays visible by day.
  At night it kindles brighter, and a flickering ember light pools on the
  ground around it.
- **Magical iron** is black meteoric ore, a "blood iron" kept distinct from
  the shrine's orange fire and the gold vein's amber. Jagged black-glass
  crystals erupt from etched, steel-sheened rock. Blood-crimson veins pulse
  through the rock and up the crystal roots, and a dark crimson light glows
  on the ground around it.
- The cult objects burn and the dead stay cold. Their ghost-silver miasma is
  the eerie counterweight to the hellfire, so the two never share a colour.
- **The dead** wear aged ivory bone, faded burial linen and verdigris bronze.
  Their miasma and the sepulcher's shadows are ghost silver, not violet.
- **Ruins** are sun-bleached cut stone, not black slabs.

## Look survey

`scripts/look-survey.sh <out>` captures 60 arena shots from the curated promo
cameras (`tools/arena/promos/look_survey.json`) plus mission-start frames from
the real game. `scripts/look-survey-compare.py <before> <after> <out>` pairs
two runs side by side and reports luma and saturation per frame. Run both for
any change that alters the look. Also take the night-luma measurement on
`lighting_moonlit_night` that [RENDERING_ARCHITECTURE.md](RENDERING_ARCHITECTURE.md)
asks for.
