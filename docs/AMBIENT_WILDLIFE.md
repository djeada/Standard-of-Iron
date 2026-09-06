# Ambient Wildlife

A battlefield that only contains soldiers reads as a stage set. Sheep drifting across a
pasture, a wolf pack shadowing the treeline and a flock bursting out of the grass when a
column marches past are what make a map feel like a place that existed before the armies
arrived.

This document explains how that ambience is built, why sheep and wolves are real world
objects while birds are not, and which knobs a map author has.

## The split: two kinds of animal

**Sheep and wolves are ordinary entities.** They are spawned through the normal unit
factory (`SpawnType::Sheep`, `SpawnType::Wolf`), they own a `TransformComponent`, a
`UnitComponent` with health, a `MovementComponent` and — for wolves — an
`AttackComponent`. That is a deliberate decision: the player must be able to hunt a sheep
and fight a wolf, and both only work if the animal is a thing the combat, movement and
selection systems already understand. Reusing the movement stack also means terrain,
water, walls, gates and building footprints block an animal for free, because the same
pathfinder that refuses to walk a legionary into a lake refuses to walk a sheep into one.

**Birds are not entities.** A flock is a compact array inside `BirdFlockManager`: position,
yaw, altitude, a wing phase and a behaviour byte per bird. Nothing can target a bird,
nothing collides with one, and no ground pathfinding is involved. A hundred birds cost
about as much as a hundred rows in a vector, which is the only reason a map can afford
several flocks at once.

Both halves are owned by `WildlifeSystem`, an ordinary `Engine::Core::System` registered
with every runtime world.

## Ownership and what wildlife must never do

Every animal belongs to `NEUTRAL_OWNER_ID`. That single fact carries most of the safety
guarantees:

- `VisibilityService` skips neutral units when it gathers vision sources, so wildlife
  never reveals a tile. A sheep cannot scout for you.
- `is_troop_spawn` excludes both wildlife spawn types, so population counts, victory
  conditions and formation code ignore them.

Neutrality decides nothing about who may be shot at. That question has one answer,
`Combat::evaluate_target` — see the target-validation section of
[COMBAT_SYSTEM.md](COMBAT_SYSTEM.md) — and the only input it takes about an animal is
whether the animal is currently in a fight, never who owns it. An animal is an ordinary
enemy of every owner under `EngagementIntent::Ordered`, which is what makes an ordered hunt
land, lets a wolf's bite provoke the retaliation every other hit provokes, and lets a
soldier's swing connect with the wolf that is chewing on him. Civilians are the one
exception: they never take a retaliation target, so a bitten villager runs instead of
turning to fight — see [SETTLEMENT_LIFE.md](SETTLEMENT_LIFE.md).

The protection animals do get is `EngagementIntent::AutoAcquired`: **passive** wildlife —
every sheep, and any wolf that is not currently in a fight — is refused to everything that
picks a target on its own, so troops march past a herd, guard mode ignores it, towers do
not waste bolts on it, the attack cursor does not paint a marker over every animal on the
map, a right-click on a sheep is a move order rather than a hunt, and the faction AI never
sees one in its threat list. A wolf that has committed to a person, or that has been hit by
one, carries `hostile_timer` for a few seconds; while it burns, that wolf is a target
troops will take on their own initiative, which is what turns a pack rush into a fight
instead of a massacre — and it is equally a legal target for an explicit attack order, for
retaliation and for the AI, because all four ask the same function.

Getting that wrong is what issue #1414 was: the command validator asked `are_enemies`,
a diplomacy question that is false for every neutral owner, and refused an attack order on
a wolf that was mid-bite with "cannot attack a friendly or neutral target" — while the
combat code the order would have fed was perfectly willing to swing at it.

The retaliation hook is the one place where wildlife takes a different path from a troop:
`assign_retaliation_target_if_needed` records the attacker in `WildlifeComponent`
(`aggressor_id`, `hostile_timer`) and stops, rather than hanging an `AttackTargetComponent`
on the animal. An animal that carried one would be driven by the RTS attack processor and
by its own brain at the same time, and the two would fight over its movement orders every
tick.

The renderer applies the matching rule on the presentation side: a bird is only submitted
when its tile is _currently visible_, not merely explored, so a flock scattering in the
fog cannot betray the army that startled it.

## Behaviour: the nature AI

Wildlife is not driven by the faction AI in `game/systems/ai_system` — that one reasons
over a whole-map `AISnapshot` and issues player-style commands, which is the wrong shape
for a neutral animal. Wildlife instead has its own brain in `game/wildlife/nature_ai.*`,
built the same way: a set of behaviours sorted by priority, each of which either takes
control of the animal this tick or declines.

A `NatureBehavior` sees a `NatureContext` (the animal, its wildlife component, its species
config, the threat field and its herd's centroid and alarm) and acts through a
`NatureActions` interface — move, halt, set travel speed, alert the herd, pick an open
point, find prey, bite. Keeping the side effects behind that interface is what lets the
behaviours stay pure decision logic while `WildlifeSystem` owns the world, the pathfinder
and the stat counters.

`NatureBrain::tick` walks the behaviours from highest priority down and stops at the first
one that returns true, so the priority ladder _is_ the animal's mind:

| Priority   | Sheep           | Wolf                          |
| ---------- | --------------- | ----------------------------- |
| `Survival` | `sheep.flee`    | `wolf.defend`, `wolf.retreat` |
| `Interest` | `sheep.regroup` | `wolf.stalk`                  |
| `Routine`  | `sheep.graze`   | `wolf.menace`                 |
| `Ambient`  | `sheep.drift`   | `wolf.prowl`                  |

Two behaviours may share a priority, and `NatureBrain::add` keeps ties in registration
order, so the table reads top to bottom exactly as the brain evaluates it.

**Sheep** graze, drift and panic. `sheep.flee` fires on whoever last wounded the animal, on
any armed troop or wolf inside the
alert radius, and broadcasts the alarm to the whole group, so the herd bolts together;
civilians and builders are deliberately weak threats, because a shepherd should not
stampede the flock. `sheep.regroup` pulls back a sheep that has drifted too far from the
centroid, which is what makes a herd read as a herd rather than as eight independent
animals. Otherwise `sheep.graze` dwells in place and `sheep.drift` wanders near the herd.

**Wolves** prowl, hunt, fight and retreat. `wolf.defend` turns the animal on whoever
wounded it and calls the rest of the pack in, unless the wolf is outnumbered — the same
tolerance `wolf.retreat` uses, so exactly one of the two takes the tick: a pack answers a
lone hunter and scatters from a column. `wolf.stalk` weighs the nearest sheep against the
nearest person inside the detection radius, preferring livestock at equal distance and
civilians over soldiers, and declines a quarry that is standing behind enough strength to
hurt the pack — which, since a person counts toward the strength around themself, is what
reduces to "wolves take the isolated and leave the escorted alone". It also weighs how many
wolves are already committed to a quarry — but as a **pull**, not a push. A quarry one or
two packmates are on scores _better_ than a free one, scaled by the pack's `aggression`,
and a third wolf closes it off entirely. Pushing the pack apart (which is what the old
squared penalty did) meant three wolves each picked their own sheep and a "hunt" was three
separate one-on-ones; a kill has to look like a pack bringing something down. Three wolves
share a target for 95% of `wildlife_wolf_hunt` now, against never before, while a lazy pack
(`aggression` 0.15, as `wildlife_mixed_pasture` sets) keeps the old spread-out behaviour
because the join bonus scales to nothing.

Each wolf then claims a **slot** on a ring around its quarry: the pack members already
focused on that prey are ordered by entity id, and the wolf takes the share of the circle
its index buys. The ring is sized to the larger of bite reach and the arc the pack needs to
stand abreast, so eight wolves land eight abreast instead of inside one another. A random
per-wolf angle at a fixed radius — which is what this used to be — puts several wolves on
the same point, and a pack that reads as one clipping blob is the result. Inside bite range
the wolf halts and turns to face the prey through `desired_yaw` so the bite lands along the
muzzle — but only while the bite itself is playing. **Between** bites it keeps its slot
angle and adds `k_pack_orbit_step` to a per-wolf `orbit`, so a wolf waiting out its attack
cooldown circles its prey at bite range instead of standing on the spot; that alone
removed most of the frozen-wolf frames from a hunt. Committing to a person, rather than to a sheep, is what marks the wolf hostile.

**An animal that is not deliberately holding still must be moving.** Half-second think
ticks and a `k_move_reissue_epsilon` that suppresses near-identical orders combine badly
at a map edge: a cornered animal can be handed the same unreachable goal forever and stand
there. `release_if_stalled` watches every animal that is not grazing, mid-bite or
mid-flinch, and after `k_stall_release_seconds` without measurable movement it stops the
mover, clears the think cooldown and sets `stalled`. The behaviours read that flag on the
next tick and pick a _different_ kind of target — the flee aims back at the home anchor
instead of further into the corner, the drift re-anchors at home, and a wolf adds half a
turn to its orbit. Flee headings also carry a per-animal arc jitter and fall back through
±60° and ±110° veers when the straight-away target gains no ground, so a herd fans out
rather than stacking on one escape point. Measured over `wildlife_wolf_hunt`, the longest
run of frames where an active animal did not move fell from 3.9 s to 0.7 s.

**Biting is not on the think tick.** `try_contact_bite` runs every frame for every wolf
that has a `focus_id` within reach, and starts the bite on the animal's own attack
cooldown; the brain only decides _who_ to bite and where to stand. When the bite was
gated on the half-second think tick instead, a wolf standing on top of its prey only
landed a bite when its tick happened to line up with both timers, which made the pack's
damage output depend on the frame rate — the same seed dealt 205 damage at 30 fps and
147 at 60 — and made a mauling look like a group of animals loitering around a villager.
`begin_bite` is the one place that arms the timers, counts the bite, fires the cue and
turns the head, so the contact path and the brain path cannot drift apart.

A landed bite also applies hit feedback, and staggers the victim with a `LightFlinch` —
but **only if the victim is a civilian**. Flinching anything else lets a pack stun-lock
armed troops out of their own swings, which is what happened the first time this was
wired up: a soldier being bitten could never enter a melee lock to answer back. With aggression at or above 0.5, `wolf.menace`
closes on civilians it declined to hunt and holds a standoff distance — menacing, never
damaging. `wolf.prowl` roams the pack anchor.

The ambient behaviours never decline, so an animal always has something to do.

**Birds** are not entities and do not run the brain, and by default they are not resident
either: a flyover flock waits out a random respite with an empty sky, then enters from one
side of the camera focus, crosses it in a loose skein and leaves by the other, at which
point its birds are deleted and the timer is rearmed. That is what `flyover_interval_min`
and `flyover_interval_max` buy — birds read as an event rather than as scenery that
happens to be overhead. Each bird keeps a slot offset from the leader, so the formation
survives the crossing instead of converging on one point.

Setting `flyover_interval_max` to `0` restores the resident behaviour: the flock shares a
wander target near its spawn area, individual birds orbit it and occasionally land to peck.
Either way, a threat inside the alert radius bursts the flock upward and outward.

Each animal is re-evaluated on a stagger — roughly every half second, never all animals on
the same frame.

## Cost control

Ambient means "free". Three mechanisms keep it that way:

1. **Staggered thinking.** Animals think on their own timers, not in a synchronised sweep.
2. **Distance tiers.** Every animal is classified against the camera focus each tick:
   `Near` thinks at full rate, `Far` at a quarter of it, and `Dormant` — beyond the far
   radius — is skipped entirely. Both radii are per-map settings.
3. **Render culling.** Birds are frustum- and fog-culled before submission and drop their
   head, beak and tail beyond a detail distance; sheep and wolves ride the shared creature
   pipeline, which batches them and swaps to the minimal baked LOD with distance.

`WildlifeSystem::stats()` and `BirdFlockManager::stats()` expose the counters the tests
assert on (near thinks, far thinks, dormant skips, flee events, hunt events, bites,
respawns, scatter events).

## Map configuration

**Every map gets wildlife.** A map file that says nothing about it is given the default
population at load time, scaled to the map's area, and a map that enables a species without
authoring `spawn_areas` is given ground for it. The block below is what an author writes
when they want something other than that; coordinates follow the map's coordinate system,
so a grid map authors them in grid space:

```json
"wildlife": {
  "enabled": true,
  "seed": 4242,
  "near_simulation_radius": 48.0,
  "far_simulation_radius": 96.0,
  "sheep": {
    "enabled": true,
    "groups": 2,
    "group_size_min": 5,
    "group_size_max": 9,
    "roam_radius": 13.0,
    "move_speed": 0.85,
    "flee_speed": 3.4,
    "alert_radius": 11.0,
    "respawn": true,
    "respawn_delay": 60.0,
    "spawn_areas": [{ "x": 40, "z": 55, "radius": 12 }]
  },
  "wolves": {
    "enabled": true,
    "groups": 1,
    "group_size_min": 3,
    "group_size_max": 5,
    "aggression": 0.45,
    "spawn_areas": [{ "x": 12, "z": 18, "radius": 8 }]
  },
  "birds": {
    "enabled": true,
    "groups": 2,
    "group_size_min": 7,
    "group_size_max": 14,
    "flight_height": 9.5,
    "flyover_interval_min": 22.0,
    "flyover_interval_max": 65.0
  }
}
```

A species that is omitted from an authored block stays disabled even when `enabled` is
true, so a map can ask for sheep without inheriting wolves. Every value is clamped on
load; an author cannot request 5000 packs or a negative roam radius.

Setting `respawn` to false lets a hunted-out herd stay gone. Bird `spawn_areas` only
matter when flyovers are turned off, because a flyover enters from the map edge rather
than from an anchor.

### Wolf packs on a schedule

`spawn_areas` put wolves on the map from the first frame. A `waves` list on the wolves
block instead releases packs partway through a mission, which turns ambient wildlife into
a threat a designer can time:

```json
"wolves": {
  "enabled": true,
  "groups": 1,
  "spawn_areas": [{ "x": 470, "z": 250, "radius": 26 }],
  "waves": [
    { "timing": 200.0, "pack_size": 4, "x": 470, "z": 250, "radius": 24,
      "label": "Wolves are down off the bank." },
    { "timing": 400.0, "pack_size": 5, "x": 200, "z": 560, "radius": 26 }
  ]
}
```

| Field       | Required | Meaning                                                   |
| ----------- | -------- | --------------------------------------------------------- |
| `timing`    | Yes      | Seconds from mission start                                |
| `pack_size` | No       | Wolves in the pack (default 4, clamped to 64)             |
| `x` / `z`   | No       | Where the pack dens, in the map's coordinate system       |
| `radius`    | No       | How far from that point a wolf may be anchored            |
| `label`     | No       | Announcement text; a wave without one gets a generic line |

Waves are sorted by `timing` on load and each releases exactly once. Which have fired is
written into the save alongside the elapsed clock, so reloading a mission does not send a
spent pack in again. A wave pack is a normal group once it lands — it hunts, it can be
killed, and `respawn: false` keeps it dead.

Forests are the natural place to den a pack. Wolves count as forest-passable, so a pack in a
forest can come at a column that has no way to follow it in; see
[the forest rules](../scripts/RTS_MAP_DESIGN.md).

### Derived placement

`game/wildlife/wildlife_placement.cpp` fills in what an author left out, using only what
the map file already describes. It scores a jittered lattice of candidate points and keeps
the best ones, spaced at least `roam_radius * 1.7` apart:

- Points inside a lake, a river, a mountain or a road are rejected outright, as are points
  within 24 cells of a structure, 15 of a unit spawn, or inside an undead zone's leash. An
  author never finds a wolf den in the middle of a player's base.
- **Sheep** want open pasture: cover near the ideal of "close to a treeline, not under it",
  away from the settlements, and off the hills.
- **Wolves** want the opposite: the deepest cover and the highest ground the map has, as
  far from the players as it can get.
- **Birds** are nearly indifferent, since a flyover ignores its anchor anyway.

Placement is seeded from the map's wildlife seed (falling back to the biome seed), so the
same map always lays out the same way.

## Persistence

The animals themselves are ordinary entities, so the world snapshot already carries them;
`WildlifeComponent` adds species, behaviour, group id, anchor, timers, the animal's
current aggressor and the RNG state to the entity payload. What the entity stream cannot express is the _population plan_ — the
group anchors, their desired sizes and their respawn countdowns — plus the birds, which
have no entities at all. Those live in a `wildlife` object in the save metadata, written
and restored beside the undead-zone state. A restored system does not re-run its initial
spawn, so loading a save does not duplicate a herd.

## Rendering

Sheep and wolves are baked, skinned meshes drawn through the shared creature pipeline,
alongside the horse and the elephant. Their shapes are authored as `Quadruped::MeshNode`
graphs, so they keep the low-poly look the rest of the game uses:

- **Sheep** — a fleece core whose barrel rings vary ring to ring, so the back never
  reads as a smooth pill, with large wool clumps over the crest, shoulders, hips and
  rump breaking the silhouette and a skirt hanging over each leg top. The clumps sit
  decisively proud of the barrel rather than grazing it, because coplanar wool and
  barrel surfaces z-fight into dashed seams along the back. A dirty underside carries
  the belly, and dark face and legs end in cloven hooves. A woolly poll cap, drooping
  ears that prick forward when the sheep is alert, and eyes finish the head; the neck is
  a tapered three-segment curve, so grazing bends it to the grass instead of sliding a
  rigid tube. Breed variation gives roughly a third of a flock a brown or black fleece
  and a matching dark face.
- **Wolf** — deep narrow chest, tucked waist and muscled haunches, with countershaded
  belly, throat and cheeks. The dark saddle is a second barrel whose rings track the
  body's, so the marking follows the spine instead of floating over it as a slab. The
  hind legs carry a real canid hock zig-zag and both pairs swing from the hip rather than
  sliding, with a knee flex on the lift. Large triangular ears pin back when stalking, a
  dark mask runs from the crown down the bridge to a pale muzzle, the neck carries a ruff
  under a darker mane, and the bushy tail hangs when prowling and levels out on the
  stalk. Pelt morphs give some wolves a brown or near-black coat.
- **Birds** (`render/wildlife/bird_flock_renderer.cpp`) — body, flapping wings, tail and beak, drawn
  from the flock array during the scene walk.

Both ground species keep their fine detail — eyes, inner ears, extra fleece tufts, limb
masses — in the full LOD only, so a dense map pays for silhouettes rather than eyes.

**The gait thresholds are read against the species' own speeds.** `resolve_gait` scales
the smoothed speed by a `k_top_speed` and compares it to a fraction — so if that constant
is below the animal's own walk speed, the walk clip is unreachable and the animal runs
everywhere. Both species shipped that way: the wolf scaled 1.6 m/s of prowling against a
3.1 top speed and cleared a 0.30 run threshold, and the sheep scaled 0.85 against 1.5.
Each `k_top_speed` is now the species' flee speed and the run threshold sits between its
walk and its flee, so prowling and grazing finally use the walk cycle.

**Footfalls are per gait, not per leg.** The leg phase offsets used to live on the leg and
apply to every gait, which made the run a trot — diagonal pairs, wrong for a canid at
speed. `leg_phase_offset` supplies a four-beat lateral sequence for the walk and stalk and
a transverse gallop for the run, where the fore pair lands nearly together and the hind
pair follows.

**The stride sets the cadence, and the cadence is not a free parameter.** `gait_advance`
is `stride / stance_duty`, which is exactly the distance the body must cover for the
stance foot to stay planted — so the world metres per cycle and the foot's sweep are one
number, not two. That means a gait's cadence can only be slowed by **lengthening the
stride**; scaling the advance on its own buys the slower cycle at the price of feet that
slide. The wolf shipped with a 0.29 stride, which at its 3.9 m/s hunt speed is 5.1 leg
cycles a second — nearly twice a real wolf's gallop, fast enough to strobe at 60 fps, and
with so little reach that the animal reads as gliding. It is 0.52 now, for 2.9 cycles a
second, with the lift raised to match.

**A chain rotates about a pivot only after that pivot has moved.** The bite's worry phase
shakes the head one way and counter-rotates the shoulders the other, and the counter is
applied to `withers` — the head's own parent. Rotating the head about the _old_ withers
and then swinging the withers away leaves the head behind by the whole counter angle: with
the shake at full amplitude the neck measured 0.162 to 0.281 units long over the clip, a
**74% stretch**, and the head and ears visibly tore off. The counter now moves the head
chain along with the spine, and the head's own yaw is applied afterwards about the withers
it is actually attached to; the same measurement is 13%, which is the authored head-lower.
When touching this rig, measure a bone length across the clip rather than looking at it —
`(pose.poll - pose.withers).length()` should only vary by what the pose deliberately
changes.

**The body bounces, and the hips bounce with it.** A run drives a vertical bob at twice
the stride frequency plus a spine pitch that drops the chest as the forelegs load. The
legs are IK-solved from the hip down to a toe placed in ground space, so the bob has to be
added to the leg hips as well as to the torso — move the torso alone and the shoulders
tear away from it. Feet stay where they were placed, and the solver takes up the slack,
which is what makes the bounce read as weight instead of as a floating body.

**Clips cross-fade into one another.** Each baked clip loops cleanly on its own — the
distance-driven `gait_phase` cursor wraps continuously and the bake samples `i/frame_count`
so no frame is duplicated — but the _swap_ between clips used to be instantaneous, and a
hunting wolf swaps constantly: run, stand, bite, run. Every swap snapped the whole
skeleton, which reads as the gait stuttering rather than as a clip change.
`resolve_clip_transition` keeps a per-entity cursor of the state it drew last and the
phase it left it at, and for `k_clip_blend_seconds` afterwards the renderer hands the
outgoing clip to `full_body_blend` with a decaying weight. That layer already existed for
humanoids; wildlife simply never filled it in. The blend is the reason a wolf breaking
from a run into a bite no longer pops.

**A one-shot clip's phase is latched, because a simulation timer is not a render
clock.** The bite and the death read their progress from `bite_timer` and the death
sequence, which the simulation decrements on its own step — and a creature is drawn more
than once per frame (the main pass and the shadow pass), at different points relative to
that step. The bite phase therefore stepped forward and back by exactly one frame's worth,
every frame, for the whole bite: measured over eighteen seconds, 252 of 261 backwards
phase steps across the pack were in `AttackMelee`. `action_phase` keeps a per-entity latch
that never moves a one-shot clip backwards and treats a large drop as the next bite
starting; the same run now has one backwards step, which is a genuine restart.

**A pack can only bite from inside bite reach.** `pack_ring_radius` sizes the ring by the
arc the pack needs to stand abreast, and that was clamped to the bite reach itself — so at
eight wolves the ring landed _exactly_ on the reach boundary and most of the pack sat a
hair outside it, unable to land anything. The per-wolf arc is 0.95 now rather than 1.35,
and the ring is capped at `reach * k_pack_ring_reach_margin` so the whole pack stands
inside the range it needs. Distinct wolves biting a victim went from five to seven.

**One speed picks the clip and drives its phase.** `gait_speed` low-passes the animal's
speed once per draw; the gait is chosen from _that_ number and the phase is advanced with
_that_ number. They used to disagree — the gait came from the instantaneous speed while
the cursor advanced on the smoothed one — and since each gait carries its own `advance`,
a decelerating animal could be handed the short walk or stalk stride while still moving at
run speed, which cycles its legs several times faster than anything it can do. Two speeds
is two sources of truth; there is one now, and `resolve_gait` takes it as an argument.

**A standing animal is on the clock, not on the odometer.** Idle and the standing stalk
pose are looped by `ambient_phase` against wall time with a per-entity offset. They used
to be fed the locomotion phase, which is distance-driven — so a wolf that stopped had its
idle frozen on a single frame, and `wolf_gait_advance(Stand)` is zero anyway, so the
cursor could not have advanced even if it had wanted to.

**A stationary stalker needed a clip of its own.** Being on the clock is not enough if the
clip itself is a stride: `Hold` maps to the wolf's `stalk` **walk cycle**, so a wolf
holding station between bites either froze or skated. `crouch` is a looping three-second
clip baked from the same crouched drive with no stride at all — panting breath, a head
that tracks side to side, ear flicks and a low tail sway — and `resolve_gait` returns
`Stand` below the walk threshold so a stalking wolf that has stopped selects it
(`StateId::WildlifeTense`). The sheep maps the same state to `alert`: head up, ears
forward, quick shallow breathing, weight shifting foot to foot. A frightened sheep that
has run out of room now stands _tensely_ rather than dropping into the calm idle.

**Every standing clip carries idle motion, so nothing is ever a statue.** `idle` was
twenty-four frames of a one-centimetre bob. Both species' idles are ninety-six frames
(four seconds) driven by the new `breath`, `head_turn`, `head_dip`, `ear_*`, `tail_*` and
`weight_shift` fields, which `apply_idle_motion` turns into a rising ribcage, a head that
looks around on the neck pivot, independent ear twitches and a lateral weight shift that
keeps the feet planted. The drives are authored as **integer harmonics of the clip phase**
plus `pulse()` bumps, because anything else does not loop.

**A bite that no one flinches from reads as a nudge.** Two halves were missing. On the
render side the jaw articulated 0.5 rad — barely a parted lip — and the lunge was a
forward slide with the head low; the bite now coils, gapes at 1.05 rad, drives in with a
`rear` that lifts the forequarters off the ground, clamps, then wrenches with a
`head_roll` so the worry phase reads from any camera angle rather than only from above.
On the simulation side nothing told the _victim_ it had been hit: `WildlifeComponent`
now watches its own health, and any drop arms `flinch_timer`, which the sheep renderer
plays as the one-shot `startle` clip — a lateral shy, a hop off the ground, the head
thrown up and a twist away from the bite. It is armed by a health drop rather than by the
bite itself, so a wolf shot by an archer flinches on the same path.

**A clip's authored length is its playback length.** The bite is baked as 32 frames at
30 fps and the game plays it over `k_bite_animation_seconds`; when those two disagreed the
clip simply ran at the ratio between them — 1.08 s of animation crammed into 0.55 s, which
is what "the bite plays at double speed" was. `k_bite_impact_phase` is likewise the same
number as the bake's contact phase, so damage lands on the frame the jaws shut. Wildlife
deaths resolve to the quadruped `Horse` death profile rather than the humanoid one for the
same reason: a wolf was collapsing on an infantryman's 1.0 s timing over a 1.2 s clip. The
standing periods are written as the clip's own `frames / fps` — `24.0F / 24.0F` for idle,
`120.0F / 24.0F` for the sheep's graze — so the number that loops the clip is visibly the
number that baked it.

**The cycle advances on smoothed speed, not on raw distance.** `gait_phase` used to add
`(distance travelled this frame) / advance` straight into the cursor, which tied the legs
to every hitch in translation — and the mover scales translation down while a unit turns,
so a wolf steering around its packmates stalled its own legs for a frame roughly twice a
second. The cursor now low-passes the measured speed over `k_gait_speed_smoothing` and
advances by `speed * dt`, so a turn no longer freezes the stride and a wolf that halts to
bite eases its legs down instead of stopping mid-step.

## Baked species assets

Sheep and wolves also have a full `SpeciesManifest` under `render/wildlife/`, alongside
the horse and the elephant, so their geometry and animation can be produced at build time
instead of assembled every frame:

- `wildlife_rig.{h,cpp}` — the shared twenty-one-bone quadruped rig (root, body, four
  three-joint legs, neck, head, articulated jaw, two ears and a two-segment tail).
- `sheep_spec.cpp` / `wolf_spec.cpp` — the bind pose, the pose function each clip samples,
  and the whole-mesh node graphs authored as `Quadruped::MeshNode` shapes.
- `sheep_manifest.cpp` / `wolf_manifest.cpp` — clip descriptors and the bake callback.

`tools/bpat_baker` walks both manifests and writes `sheep.bpat`/`wolf.bpat` (eight clips
each: the sheep adds `startle` and `alert`, the wolf adds `crouch`), the `*_full.bprm`/`*_minimal.bprm` bodies and the `*_minimal.bpsm` snapshots into
`assets/creatures`, and `bake_creature_assets` lists them so a normal build regenerates
them. The full body is 3024 triangles for the sheep and 3612 for the wolf, with minimal
LODs at 1380 and 1600 — the same order as the horse's 2182. The minimal LODs carry the
wool clumps and the wolf's limb masses because those are what the silhouette is made of;
dropping them at distance is what made the two species read as pills.

At runtime nothing is rebuilt: `render/wildlife/wildlife_prepare.cpp` resolves only the
animation state, the clip phase, the grounded transform and the role colours, then hands a
`CreatureRenderRequest` to the shared creature pipeline, which pulls the baked clip and
mesh. `render/entity/wildlife/{sheep,wolf}_renderer.cpp` are now colour-and-clip selection
only — they hold no geometry.

`resolve_draw_state` derives the shared inputs (gait phase, speed ratio, behaviour, a
stable per-entity colour variation) so the two ground species animate consistently.

Both species bake with material id `k_wildlife_material_id`, which selects a coat branch
in `character_skinned{,_instanced}.frag`. That branch exists because the shared character
path is authored for armour and for the horse: the horse's fur branch keys off the horse's
own colour-role numbers, which mean something different for a sheep, and its countershading
assumes a mesh centred on the origin rather than one standing on y = 0. The wildlife branch
instead shades from height above the belly, so undersides darken and spines catch light,
and it trades the shared cold sky rim for a weaker warm one — a bright blue outline along
a white fleece is what made a grazing herd read as plastic.

## Arena fixtures

Eleven scenarios cover the feature; run them with
`arena_app --batch --scenario <id>`:

| Scenario                     | What it proves                                              |
| ---------------------------- | ----------------------------------------------------------- |
| `wildlife_grazing_herd`      | Idle loop, herd cohesion, sheep silhouette, frame budget    |
| `wildlife_herd_flees_troops` | A patrol triggers the herd's flee response                  |
| `wildlife_wolf_hunt`         | Pack stalking, bites, herd panic                            |
| `wildlife_wolf_pack`         | Undisturbed prowl: wolf silhouette, coat and gait           |
| `wildlife_wolf_ambush`       | A pack rushes a lone patrol; both sides take losses         |
| `wildlife_pack_takedown`     | Close camera on a pack kill: bite, flinch, circling, death  |
| `wildlife_bird_scatter`      | Resident flock cruise and burst under a marching column     |
| `wildlife_bird_flyover`      | Empty sky, then a flock crosses it and leaves               |
| `wildlife_mixed_pasture`     | Sheep, wolves and a passing flock in one frame              |
| `wildlife_storm_pasture`     | Wildlife shading under heavy rain and wind                  |
| `wildlife_dense_population`  | Six herds, three packs, six flocks against the frame budget |

They assert through wildlife-specific expectations (`WildlifeGrazingObserved`,
`WildlifeFleeObserved`, `WildlifeHuntObserved`, `WildlifeBirdsScattered`,
`WildlifeBirdFlyoverObserved`, `WildlifePopulationHeld`,
`WildlifeCasualtyObserved`) rather than by eye.

A fixture answers _whether the fight happens_, not _what a clip looks like_ — an animal is
a few dozen pixels at the gameplay camera, and a panicking herd leaves the shot within
seconds. For the clips themselves use
`wildlife_preview <wolf|sheep> [out_dir] [clip] [samples] [side|quarter|front]`, which
skins each baked clip through `Render::Software::SoftwareRasterizer` and writes a labelled
strip of phases. It needs no display and takes about a second, so it is the loop to
iterate a pose in; the arena is where you then confirm the clip is actually _selected_.
Sampling at 10% steps hides real motion between frames — take twenty samples before
concluding a pose is broken.

## Authoring in the map editor

The editor carries wildlife ranges as ordinary canvas elements, under a **Wildlife** tool
group and a **Wildlife** layer: click to drop a sheep pasture, a wolf range or a bird
roost, drag it, erase it, undo it, and double-click it to edit the species and radius.
Dropping the first range for a species switches that species on when the file is written.

Everything else in the `wildlife` block — seed, simulation radii, group sizes, speeds,
flyover intervals — round-trips untouched, so the editor never flattens a hand-tuned
population just because it opened the file.

## Deliberately out of scope

Sheep do not yield food when killed or captured. The economy hook was left out on purpose:
it turns ambience into a resource mechanic, which needs balance work and a UI affordance
of its own. Everything needed for it is already in place — the animals are real entities
with owners and health — so it can be added later without revisiting this design.
