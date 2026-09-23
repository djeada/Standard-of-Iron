# Residential ambient activity

Roman and Carthaginian houses look inhabited: a hearth vents smoke through an
opening the model actually has, shutters stand open by day and close at dusk,
washing hangs out, cloth stirs in the breeze, lamps come on at night, residents
step outside, and very occasionally one carries a bowl of soup out of the door
and wears it. Everything here is presentation-only. No population, selection,
collision, pathfinding, vision, resource output or replay state is added, and
nothing reads the live simulation while drawing.

## Nothing in lockstep

The headline rule: in a town of a hundred houses, no two may look like copies
and none may change state together. Every periodic thing here derives its
offset, period, duration and magnitude from `ambient_hash` of the entity id
(`roll(seed, salt)` in `home_activity.cpp` hands out an independent unit float
per knob), never from raw `animation_time` alone. Slow global changes -- dusk
arriving -- act through per-house thresholds and per-house phases, so a
street responds one house at a time. `tests/render/home_activity_test.cpp`
proves this for every schedule by sweeping the night factor and counting how
many houses flip per step.

## No chimneys

An ancient Mediterranean house has no chimney, so neither does this. Each
nation registers a `HomeSmokeAnchor` from its own house renderer, pointing at
an outlet that building already models:

| Nation   | Outlet                                                                  | Local anchor          |
| -------- | ----------------------------------------------------------------------- | --------------------- |
| Rome     | smoke works out through the roof tiles, above the ridge toward the rear | `(0.00, 1.66, -0.42)` |
| Carthage | the rooftop bread oven, offset to one corner of the flat roof           | `(0.48, 1.70, -0.46)` |

The anchors are in house-local units and are carried through the building's own
model matrix, so a plume scales and rotates with the house rather than floating
at a fixed world height. The Punic anchor is deliberately off-centre: the oven
is a corner structure, and a centred plume would read as a chimney.

The same anchor carries the rest of the house's dressing: the doorstep and its
outward direction, the hanging cloth, the lamp, the side-wall windows for the
shutters, and where a washing line can be strung.

## The plume

`hearth_smoke` is its own effect kind, not a recoloured dust cloud or flame.
`CombatDust` is a ground-hugging dome and `BuildingFlame` adds ember sparks
unconditionally; neither can pass for domestic smoke. The branch
(`u_effect_type == 7` in `combat_dust.vert`/`.frag`) uses twenty overlapping,
camera-facing wisps in one draw per hearth. Each wisp rises from the outlet,
expands slowly and drifts downwind, with coherent noise breaking up its soft
edges. Both ends of its lifetime fade to zero before it is recycled. There is
no enclosing cone or tube, and opacity follows the fire intensity all the way
to zero. The house-local plume scale is 0.68 for Rome and 0.60 for Carthage,
keeping cooking smoke modest above the roof.

The shader is unlit, so the caller dims and thins smoke at night. Wisps use
ordinary alpha blending with depth testing and no depth writes; they carry no
sparks or glow. Their mesh is separate from combat dust and building flames.

### Every hearth is a different fire

`hearth_character(id)` hashes size (0.8-1.25x), density (0.62-1.0x), colour
temperature, rise-clock speed and a large clock offset. Tended fires breathe
on a slow per-house pulse. The shader also hashes the vent position for rise
speed and wind drift, and varies each new wisp while it is invisible at the
lifetime boundary. Neighbouring hearths share the prevailing wind direction
but never emit identical wisps together. The noise clock is the house's own
(`time * speed + offset`), never raw animation time.

It emits no local light. Only flames and lamps feed the light buffer, and a
cooking fire seen through a roof should not illuminate the street.

## Scheduling

Each house is lit for a stretch and then goes out. Its offset, period, duty
cycle and strength all come from a hash of its entity id, so neighbours never
share a cycle and a street never animates in lockstep. Roughly a third of
eligible houses are alight at midday. The intensity ramps in and out over
about nine seconds at each end, so a hearth is banked up and dies down rather
than switching.

### Meal-times

`HomeActivity::meal_bias()` stretches every house's lit window by the light:
0.8x at full day, 1.35x in twilight (the evening meal cooked as the light goes,
the morning one as it returns), 0.64x at full dark when most fires are banked.
The bias moves slowly with the night factor and each house crosses its own
window edge at its own phase, so dusk lights hearths one by one; the test
sweeps the night factor in a hundred steps and allows at most three of sixty
four houses to change per step.

Zero-health, ruined, neutral, previewed, pending-removal, dying and
dismantling houses show nothing. Eligibility is resolved for every house in one
scan of the locked render snapshot at the top of the frame; submission is then
a binary search.

A house carries a `UnitComponent`, so the render snapshot files it with the
units and it is drawn from `Renderer::plan_unit_entry`, not from
`Renderer::submit_non_unit_entry`. That is where the per-frame `HomeActivity`
is attached to the draw context.

## Night

Night is not a clock. `mediterranean_summer` still reports a primary intensity
of 0.44 at 21:00 and only the colour goes dim, so `render/scene_walk.cpp`
measures the light the scene actually casts (`luma(primary_color) *
primary_intensity`) and feeds `HomeActivity::set_night` a 0-1 factor. That one
factor drives the lamps, the shutters, the laundry and the meal-time bias.

### Lamps

`lamp_state(id, time)` is the oil lamp inside the door. About a fifth of
households are simply dark. The rest each light up at their own point in the
dusk (threshold 0.10-0.52 of the night factor) and ease in over their own
stretch of it, so a street comes alight window by window. Each lamp also has a
long schedule of its own (240-460 s, ~70-90% lit) on which it goes out for a
while — someone went to bed — and its own flicker: a quick wobble under a slow
one, 1.6-3.8 Hz at a per-house phase and depth. Colour runs from amber to a
deeper orange per house, always candle-warm. What is drawn is a dim pool on the
threshold (alpha 0.42 at most) and a small local light (radius 3.8, intensity
0.72 at most): restraint sells it, because the effect shader is unlit and a
bright glow reads as neon.

## Dressing

All of this is skipped below fourteen projected pixels.

### Cloth

A run of five strips, each in two links: the upper hangs from the top edge, the
tail hangs from the upper's hem and lags it with a larger swing, so the free
edge moves further than the hung edge and the strip bends rather than tilting.
The gust arrives at each strip a little after the one before it, so it passes
along the run instead of lifting every strip together; per house the breeze
strength, gust rate, flutter rate and phase all differ. A Punic house hangs an
indigo valance under its roof awning; a domus curtains its door, and that
curtain (`cloth_at_door`) is pushed aside from the middle out as each resident
goes out and again as they come in (`curtain_parting`).

### Shutters

Both models cut windows in their side walls; each now has two leaves hinged at
its jambs. `shutter_angle(id, window)` settles each of the four windows as
closed (22%), ajar (30%) or thrown open flat against the wall (48%), so the
same street is never four identical facades, and closes them for the night,
each window at its own moment in the dusk.

### Washing

Some houses (36% Roman, 48% Punic) have washing out. A domus strings a short
line between two of its portico columns; a Punic house runs one across its flat
roof from the awning post to a pole by the far parapet. One to three pieces in
undyed linen, ochre, indigo or madder, each its own width and height, sway on
the line and are brought in one at a time as dusk comes.

## Residents

`doorstep_plan(id, time)` is the ordinary ambient, far more common than the
gag. Each house keeps its own rhythm (69-136 s) and within it each turn outside
is decided afresh from a hash of house and cycle: whether anyone comes out at
all (58% of turns), for how long (12-34 s), what they do, and whether a second
resident joins them. Activities:

| Activity | What is drawn                                                     |
| -------- | ----------------------------------------------------------------- |
| Stand    | shifts their weight, looks about, turned a little one way         |
| Sweep    | works at something in front of the door                           |
| Scrub    | kneels at the threshold, facing the house                         |
| Sit      | sits on the step (sits down, holds the pose, stands back up)      |
| Errand   | walks along the front of the house, pauses at the end, walks back |
| Talk     | two residents, a stride apart, facing each other                  |

Everyone walks out through the door and back in through it; the walk phase
follows distance so nobody skates, and the walk path bends from the door to
where they settle (their own spot across the door, 1.1-2.0 m out). Off the
plinth they stand on the terrain. Residents are rigged actors and share the
per-frame actor budget; a pair costs two.

Residents turn over 0.6 s instead of snapping (on arrival, before the walk
back, and at each end of an errand) and fade walk and activity clips into each
other over 0.35 s. The errand's walk phase counts the whole distance covered,
so the return leg steps forward instead of replaying the outward stride
backwards, and a resident standing idle breathes on a 7-9 s cycle, close to the
baked 8 s idle.

## Budget

| Quality | Plumes per world render | Rigged actors |
| ------- | ----------------------: | :------------ |
| Low     |                       0 | no            |
| Medium  |                       8 | no            |
| High    |                      20 | 6 per frame   |
| Ultra   |                      32 | 10 per frame  |

Reduced effects means no rigged actor at all, not merely a cheaper one. Houses
below nine projected pixels or beyond 190 world units are skipped (plumes thin
out over the last quarter of that range rather than popping), and residents
need twenty-two pixels before they will play. Earlier visible houses consume
the budget. Fog is checked on current visibility, never on merely explored
ground, so neither smoke nor lamplight can betray an unseen residence.

## Soup spill

One renderer-wide slot permits at most one sequence, and it reuses the nation's
civilian rig with a clay bowl hung off it as ordinary humanoid equipment. Beats
are constants in `Render::GL::Spill`, in seconds from the moment the slot is
claimed:

| Window  | What happens                                                         |
| ------- | -------------------------------------------------------------------- |
| 0.0-1.6 | walks out of the doorway, bowl in hand                               |
| 1.6-2.3 | staggers — the trip, bowl still held                                 |
| 2.3-4.2 | flat out, face down; the broth is on the ground and the bowl is gone |
| 4.2-5.0 | gets up: the fall played backwards                                   |
| 5.0-6.2 | stands over the mess, empty-handed                                   |
| 6.2-8.4 | lingers                                                              |
| 8.4-9.5 | walks back inside                                                    |

The path runs straight out from the doorway along the house's own facing for
2.4 m, so it never crosses the building and never needs the navigation grid.
The walk phase advances with distance covered rather than wall-clock, so the
resident does not skate. The spill is drawn as overlapping discs rather than
one disc, because a single disc reads as a painted circle, and it fades out
with the sequence.

`HomeActivity::cooldown_seconds` defaults to 600 and clamps to at least 300.
Entity/cycle hashes select only about a quarter of eligible windows. Losing
eligibility or visibility releases the slot on the next frame rather than
stranding it.

## Reproducible checks

Build `render_tests` and `arena_app`. Run:

```sh
QT_QPA_PLATFORM=offscreen LC_ALL=C build/bin/render_tests --gtest_filter='HomeActivityTest.*'
DISPLAY=:0 build/bin/arena_app --batch --scenario home_ambient_street --clean-capture --capture-interval 6 --artifact-dir /tmp/home-street
DISPLAY=:0 build/bin/arena_app --batch --scenario home_ambient_dense --clean-capture --capture-interval 8 --artifact-dir /tmp/home-dense
DISPLAY=:0 build/bin/arena_app --batch --scenario home_ambient_night --clean-capture --capture-interval 8 --artifact-dir /tmp/home-night
DISPLAY=:0 build/bin/arena_app --batch --scenario home_ambient_street --fog-of-war --clean-capture --capture-interval 6 --artifact-dir /tmp/home-fog
SOI_HOME_GAG_SECONDS=12 DISPLAY=:0 build/bin/arena_app --batch --scenario home_ambient_soup --duration 22 --scenario-distance 0.55 --clean-capture --capture-interval 1 --artifact-dir /tmp/home-soup
```

Add `--scenario-distance 0.5` to read the shutters, cloth and residents. Two
review hooks exist because both behaviours are deliberately rare:

- `SOI_HOME_SMOKE_ALWAYS=1` lights every eligible hearth, which is the only
  practical way to inspect both nations' anchors in one capture, and the right
  way to measure a dense street's worst case.
- `SOI_HOME_GAG_SECONDS=<n>` forces the spill every _n_ seconds.

`home_ambient_street` and `home_ambient_dense` destroy a house partway through;
it must stop smoking at once. `home_ambient_night` moves the clock to 21:00 so
the plume and the lamps can be judged against a dark sky. `building_preview`
cannot show any of this — it replays captured primitive draws on the CPU with
no effect or rigged-mesh path — so the Arena is the review hook.
