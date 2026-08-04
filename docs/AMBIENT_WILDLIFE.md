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
- Wildlife is filtered out of automatic target selection (`is_valid_enemy_unit`) and never
  auto-engages anything itself. Troops walk past a herd instead of stopping to kill it; a
  hunt is something the player orders explicitly.
- `is_troop_spawn` excludes both wildlife spawn types, so population counts, victory
  conditions and formation code ignore them.

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

| Priority   | Sheep           | Wolf           |
| ---------- | --------------- | -------------- |
| `Survival` | `sheep.flee`    | `wolf.retreat` |
| `Interest` | `sheep.regroup` | `wolf.stalk`   |
| `Routine`  | `sheep.graze`   | `wolf.menace`  |
| `Ambient`  | `sheep.drift`   | `wolf.prowl`   |

**Sheep** graze, drift and panic. `sheep.flee` fires on any armed troop or wolf inside the
alert radius and broadcasts the alarm to the whole group, so the herd bolts together;
civilians and builders are deliberately weak threats, because a shepherd should not
stampede the flock. `sheep.regroup` pulls back a sheep that has drifted too far from the
centroid, which is what makes a herd read as a herd rather than as eight independent
animals. Otherwise `sheep.graze` dwells in place and `sheep.drift` wanders near the herd.

**Wolves** prowl, hunt and retreat. `wolf.retreat` calls the hunt off when troop strength
near the wolf exceeds its tolerance (scaled by aggression). `wolf.stalk` picks the nearest
sheep inside the detection radius, declines if the prey is standing behind a formation,
and otherwise approaches on a per-wolf angle so the pack surrounds it instead of stacking;
inside bite range it applies melee damage on the attack cooldown. With aggression at or
above 0.5, `wolf.menace` closes on isolated civilians and holds a standoff distance —
menacing, never damaging. `wolf.prowl` roams the pack anchor.

The ambient behaviours never decline, so an animal always has something to do.

**Birds** are not entities and do not run the brain. A flock shares a wander target;
individual birds orbit it and occasionally land to peck. Any threat inside the alert radius
bursts the whole flock upward and outward, after which they settle back into a cruise.

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

Wildlife is off unless a map asks for it. The block is authored in the map JSON next to
`rain`, and spawn-area coordinates follow the map's coordinate system, so a grid map
authors them in grid space:

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
    "groups": 3,
    "group_size_min": 6,
    "group_size_max": 12,
    "flight_height": 7.5,
    "spawn_areas": [{ "x": 32, "z": 32, "radius": 30 }]
  }
}
```

A species that is omitted from the block stays disabled even when `enabled` is true, so a
map can ask for sheep without inheriting wolves. Every value is clamped on load; an
author cannot request 5000 packs or a negative roam radius.

Omitting `spawn_areas` scatters the groups around the middle of the map. Setting
`respawn` to false lets a hunted-out herd stay gone.

## Persistence

The animals themselves are ordinary entities, so the world snapshot already carries them;
`WildlifeComponent` adds species, behaviour, group id, anchor, timers and the RNG state to
the entity payload. What the entity stream cannot express is the _population plan_ — the
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

- `wildlife_rig.{h,cpp}` — the shared twenty-bone quadruped rig (root, body, four
  three-joint legs, neck, head, two ears and a two-segment tail).
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

Seven scenarios cover the feature; run them with
`arena_app --batch --scenario <id>`:

| Scenario                     | What it proves                                              |
| ---------------------------- | ----------------------------------------------------------- |
| `wildlife_grazing_herd`      | Idle loop, herd cohesion, sheep silhouette, frame budget    |
| `wildlife_herd_flees_troops` | A patrol triggers the herd's flee response                  |
| `wildlife_wolf_hunt`         | Pack stalking, bites, herd panic                            |
| `wildlife_wolf_pack`         | Undisturbed prowl: wolf silhouette, coat and gait           |
| `wildlife_bird_scatter`      | Flock cruise and burst under a marching column              |
| `wildlife_storm_pasture`     | Wildlife shading under heavy rain and wind                  |
| `wildlife_dense_population`  | Six herds, three packs, six flocks against the frame budget |

They assert through wildlife-specific expectations (`WildlifeGrazingObserved`,
`WildlifeFleeObserved`, `WildlifeHuntObserved`, `WildlifeBirdsScattered`,
`WildlifePopulationHeld`) rather than by eye.

## Deliberately out of scope

Sheep do not yield food when killed or captured. The economy hook was left out on purpose:
it turns ambience into a resource mechanic, which needs balance work and a UI affordance
of its own. Everything needed for it is already in place — the animals are real entities
with owners and health — so it can be added later without revisiting this design.
