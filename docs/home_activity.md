# Residential ambient activity

Roman and Carthaginian houses look inhabited: a hearth vents smoke through an
opening the model actually has, and very occasionally a resident carries a bowl
of soup out of the door and wears it. Everything here is presentation-only. No
population, selection, collision, pathfinding, vision, resource output or
replay state is added, and nothing reads the live simulation while drawing.

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

## The plume

`hearth_smoke` is its own effect kind, not a recoloured dust cloud or flame.
`CombatDust` is a ground-hugging dome and `BuildingFlame` adds ember sparks
unconditionally; neither can pass for domestic smoke. The new branch
(`u_effect_type == 7` in `combat_dust.vert`/`.frag`) is a column that rises,
widens with height, leans on a shared wind vector and dissolves near the top.
It is deliberately dim, because the effect shader is unlit — a bright plume
would glow at night rather than catch the last of the daylight.

It emits no local light. Only flames feed the light buffer, and a cooking fire
seen through a roof should not illuminate the street.

## Scheduling

Each house is lit for a stretch and then goes out. Its offset, period, duty
cycle and strength all come from a hash of its entity id, so neighbours never
share a cycle and a street never animates in lockstep. Roughly a third of
eligible houses are alight at any moment. The intensity ramps in and out over
about nine seconds at each end, so a hearth is banked up and dies down rather
than switching.

Zero-health, ruined, neutral, previewed, pending-removal, dying and
dismantling houses show nothing. Eligibility is resolved for every house in one
scan of the locked render snapshot at the top of the frame; submission is then
a binary search.

A house carries a `UnitComponent`, so the render snapshot files it with the
units and it is drawn from `Renderer::plan_unit_entry`, not from
`Renderer::submit_non_unit_entry`. That is where the per-frame `HomeActivity`
is attached to the draw context.

## Budget

| Quality | Plumes per world render | Gag actors |
| ------- | ----------------------: | :--------- |
| Low     |                       0 | no         |
| Medium  |                       8 | no         |
| High    |                      20 | yes        |
| Ultra   |                      32 | yes        |

Reduced effects means no rigged gag actor at all, not merely a cheaper one.
Houses below nine projected pixels or beyond 190 world units are skipped, and
the gag needs twenty-two pixels before it will play. Earlier visible houses
consume the budget. Fog is checked on current visibility, never on merely
explored ground, so smoke cannot betray an unseen residence.

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
QT_QPA_PLATFORM=offscreen build/bin/render_tests --gtest_filter='HomeActivityTest.*'
DISPLAY=:0 build/bin/arena_app --batch --scenario home_ambient_street --clean-capture --capture-interval 6 --artifact-dir /tmp/home-street
DISPLAY=:0 build/bin/arena_app --batch --scenario home_ambient_dense --clean-capture --capture-interval 8 --artifact-dir /tmp/home-dense
DISPLAY=:0 build/bin/arena_app --batch --scenario home_ambient_night --clean-capture --capture-interval 8 --artifact-dir /tmp/home-night
DISPLAY=:0 build/bin/arena_app --batch --scenario home_ambient_street --fog-of-war --clean-capture --capture-interval 6 --artifact-dir /tmp/home-fog
SOI_HOME_GAG_SECONDS=12 DISPLAY=:0 build/bin/arena_app --batch --scenario home_ambient_soup --duration 22 --scenario-distance 0.55 --clean-capture --capture-interval 1 --artifact-dir /tmp/home-soup
```

Two review hooks exist because both behaviours are deliberately rare:

- `SOI_HOME_SMOKE_ALWAYS=1` lights every eligible hearth, which is the only
  practical way to inspect both nations' anchors in one capture, and the right
  way to measure a dense street's worst case.
- `SOI_HOME_GAG_SECONDS=<n>` forces the spill every _n_ seconds.

`home_ambient_street` and `home_ambient_dense` destroy a house partway through;
it must stop smoking at once. `home_ambient_night` moves the clock to 21:00 so
the plume can be judged against a dark sky. `building_preview` cannot show any
of this — it replays captured primitive draws on the CPU with no effect or
rigged-mesh path — so the Arena is the review hook.
