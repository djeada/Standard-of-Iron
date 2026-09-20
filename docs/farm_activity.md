# Farm activity

Roman and Carthaginian farms show presentation-only field workers. They are
**the nation's own civilian humanoids** -- the same rig, proportions, equipment
archetypes and palette the real civilian and builder units use, driven by the
baked clip bank through `CreaturePipeline::submit_requests`. They are not
entities: no population, selection, collision, pathfinding, vision, resource
output, or replay state is added.

Each nation registers its look once, from its own troop renderers, as a
`NationCivilianRig` (`register_nation_civilian_rig`): the civilian visual spec,
the civilian idle archetype for tending, the builder's sickle archetype for
reaping, and the civilian palette fill. Nothing about a farm worker's
appearance is authored in the farm code, so a change to a nation's civilians
carries straight over.

Registration then derives five archetypes by hanging the shared field props
(`render/entity/farm_worker_props.cpp`) on those two: a straw sun hat for
tending and reaping, hat plus a tied wheat sheaf for carrying, the sheaf alone
for a bare-headed carrier, and a hat tipped forward over the face for the
sleeper. A straw hat is field dress, not faction dress, so both nations share
it. Roughly one worker in five goes bare-headed, so a row of hats is not a
uniform. The props are registered as ordinary humanoid equipment
contributions, and head gear is authored against the rendered skull silhouette
(0.168), not the rig's nominal head radius.

## What a field does

A field carries one to three people, chosen by its entity hash
(`farm_activity_roster`): three a little over half the time, a pair a third
of the time -- as often a reaper with a gatherer as two reapers -- and
occasionally a lone worker. Fewer than three is deliberate; a street of farms
must not read as the same three people copied along it.

Workers 0 and 1 are **lane workers**. Each has a lane running between the crop
rows (`farm_activity_lane`): along x for the Roman field, along z for the
Punic one, offset a little from the row centre per worker. Their loop is three
bouts of _work_ (a whole number of strokes: `construct_chisel` on growing
crops, `construct_reap` when ripe), a _breather_ (`idle`, straightening up and
turning to look down the row) and a few _steps_ along the lane (`walk`, phase
from distance covered, so feet do not skate). Over minutes a worker walks the
lane out and back; at the row end its heading swings round over the first
half-metre of the return rather than flipping. While working it stands turned
30-50 degrees into the standing crop; the turn in and out of that stance is
eased over the first part of the bout and the breather.

Worker 2 is the **headland hand**, on the strip outside the crop rectangle by
the gate. On a growing field it kneels at the edge row (`construct_kneel_chisel`)
with the same bout loop, shuffling along the headland. On a ripe field it runs
the harvest trip (`farm_gatherer_beat`): kneel and bundle at the row end,
stand and lift (the sheaf rig appears), carry it to the stook by the gate,
set it down (the plain rig returns), walk back. The walk beats last as long as
the actual distance takes at that worker's pace.

Every duration, stride, offset, stroke rate, lane offset, facing tilt and hat
is drawn from `ambient_hash` of the entity id with a per-worker sub-stream.
Nothing derives from `animation_time` alone. Two adjacent fields never share a
rhythm, and a hundred fields spread across work, breather and stepping at any
instant (`FarmActivityTest.AHundredFieldsDoNotMoveInLockstep` pins this on
both the pure schedule and the submitted bone palettes).

Clip changes are placed on shared poses -- work bouts end on a stroke
boundary, steps start from a standing idle -- so the swap is as quiet as it
can be without blending. Every pose also carries the outgoing clip, its phase
and a 0.35 s fade weight; the actor path picks those up as soon as
`CivilianActor` grows `blend_clip` / `blend_phase` / `blend_weight` fields
(a compile-time seam in `farm_activity.cpp`), and `blend_out_interrupted_clip`
already does the work on the pipeline side.

## Harvest reading

While a field stands ripe, cut sheaves are stood up in a **stook** beside the
gate on the headland (`farm_activity_stook_anchor`): five sheaves, one more
for each harvest the field has been through up to eight, in the field's own
straw tone. It is part of the field, not a person: it follows the field's own
fog and distance treatment and is still drawn when the actor budget is spent.
It disappears with the reset after harvest, when the grain has been carted
off. Nothing is stooked on a growing crop.

The crop generator keeps the two lane strips clear of stalks
(`farm_activity_clearing`), which reads as walked rows rather than bald
patches; the headland is outside the crop and needs no clearing. Actors are
people, so the field's own scale never reaches their world matrix; only its
placement and row orientation do.

## Eligibility and gating

Zero-growth plots, ruined building visuals, neutral farms, construction
previews, pending removal, death and dismantling hide all actors. A builder
assigned to the field -- harvesting it, raising it or repairing it -- suppresses
the decorative workers there, because a real person is already standing in that
field doing that job.

Eligibility is resolved for every field in two scans of the locked render
snapshot at the top of the frame -- one over builder tasks, one over farms --
and submission is then a binary search on the resulting list. No component is
looked up per field per frame, and the live simulation is never read; growth
and harvest count come off the snapshot.

A farm carries a `UnitComponent`, so the render snapshot files it with the units
and it is drawn from `Renderer::plan_unit_entry`, not from
`Renderer::submit_non_unit_entry`. That is where the per-frame `FarmActivity` is
attached to the draw context; wiring it into the building path alone renders
nothing at all while every unit test stays green.

Each actor position resolves against the session terrain surface and the
field's soil height. Frustum, current fog visibility (not merely explored
terrain), lens occlusion, screen size and distance checks precede submission,
per actor, at the actor's current place on its lane.

## Render budget

| Quality | Workers per field | Maximum submitted actors per world render |
| ------- | ----------------: | ----------------------------------------: |
| Low     |                 0 |                                         0 |
| Medium  |                 1 |                                        24 |
| High    |                 3 |                                        64 |
| Ultra   |                 3 |                                        96 |

The per-field cap is then the field's own roster. Workers below seven
projected pixels or beyond 160 world units are culled. Below fifteen pixels, a
field has at most one worker, always a lane worker. Earlier visible fields
consume the budget; excess fields are skipped. The nap requires both lane
workers visible and is disabled by the one-worker LOD and by pair rosters that
field a gatherer instead.

Workers reuse the shared humanoid rigged-mesh and bone-palette caches, so they
cost the same per body as any other civilian on screen and warm through the
existing creature asset prewarm. Each body arrives as two rigged commands, the
body and its baked attachments. Below twelve projected pixels a worker drops
to `CreatureLOD::Minimal`; that tier resolves a per-state snapshot mesh instead
of the requested clip, so it is kept to sizes where the pose cannot be read.
The stook is one instanced archetype of at most 25 primitives. The per-frame
batch and the eligible-field and real-task lists retain their vector capacity
across frames.

The CPU sampling test reports `dense_submission_p95_us` for 100 field
submissions with the High cap, including field lookup, schedule sampling,
transforms, request assembly and a recording submitter. This is a CPU
microbenchmark, not a GPU frame-time measurement. Use the dense Arena scene on
target hardware to measure the full GPU cost; the actor/primitive caps are
structural budgets, not a promised millisecond cost.

## Lazy farmer

One renderer-wide slot permits at most one sequence, and no extra actors are
spawned -- the two lane workers already on the field play it. The beats are
constants in `Render::GL::Nap`, in seconds from the moment the slot is claimed:

| Window  | Sleeper (worker 0)                              | Coworker (worker 1)                      |
| ------- | ----------------------------------------------- | ---------------------------------------- |
| 0.0-1.2 | sits down (`showcase_rest_sit_down`)            | works                                    |
| 0.8-3.6 | --                                              | walks over (`walk`, phase from distance) |
| 1.2-4.0 | asleep, hat over the face (`showcase_rest_sit`) | arrives, stands and stares               |
| 4.0-4.7 | jolts up: the sit-down beat run backwards       | still staring                            |
| 4.7-7.2 | works at more than twice the normal rate        | still staring                            |
| 7.2-9.0 | back to its normal loop                         | walks back to wherever its loop now is   |

Rest poses are showcase clips, so they sit on the ground without foot snapping.
The walk phase advances with distance covered rather than wall-clock, so the
coworker does not skate. The sequence only starts when the sleeper's own loop
keeps it on the spot for the full nine seconds (`farm_worker_stays_put`), so
nobody sleeps while sliding down the row. Worker 2 keeps working throughout.

`SOI_FARM_GAG_SECONDS=<n>` is the review hook: it forces the sequence to start
every _n_ seconds on every eligible field, which is the only practical way to
capture something meant to happen twice an hour.

`FarmActivity::cooldown_seconds` defaults to 600 seconds and clamps to at least 300. Entity/cycle hashes select only roughly one quarter of eligible scheduling
windows, with different start offsets. Consecutive possible starts are separated
by at least the configured cooldown. Missed off-screen windows are not queued.
Loss of eligibility, visibility, budget or crop stage cancels the sequence;
renderer/world changes and time rewinds clear it. Simulation pause does not
advance it. No schedule data is persisted into gameplay state.

## Reproducible checks

Build `render_tests` and `arena_app`. Run:

```sh
QT_QPA_PLATFORM=offscreen LC_ALL=C build/bin/render_tests --gtest_filter='FarmActivityTest.*' --gtest_output=xml:/tmp/farm-activity.xml
DISPLAY=:0 build/bin/arena_app --batch --scenario farm_activity_single --clean-capture --capture-interval 4 --artifact-dir /tmp/farm-single
DISPLAY=:0 build/bin/arena_app --batch --scenario farm_activity_dense --scenario-distance 0.55 --clean-capture --capture-interval 6 --artifact-dir /tmp/farm-dense
DISPLAY=:0 build/bin/arena_app --batch --scenario farm_activity_single --fog-of-war --clean-capture --capture-interval 4 --artifact-dir /tmp/farm-fog
SOI_FARM_GAG_SECONDS=10 DISPLAY=:0 build/bin/arena_app --batch --scenario farm_activity_lazy_farmer --duration 20 --scenario-distance 0.75 --clean-capture --capture-interval 1 --artifact-dir /tmp/farm-lazy
```

`building_preview` cannot show these workers: it replays captured primitive
draws on the CPU and has no rigged-mesh or bone-palette path, so the Arena is
the review hook. `--scenario-distance` is how you get close enough to read a
pose.

`farm_activity_dense` exercises 12 adjacent farms and the global cap over a
forty-second run -- long enough for the full growth/ripen/reset/destroy cycle
and short enough to stay inside the arena watchdog on software GL. Note that
its groups are seated with `count = 2` and `spacing = ±16` from `x = ∓8`, which
puts the first Roman and the first Punic field of each row on the same tile
(both at `x = 0.5`); the middle column on screen is therefore two farms drawn
on top of each other and shows their combined workers. Judge composition on
the outer columns. `farm_activity_terrain` places both factions on a hill.

The first three scenes start growing, ripen at eight seconds, reset growth at
sixteen and destroy the first pair at twenty-four, so one run covers active,
inactive and destroyed plots; normal growth resumes after the empty reset. Each
carries one Roman and one Carthaginian owner, which is what makes `--fog-of-war`
a real check: the other player's field and its workers must disappear together.
Graphics-quality overrides exercise the LOD tiers.

`farm_activity_lazy_farmer` is the exception. It frames a single field closely
and holds it ripe, because a growth change cancels the sequence by design and
would cut every capture short. Pair it with `SOI_FARM_GAG_SECONDS`; the
deterministic scheduling test already covers the real cooldown, the shared
concurrency cap and cancellation, so nobody has to sit through ten minutes of
wheat to check the timing.
