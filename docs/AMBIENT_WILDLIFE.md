# Ambient Wildlife

Ambient wildlife makes battlefields feel inhabited rather than staged. Sheep occupy pasture, wolves work the edges of settlement and cover, and bird flocks move through the sky or burst away from nearby threats.

This document describes the current wildlife architecture, simulation rules, authoring controls, rendering path, persistence model, and validation fixtures.

## Wildlife model

Wildlife is split into two simulation classes.

**Sheep and wolves are ordinary world entities.** They are spawned through the normal unit factory as `SpawnType::Sheep` and `SpawnType::Wolf`. They carry `TransformComponent`, `UnitComponent`, `MovementComponent`, and species-specific wildlife state; wolves also carry `AttackComponent`. Because they use the normal movement and combat stacks, terrain, water, walls, gates, buildings, attacks, health, selection, and pathfinding work through the same systems used by other world bodies.

**Birds are cosmetic flock data rather than entities.** `BirdFlockManager` stores compact per-bird state such as position, yaw, altitude, wing phase, and behavior. Birds cannot be targeted, do not collide, and do not participate in ground pathfinding or authoritative combat.

`WildlifeSystem` owns both halves and is registered with every runtime world.

## Ownership and combat safety

Every ground animal belongs to `NEUTRAL_OWNER_ID`.

Neutral ownership provides the simulation boundaries wildlife needs:

- `VisibilityService` ignores neutral units as vision sources, so wildlife never reveals map tiles.
- `is_troop_spawn` excludes wildlife, so population accounting, victory logic, and formation systems ignore it.
- Ground animals remain normal damageable entities with health and movement.

Target legality is resolved through `Combat::evaluate_target`, not diplomacy alone. Wildlife targeting depends on engagement intent and the animal's combat state.

Under `EngagementIntent::Ordered`, an animal is a legal explicit combat target. This supports deliberate hunting, wolf retaliation, and soldiers striking a wolf that is actively attacking them.

Under `EngagementIntent::AutoAcquired`, passive wildlife is protected from automatic aggression:

- Sheep are not selected automatically.
- Wolves that are not in a fight are not selected automatically.
- Guard behavior ignores passive animals.
- Towers do not waste attacks on passive wildlife.
- Faction AI does not treat passive wildlife as a threat.
- Passive animals do not populate attack-cursor targeting.

A wolf that commits to a person or is attacked carries `hostile_timer`. While hostile, the wolf is a valid automatic target as well as a valid explicit, retaliation, and AI target.

Civilians do not take retaliation targets. A bitten civilian flees rather than entering a combat exchange.

### Wildlife retaliation

`assign_retaliation_target_if_needed` records retaliation state directly in `WildlifeComponent` through `aggressor_id` and `hostile_timer`.

Wildlife does not receive an `AttackTargetComponent` for retaliation. Its movement and attack decisions remain owned by the nature AI, avoiding conflicting commands from the RTS attack processor.

### Fog of war

Birds are submitted only when their tile is currently visible. Explored-but-not-visible tiles do not render birds, so a flock cannot reveal activity through fog.

## Nature AI

Wildlife is not controlled by the faction AI in `game/systems/ai_system`. Ground animals use `game/wildlife/nature_ai.*`, which evaluates species behaviors by priority.

A `NatureBehavior` receives a `NatureContext` containing the animal, its `WildlifeComponent`, species configuration, the threat field, herd or pack state, centroid, and alarm state. Behaviors act through `NatureActions`, which owns side effects such as movement, stopping, speed selection, herd alerts, open-point selection, prey search, and bites.

`NatureBrain::tick` evaluates behaviors from highest to lowest priority and stops at the first behavior that takes control.

| Priority   | Sheep           | Wolf                          |
| ---------- | --------------- | ----------------------------- |
| `Survival` | `sheep.flee`    | `wolf.defend`, `wolf.retreat` |
| `Interest` | `sheep.regroup` | `wolf.stalk`                  |
| `Routine`  | `sheep.graze`   | `wolf.menace`                 |
| `Ambient`  | `sheep.drift`   | `wolf.prowl`                  |

Behaviors at the same priority preserve registration order.

## Sheep behavior

Sheep graze, drift, regroup, and flee.

`sheep.flee` reacts to the last attacker, armed troops, and wolves inside the alert radius. The alert propagates to the herd so nearby sheep flee as a group. Civilians and builders are intentionally weak threats so non-combat settlement activity does not constantly stampede livestock.

`sheep.regroup` pulls animals back toward the herd centroid when they drift too far from the group.

`sheep.graze` holds the animal in place for a grazing interval.

`sheep.drift` wanders around the herd anchor when no higher-priority behavior owns the tick.

A frightened sheep that cannot continue moving uses the tense `alert` state rather than the calm idle state.

## Wolf behavior

Wolves prowl, stalk, attack, defend the pack, menace civilians, and retreat when local strength is unfavorable.

`wolf.defend` turns a wolf toward an attacker and alerts the pack when the local fight is supportable.

`wolf.retreat` takes priority when nearby hostile strength exceeds the pack's tolerance.

`wolf.stalk` scores nearby sheep and people inside the detection radius. Livestock is preferred at equal distance, civilians are preferred over soldiers, and escorted targets become less attractive as nearby strength rises.

Pack focus is cooperative rather than purely individual. A quarry already engaged by one or two packmates becomes more attractive according to the pack's `aggression`; a third committed wolf closes that focus slot. Low-aggression packs spread out more readily.

### Pack slots

Wolves focused on the same quarry claim deterministic positions on a ring.

Pack members are ordered by entity id and assigned an angular share. `pack_ring_radius` considers both bite reach and the lateral arc required for the pack to stand around the target. The ring is capped by `reach * k_pack_ring_reach_margin` so assigned attack positions remain inside usable bite distance.

When a wolf is inside bite range, it faces the prey through `desired_yaw` while the bite is active. Between bites it retains its slot and advances a per-wolf `orbit` by `k_pack_orbit_step`, producing circling pressure rather than a static ring.

Committing to a person marks the wolf hostile.

### Stall release

An animal that is not deliberately holding still must make progress.

`release_if_stalled` watches animals that are not grazing, biting, or flinching. After `k_stall_release_seconds` without measurable movement, it stops the mover, clears the think cooldown, and sets `stalled`.

The next behavior tick responds to `stalled` by changing the kind of target it chooses:

- Flee behavior redirects toward the home anchor instead of continuing deeper into a corner.
- Drift behavior re-anchors around home.
- Wolf orbit shifts by half a turn.
- Flee headings include per-animal angular jitter and fallback veers at approximately ±60° and ±110° if the straight escape direction does not gain ground.

This keeps animals from repeatedly issuing the same unreachable movement order.

### Bite timing

Wolf contact checks are independent of the nature-AI think interval.

`try_contact_bite` runs every frame for wolves with a `focus_id` in reach. The brain decides who to attack and where to stand; the contact path decides when the bite is physically allowed to begin.

`begin_bite` is the single source of truth for bite timers, bite statistics, audio cues, and bite-facing behavior.

A landed bite applies hit feedback. Civilian victims receive `LightFlinch`; armed troops do not, so a wolf pack cannot continuously suppress their melee response through repeated light stagger.

`wolf.menace` approaches civilians that are not selected as prey and holds a standoff distance without dealing damage. `wolf.prowl` roams around the pack anchor.

## Bird behavior

Birds do not run the ground-animal brain.

The default flock behavior is a flyover. A flock waits for a randomized respite, enters from one side of the camera focus, crosses in a loose formation, leaves the far side, deletes its birds, and rearms the interval. `flyover_interval_min` and `flyover_interval_max` control that cadence.

Each bird retains a stable slot offset from the flock leader so the formation does not collapse into a single point.

Setting `flyover_interval_max` to `0` enables resident flock behavior. Resident birds share a wander target around the authored spawn area, orbit the target, and can land to peck.

A nearby threat triggers scatter behavior in both modes, sending the flock upward and outward.

## Simulation cost control

Ground wildlife uses staggered thinking and distance tiers.

Animals maintain independent think timers rather than evaluating in one synchronized sweep. Each ground animal is also classified against the authoritative interest field:

| Tier      | Behavior              |
| --------- | --------------------- |
| `Near`    | Full-rate thinking    |
| `Far`     | Quarter-rate thinking |
| `Dormant` | Brain tick skipped    |

The near and far radii are map-configurable.

The interest field is built from live non-wildlife units and buildings rather than from the camera. Simulation LOD therefore remains a pure function of world state and behaves consistently in headless runs, replays, and different camera positions.

A world containing no non-wildlife anchors treats every animal as `Near`.

`WildlifeSystem::set_cosmetic_focus()` accepts the camera focus only for birds. Bird simulation is cosmetic and may use viewer-relative LOD without affecting authoritative results.

`BirdFlockManager` is session-owned. `SessionContext` holds one flock manager per match, and `WorldView::of(session)` exposes the correct flock to the renderer. Code without a session can use `BirdFlockManager::process_flock()` for tool and test contexts.

Render cost is controlled separately:

- Birds are frustum- and fog-culled before submission.
- Distant birds omit head, beak, and tail detail.
- Sheep and wolves use the shared creature pipeline and its baked minimal LODs.

`WildlifeSystem::stats()` and `BirdFlockManager::stats()` expose counters including near thinks, far thinks, dormant skips, flee events, hunt events, bites, respawns, and scatter events.

## Map configuration

Every map receives wildlife defaults unless the map authors an explicit wildlife block. Default population scales with map area. If a species is enabled without explicit `spawn_areas`, placement can be derived from the map.

Coordinates follow the map's own coordinate system.

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
    "spawn_areas": [
      { "x": 40, "z": 55, "radius": 12 }
    ]
  },
  "wolves": {
    "enabled": true,
    "groups": 1,
    "group_size_min": 3,
    "group_size_max": 5,
    "aggression": 0.45,
    "spawn_areas": [
      { "x": 12, "z": 18, "radius": 8 }
    ]
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

A species omitted from an authored wildlife block stays disabled even when the top-level wildlife system is enabled. Values are clamped during load, including group counts and spatial radii.

`respawn: false` allows a hunted-out population to remain absent.

Bird `spawn_areas` matter for resident flocks. Flyovers enter relative to the map/camera traversal rather than treating the spawn range as a permanent roost.

## Scheduled wolf packs

The wolf block can contain `waves` for mission-timed pack releases.

```json
"wolves": {
  "enabled": true,
  "groups": 1,
  "spawn_areas": [
    { "x": 470, "z": 250, "radius": 26 }
  ],
  "waves": [
    {
      "timing": 200.0,
      "pack_size": 4,
      "x": 470,
      "z": 250,
      "radius": 24,
      "label": "Wolves are down off the bank."
    },
    {
      "timing": 400.0,
      "pack_size": 5,
      "x": 200,
      "z": 560,
      "radius": 26
    }
  ]
}
```

| Field       | Required | Meaning                                             |
| ----------- | -------- | --------------------------------------------------- |
| `timing`    | Yes      | Seconds from mission start                          |
| `pack_size` | No       | Wolves in the pack; default 4, clamped to 64        |
| `x` / `z`   | No       | Den location in map coordinates                     |
| `radius`    | No       | Anchor range around the den                         |
| `label`     | No       | Announcement text; otherwise a generic line is used |

Waves are sorted by `timing` and release exactly once. Fired-wave state and elapsed wildlife clock are serialized so loading a save does not release a completed wave again.

A released wave becomes an ordinary wolf group and follows the normal hunt, death, hostility, and respawn rules.

Wolves are forest-passable, so a den can be placed in cover that ordinary formations cannot traverse. Map authors should account for that asymmetry when placing packs near roads and settlements.

## Derived placement

`game/wildlife/wildlife_placement.cpp` fills missing spawn areas from map data.

Placement evaluates a jittered lattice of candidate points, scores them by species preference, and keeps accepted candidates at least `roam_radius * 1.7` apart.

Candidates are rejected inside lakes, rivers, mountains, or roads, within 24 cells of a structure, within 15 cells of a unit spawn, or inside an undead-zone leash.

Species preferences differ:

- Sheep favor open pasture near, but not inside, cover; they avoid settlements and steep terrain.
- Wolves favor deep cover, high ground, and distance from players.
- Birds have weak ground-placement preferences because flyover flocks do not depend strongly on their anchor.

Placement uses the wildlife seed, falling back to the biome seed. Identical map data and seed produce identical wildlife placement.

## Persistence

Sheep and wolves persist through the normal entity snapshot.

`WildlifeComponent` serializes species, behavior, group id, anchor, timers, aggressor state, and RNG state with the entity.

The system also serializes population-level state that cannot live on individual entities:

- Group anchors
- Desired group sizes
- Respawn countdowns
- Scheduled-wave progress
- Bird flock state

This data lives in the save metadata under `wildlife`.

Restoring a save does not rerun the initial wildlife spawn, preventing duplicate herds or packs.

## Rendering

Sheep and wolves use baked skinned meshes in the shared creature pipeline alongside other quadrupeds.

Their geometry is authored as `Quadruped::MeshNode` graphs.

### Sheep

The sheep mesh uses a fleece barrel with nonuniform rings, pronounced wool clumps over the crest, shoulders, hips, and rump, a lower wool skirt, dark face and legs, cloven hooves, poll cap, ears, eyes, and a three-segment neck that can bend naturally into grazing.

Breed variation gives part of the herd brown or black fleece with matching face variation.

### Wolf

The wolf mesh uses a narrow chest, tucked waist, stronger haunches, countershaded underside and face, a dark saddle following the body rings, articulated canid rear legs, triangular ears, facial mask, neck ruff, mane, and a multi-segment tail.

Pelt variation includes brown and near-black coats.

### Birds

`render/wildlife/bird_flock_renderer.cpp` draws body, flapping wings, tail, and beak directly from flock data during the scene walk.

Ground species keep fine features such as eyes, inner ears, extra fleece tufts, and limb masses in the full LOD. Minimal LODs preserve the silhouette-defining shapes.

## Ground-animal gait

Wildlife animation uses the species' own movement scale rather than one shared threshold.

`resolve_gait` compares smoothed speed with fractions of species top speed. `k_top_speed` is tied to the species flee speed, and the run threshold sits above ordinary walking/prowling speed and below flee speed.

### Footfall pattern

`leg_phase_offset` is gait-specific.

Walk and stalk use a four-beat lateral sequence. Running uses a transverse gallop pattern with the fore pair landing near each other and the hind pair following.

### Stride and cadence

`gait_advance` is derived from `stride / stance_duty`. The distance advanced by the body and the stance-foot sweep therefore describe the same gait.

Cadence is changed by authoring stride length and stance duty rather than independently scaling cycle advance.

### Body motion

Running drives vertical body bob at twice the stride frequency plus spine pitch during foreleg loading.

Legs are IK-solved from moving hip positions to toes placed in ground space. Vertical body motion is also applied to the leg hips, keeping the torso and leg chains connected while planted feet remain grounded.

### Clip transitions

`resolve_clip_transition` stores the per-entity outgoing state and phase. For `k_clip_blend_seconds`, the previous clip is supplied to `full_body_blend` with a decaying weight.

This crossfade applies to ordinary gait changes and transitions into one-shot actions such as bite.

### One-shot phase

Bite and death use a latched `action_phase`.

A one-shot phase never moves backward within the same action, even when the creature is drawn by multiple passes around the same simulation step. A large phase drop is treated as a new action instance.

### One speed source

`gait_speed` is the single smoothed speed used for both gait selection and locomotion phase advancement. `resolve_gait` receives that value directly.

This keeps clip selection and cycle rate on the same speed source.

### Standing animation

Standing clips use `ambient_phase` rather than distance-driven locomotion phase.

Wolf stationary stalk behavior uses the looping `crouch` / `StateId::WildlifeTense` state. Sheep use the corresponding `alert` state.

Idle, alert, and crouch clips contain breathing, head movement, ear motion, tail motion where applicable, and lateral weight shifts. Their periodic drives use integer harmonics of clip phase plus loop-safe `pulse()` functions.

### Bite pose

The wolf bite includes a coil, jaw gape of approximately 1.05 rad, forequarter lift through `rear`, clamp, and `head_roll` during the wrench phase.

Wildlife victims detect health loss through `WildlifeComponent` and arm `flinch_timer`. Sheep render that reaction through the one-shot `startle` clip. The same path works for damage from any source.

### Authored timing

Playback duration matches baked clip duration.

Bite playback uses `k_bite_animation_seconds`, and `k_bite_impact_phase` matches the authored contact phase so damage aligns with jaw closure.

Wildlife death uses the quadruped death profile.

Standing clip periods are expressed directly from their authored `frames / fps`.

### Smoothed locomotion phase

`gait_phase` advances from low-pass-filtered measured speed using `speed * dt / advance`.

Turning or temporary path correction therefore changes cycle speed smoothly rather than tying phase directly to one frame of translated distance.

### Wolf head chain

Wolf head and shoulder rotations are evaluated around the current transformed pivots. Counter-rotation moves the head chain with the spine before local head yaw is applied around the current withers pivot.

Rig changes should preserve intended segment lengths across every clip. `(pose.poll - pose.withers).length()` is the primary check for the neck/head chain.

## Baked species assets

Sheep and wolves have full `SpeciesManifest` definitions under `render/wildlife/`.

Key files are:

- `wildlife_rig.{h,cpp}` — shared 21-bone quadruped rig: root, body, four three-joint legs, neck, head, articulated jaw, ears, and two-segment tail.
- `sheep_spec.cpp` / `wolf_spec.cpp` — bind pose, sampled pose functions, and `Quadruped::MeshNode` graphs.
- `sheep_manifest.cpp` / `wolf_manifest.cpp` — clip descriptors and bake callbacks.

`tools/bpat_baker` produces:

- `sheep.bpat`
- `wolf.bpat`
- `*_full.bprm`
- `*_minimal.bprm`
- `*_minimal.bpsm`

Both species contain eight baked clips. Sheep includes `startle` and `alert`; wolf includes `crouch`.

The full sheep body is 3024 triangles and the full wolf body is 3612. Minimal LODs are 1380 and 1600 triangles respectively. Silhouette-critical wool clumps and wolf limb masses remain in the minimal meshes.

At runtime, `render/wildlife/wildlife_prepare.cpp` resolves animation state, clip phase, grounded transform, and role colors, then submits a `CreatureRenderRequest` to the shared creature pipeline.

`render/entity/wildlife/{sheep,wolf}_renderer.cpp` contains color and clip selection rather than runtime mesh construction.

`resolve_draw_state` derives gait phase, speed ratio, behavior, and stable per-entity color variation shared by both species.

### Wildlife material

Both ground species use `k_wildlife_material_id` in `character_skinned{,_instanced}.frag`.

The wildlife material branch shades coat color from height above the belly, darkening undersides and lifting the spine while using a restrained warm rim. It does not reuse horse color-role semantics.

## Arena fixtures

Wildlife scenarios run with:

```sh
arena_app --batch --scenario <id>
```

| Scenario                     | Coverage                                                 |
| ---------------------------- | -------------------------------------------------------- |
| `wildlife_grazing_herd`      | Idle loop, herd cohesion, sheep silhouette, frame budget |
| `wildlife_herd_flees_troops` | Patrol-triggered herd flee                               |
| `wildlife_wolf_hunt`         | Pack stalking, bite contact, herd panic                  |
| `wildlife_wolf_pack`         | Undisturbed prowl, wolf silhouette, coat, gait           |
| `wildlife_wolf_ambush`       | Pack pressure against a lone patrol                      |
| `wildlife_pack_takedown`     | Close pack kill: bite, flinch, orbit, death              |
| `wildlife_bird_scatter`      | Resident flock movement and scatter                      |
| `wildlife_bird_flyover`      | Flyover entry, crossing, and departure                   |
| `wildlife_mixed_pasture`     | Sheep, wolves, and birds together                        |
| `wildlife_storm_pasture`     | Wildlife shading under rain and wind                     |
| `wildlife_dense_population`  | Dense population against frame budget                    |

Validation uses wildlife-specific expectations including `WildlifeGrazingObserved`, `WildlifeFleeObserved`, `WildlifeHuntObserved`, `WildlifeBirdsScattered`, `WildlifeBirdFlyoverObserved`, `WildlifePopulationHeld`, and `WildlifeCasualtyObserved`.

Arena scenarios establish whether the behavior occurs and whether the correct clips are selected. Clip authoring is reviewed separately with:

```sh
wildlife_preview <wolf|sheep> [out_dir] [clip] [samples] [side|quarter|front]
```

The preview skins baked clips through `Render::Software::SoftwareRasterizer` and writes labeled phase strips without requiring a display. Twenty or more samples should be used when evaluating motion between key phases.

## Map editor

The map editor exposes wildlife ranges as normal canvas elements under a **Wildlife** tool group and **Wildlife** layer.

Authors can place, drag, erase, undo, and edit sheep pasture, wolf range, and bird roost elements. Double-click editing changes species and radius. Creating the first range for a species enables that species when the file is written.

Other wildlife settings, including seed, simulation radii, group sizes, speeds, and flyover intervals, round-trip without being rewritten by the editor.

## Scope

Wildlife is ambience and encounter content, not an economy system.

Sheep do not currently yield food when killed or captured. Ground wildlife already has entity ownership and health, but no resource reward, capture economy, or associated UI is part of this system.
