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

Neutrality decides who may be shot at, not what an animal _is_ to the combat code. An
animal is an ordinary enemy of every owner (`is_valid_enemy_unit`), which is what makes an
ordered hunt land, lets a wolf's bite provoke the retaliation every other hit provokes, and
lets a soldier's swing connect with the wolf that is chewing on him. What neutrality buys
is the second predicate, `is_auto_acquirable_enemy`: **passive** wildlife — every sheep, and
any wolf that is not currently in a fight — is skipped by everything that picks a target on
its own, so troops march past a herd, guard mode ignores it, towers do not waste bolts on
it and the attack cursor does not paint a marker over every animal on the map. A wolf that
has committed to a person, or that has been hit by one, carries `hostile_timer` for a few
seconds; while it burns, that wolf is a target troops will take on their own initiative,
which is what turns a pack rush into a fight instead of a massacre.

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
reduces to "wolves take the isolated and leave the escorted alone". It approaches on a
per-wolf angle so the pack surrounds the target instead of stacking, and inside bite range
applies melee damage on the attack cooldown. Committing to a person, rather than to a
sheep, is what marks the wolf hostile. With aggression at or above 0.5, `wolf.menace`
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

Woods are the natural place to den a pack. Wolves count as forest-passable, so a pack in a
grove can come at a column that has no way to follow it in; see
[the grove rules](../scripts/RTS_MAP_DESIGN.md).

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

## Baked species assets

Sheep and wolves also have a full `SpeciesManifest` under `render/wildlife/`, alongside
the horse and the elephant, so their geometry and animation can be produced at build time
instead of assembled every frame:

- `wildlife_rig.{h,cpp}` — the shared twenty-one-bone quadruped rig (root, body, four
  three-joint legs, neck, head, articulated jaw, two ears and a two-segment tail).
- `sheep_spec.cpp` / `wolf_spec.cpp` — the bind pose, the pose function each clip samples,
  and the whole-mesh node graphs authored as `Quadruped::MeshNode` shapes.
- `sheep_manifest.cpp` / `wolf_manifest.cpp` — clip descriptors and the bake callback.

`tools/bpat_baker` walks both manifests and writes `sheep.bpat`/`wolf.bpat` (six and seven
clips), the `*_full.bprm`/`*_minimal.bprm` bodies and the `*_minimal.bpsm` snapshots into
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

Ten scenarios cover the feature; run them with
`arena_app --batch --scenario <id>`:

| Scenario                     | What it proves                                              |
| ---------------------------- | ----------------------------------------------------------- |
| `wildlife_grazing_herd`      | Idle loop, herd cohesion, sheep silhouette, frame budget    |
| `wildlife_herd_flees_troops` | A patrol triggers the herd's flee response                  |
| `wildlife_wolf_hunt`         | Pack stalking, bites, herd panic                            |
| `wildlife_wolf_pack`         | Undisturbed prowl: wolf silhouette, coat and gait           |
| `wildlife_wolf_ambush`       | A pack rushes a lone patrol; both sides take losses         |
| `wildlife_bird_scatter`      | Resident flock cruise and burst under a marching column     |
| `wildlife_bird_flyover`      | Empty sky, then a flock crosses it and leaves               |
| `wildlife_mixed_pasture`     | Sheep, wolves and a passing flock in one frame              |
| `wildlife_storm_pasture`     | Wildlife shading under heavy rain and wind                  |
| `wildlife_dense_population`  | Six herds, three packs, six flocks against the frame budget |

They assert through wildlife-specific expectations (`WildlifeGrazingObserved`,
`WildlifeFleeObserved`, `WildlifeHuntObserved`, `WildlifeBirdsScattered`,
`WildlifeBirdFlyoverObserved`, `WildlifePopulationHeld`,
`WildlifeCasualtyObserved`) rather than by eye.

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
