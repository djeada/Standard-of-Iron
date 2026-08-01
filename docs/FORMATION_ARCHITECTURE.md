# Formation architecture

The game has two formation layers. They operate at different scales, are owned
by different systems, and never share a type. Keeping them apart is the whole
point of this document: an earlier design had one `FormationType` enum whose
values were faction names, and it drove both soldier offsets and multi-unit
placement at once. Adding a faction meant editing an enum that the renderer
also switched on.

|          | Unit layout                            | Army formation                                                    |
| -------- | -------------------------------------- | ----------------------------------------------------------------- |
| Arranges | soldiers inside **one** troop entity   | **many** troop entities                                           |
| Owner    | `Game::Formation::UnitLayoutSystem`    | `Game::Formation::ArmyFormationPlanner` / `ArmyFormationRegistry` |
| Identity | `UnitLayoutId` (interned handle)       | `FormationDoctrineId` + `ArmyFormationIntent`                     |
| Read by  | renderer, combat geometry              | player commands, AI, movement                                     |
| Data     | `assets/data/formations/unit_layouts/` | `assets/data/formations/army/`                                    |

Neither layer knows about the other's types. The renderer never sees an intent;
the planner never computes a soldier offset.

## Four independent concepts

These coexist. Setting one must not silently clear another.

- **Unit layout** — how soldiers stand inside a unit (`spear_brace`, `testudo`).
- **Army formation** — where units stand relative to each other (`Line`, `Column`).
- **Combat stance** — how a unit reacts to threats (hold, guard, aggressive).
- **Order** — what a unit is doing right now (move, attack, patrol).

A unit can sit in a Roman battle line, be under a move order, hold a defensive
stance, and internally be in testudo — all at once. Entering testudo does not
remove it from the army formation, and receiving a move order does not clear
its stance.

## Layer 1: unit layout

`game/formation/unit_layout.h`

A `UnitLayoutStyle` is pure geometry parameters: spacing scales, rank stagger,
echelon drift, rank arc, jitter amplitudes, facing jitter, weapon clearance,
and a `min_separation_scale` floor. `UnitLayoutSystem::offset()` turns
`(index, row, col, rows, cols, spacing, seed)` into one soldier's offset and
yaw.

Three properties the renderer depends on:

- **Deterministic.** All variation is a hash of the unit seed and the soldier
  index. The same query always returns the same offset, so the layout cache and
  the combat-geometry cache can both key on it.
- **Bounded.** Jitter is clamped so two soldiers can never come closer than
  `min_separation_scale` of the nominal spacing. Variation never becomes
  overlap.
- **Stable.** A soldier's slot is a function of its index, not of its current
  position, so rotating or moving a unit cannot make soldiers swap slots and
  walk through each other.

`formed_ratio` (0..1) drives transitions: below 1 the block loosens and jitter
rises, so forming and breaking read as visibly different from formed.

### Selecting a layout

`select_unit_layout(doctrine, troop_type, state)` resolves in three steps:

1. The troop's `TroopFormationProfile` gives a **generic** name for the state —
   `unit_layout`, `defensive_layout` or `marching_layout`.
2. `UnitLayoutLibrary::resolve(doctrine, generic)` tries `"<doctrine>.<generic>"`.
3. If that doesn't exist it falls back to the bare `"<generic>"`.

So `rome.close_order_infantry` overrides `close_order_infantry` for Roman
troops only. A faction with no variant of a style silently inherits the generic
one, and a new faction is a set of new `<doctrine>.*` entries — no code change,
no central enum.

### Faction layout languages

The three shipped doctrines must be identifiable from overhead with neutral
materials, so the difference is carried by **silhouette**, not by spacing
tweaks:

- **Rome — the deep block.** Narrow frontage, wide rank gaps: a tall rectangle.
  Zero echelon, zero stagger, zero rank arc, so every rank is a dead-straight
  line and the ranks stack squarely on top of each other. Jitter and facing
  variation are near zero. Testudo is a `shell` shape, the tightest block in
  the game.
- **Carthage — the wide bow.** Frontage roughly 1.7× Rome's over a shallower
  depth, giving the opposite aspect ratio. Ranks carry a real arc (the line
  bows forward at the centre) and a real echelon (each rank drifts sideways),
  so the block reads as a skewed crescent rather than a rectangle. Positional
  and facing variation are an order of magnitude above Rome's. Its defensive
  layout is an `arc` shield line, not a testudo.
- **Iron Sepulcher — the compressed slab.** The tightest of the three in both
  axes with almost no variation and a procession-like column. Solemn rather
  than chaotic; the distinctness comes from compression, not demonic motifs.

`tests/formation/unit_layout_test.cpp` locks this in with shape metrics rather
than eyeballing: aspect ratio, per-rank curvature, front-to-rear rank skew and
mean facing deviation must stay separated for every paired role, and
`formation_data_loader_test.cpp` re-checks the separation after the shipped
JSON overlays are applied so a data edit cannot quietly erase it.

## Layer 2: army formation

`game/formation/army_formation_planner.h`, `army_formation_registry.h`

### Intent and doctrine are separate

```cpp
enum class ArmyFormationIntent { FactionDefault, Line, Column, Defensive,
                                 Assault, Encirclement, SiegeEscort };
using FormationDoctrineId = std::string;   // "rome", "carthage", ...
```

The player picks the **intent**. The **doctrine** decides how that intent is
expressed. Rome and Carthage can both be told `Line` and produce visibly
different deployments. Adding a faction adds a doctrine id; it never expands
the intent enum.

### Role tags, not troop ids

Doctrine templates match on `RoleTag` bitmasks that troops declare in data:

```json
"formation": {
  "roles": ["line_infantry", "shielded"],
  "unit_layout": "close_order_infantry",
  "defensive_layout": "shield_wall",
  "army_roles": ["centre", "reserve"]
}
```

A template says "put things tagged `spear_infantry` in the screening line", not
"put spearmen there". New troop types slot into every existing template as soon
as they declare their tags.

Line rules are matched in order and the first accepting rule claims the troop.
The last rule in every template must be a catch-all with no masks — content
validation warns when one is missing, because otherwise an unusually-tagged
troop is appended to whatever the final rule happens to be.

### The group owns the state

```cpp
struct ArmyFormation {
  FormationGroupID id;
  FormationDoctrineId doctrine;
  ArmyFormationIntent intent;
  QVector3D anchor;
  float facing, frontage, depth, spacing;
  ArmyFormationOptions options;
  std::vector<EntityID> members;
  std::vector<FormationSlot> slot_list;   // `slots` is a Qt macro
  ...
};
```

`ArmyFormationRegistry` is the single owner. Member entities carry only
`ArmyFormationMembershipComponent { group_id, slot_id }` — a back-reference,
never a source of truth. Group state is never reconstructed by scanning members
and hashing their ids together.

`ArmyFormationRuntime` prunes dead or destroyed members, marks the group
`needs_replan`, and replans on a fixed interval rather than every frame.

The registry serialises to `world["army_formations"]`, so a formation survives
save/load with its doctrine, intent, anchor, options and slot assignment
intact.

### Terrain fitting

`SlotTerrainFitter` searches outward from each slot's ideal position
independently, in rings, and refuses any candidate already claimed by another
slot. A slot that finds nothing after six rings is marked `Blocked` rather than
being dropped onto a shared fallback point.

This is the fix for the old behaviour, where every unwalkable slot resolved to
the same `resolved_center` and several units stacked on one tile. Slots come
back tagged `Valid`, `Adjusted` (nudged) or `Blocked` (no room), and the
placement preview colours them accordingly before the player commits.

### Movement policies

- **Reform at destination** (default) — the anchor jumps to the target, units
  path independently, and the shape appears at the destination. Cheap and
  robust through chokepoints.
- **Maintain formation** — the anchor starts at the group's current centroid
  and the runtime walks it toward the destination in stages, replanning at each
  one. Members move at `k_maintain_speed_multiplier` of their normal speed via
  `ArmyFormationRuntime::move_speed_multiplier`, which `MovementSystem` folds
  into its speed calculation alongside the defensive-formation multiplier.

Neither policy introduces per-soldier navigation. Tactics stay at troop-unit
level; soldiers are offsets inside a unit, not agents.

## Player controls

`CommandController` exposes the whole surface to QML:

- Preset intents, filtered to what the doctrine supports, each with the reason
  it is unavailable when it is (`formation_intent_unavailable_reason`).
- Drag placement: drag length sets frontage, drag direction sets facing.
  Wheel changes depth, `Alt` cycles the strong flank, `Shift` preserves
  left-to-right order, `Ctrl` tightens spacing, `Esc` cancels and releases the
  group.
- An advanced panel that scales the faction template rather than replacing it —
  frontage, depth, spacing, cavalry flank, ranged placement, reserve rows,
  movement policy, mixed-army doctrine policy, and an explicit doctrine
  override.

The preview is planned with `ArmyFormationPlanner::plan` and rendered as unit
footprints, never as individual soldiers.

## AI

`Game::Systems::AI::plan_ai_formation` builds `ArmyFormationMember`s from AI
snapshots and calls the same planner and the same templates the player uses.
The AI cannot bypass availability validation: an intent its force cannot
support comes back rejected, exactly as it would for a player.

`select_ai_intent` picks the intent from posture, force ratio and whether siege
engines are being escorted. Planning runs on the AI's own cadence, not per
frame.

Set `QT_LOGGING_RULES="soi.ai.formation.debug=true"` to trace every AI
formation decision: doctrine, intent, member count, anchor, facing, resulting
frontage and depth, and blocked/adjusted slot counts.

## Content and validation

`FormationDataLoader` overlays `assets/data/formations/**` onto the built-in
defaults, so a data file only lists the fields it changes and the game still
runs with the directory absent.

`content_validator` links `game_sim` and runs the same loader, so the validator
cannot drift from runtime behaviour. It fails the build on unknown role tags,
unknown army roles, unknown layout shapes, unknown intents, troops pointing at
layouts that do not exist, doctrines with no `faction_default` template, and
non-positive spacing scales.

See `assets/data/formations/README.md` for the file schemas.

## Tests

| File                                                    | Covers                                                                                               |
| ------------------------------------------------------- | ---------------------------------------------------------------------------------------------------- |
| `tests/formation/unit_layout_test.cpp`                  | determinism, faction distinctness, min separation, role and stance layouts, low counts               |
| `tests/formation/army_formation_planner_test.cpp`       | role placement, doctrine resolution, mixed-army policies, intent availability, terrain fitting       |
| `tests/formation/army_formation_registry_test.cpp`      | group lifecycle, membership, slot retention, save/load                                               |
| `tests/formation/formation_movement_test.cpp`           | both movement policies, speed multiplier, no slot stacking                                           |
| `tests/formation/formation_data_loader_test.cpp`        | overlay semantics, every validation failure mode, and that the shipped data keeps the factions apart |
| `tests/formation/formation_terrain_navigation_test.cpp` | slots against rivers, bridges, hills, walls and buildings; reachability; no stacking                 |

These are headless and run in `simulation_tests`. The terrain suite is a real
test rather than a tautology: disabling `resolve_terrain` makes four of its
cases fail, which is the check that it is actually exercising the fitter.

The Arena scenarios prefixed `unit_layout_` and `army_formation_` cover the
visual side and need a real GPU context; `tests/tools/arena_scenarios_test.cpp`
asserts headlessly that each one exists, spawns groups, carries expectations,
issues a `FormationMove`, and — for the terrain fixtures — actually declares the
rivers, bridges, elevation patches, walls and buildings it claims to test.
