# Formation Architecture

Standard of Iron has two formation layers because they solve two different spatial problems.

A **unit layout** places the soldiers inside one logical troop entity. An **army formation** places multiple troop entities relative to one another. The two layers can cooperate, but they do not share identity or ownership.

| Layer          | Arranges                         | Primary owner                                    | Identity                                      |
| -------------- | -------------------------------- | ------------------------------------------------ | --------------------------------------------- |
| Unit layout    | soldiers inside one troop entity | `Game::Formation::UnitLayoutSystem`              | `UnitLayoutId`                                |
| Army formation | multiple troop entities          | `ArmyFormationPlanner` + `ArmyFormationRegistry` | `FormationDoctrineId` + `ArmyFormationIntent` |

Keeping those layers separate lets a troop change its internal soldier arrangement without leaving its army group, and lets an army redeploy without redefining the geometry of every soldier inside every troop.

## Four independent concepts

Four concepts coexist at runtime and should not be collapsed into one “formation state.”

- **Unit layout** — the local arrangement of soldiers inside one troop entity.
- **Army formation** — the arrangement of troop entities inside a multi-unit group.
- **Combat stance** — how a troop reacts to threats and what defensive/engagement rules apply.
- **Order** — what the troop is currently doing: moving, attacking, guarding, patrolling, holding, and so on.

A troop can therefore be:

- a member of a Roman army line;
- moving toward a new anchor;
- internally in a defensive shield layout; and
- under a hold/guard combat stance

at the same time.

That independence is central to both data authoring and runtime behavior.

# Unit-layout layer

The unit-layout system is defined primarily in `game/formation/unit_layout.h`.

Its job is to turn a stable logical soldier index and an authored style into a local offset and facing inside a troop entity.

## Unit layout identity

`UnitLayoutId` is a compact interned handle. The invalid value is `0xFFFF`.

A troop profile refers to generic layout names. The loaded layout library resolves those names to an interned style and can prefer a doctrine-qualified variant when one exists.

The runtime therefore does not need one giant enum containing every Roman, Carthaginian, Sepulcher, marching, defensive, and special layout.

## Supported unit-layout shapes

`UnitLayoutShape` currently includes:

- `Ranks`;
- `Wedge`;
- `LooseOrder`;
- `Column`;
- `Cluster`;
- `Circle`;
- `Shell`; and
- `Arc`.

The shape selects the broad geometry family. `UnitLayoutStyle` then tunes that family with authored parameters.

## `UnitLayoutStyle`

Current style data includes parameters for:

- lateral spacing scale;
- depth spacing scale;
- rank stagger;
- echelon drift;
- rank arc;
- front/rear tuning;
- positional jitter;
- facing jitter;
- wedge behavior;
- cluster/radius behavior;
- file grouping;
- group gaps;
- file-depth stagger;
- weapon clearance;
- minimum separation scale; and
- preferred column file count.

The result is a data-driven silhouette rather than a hard-coded faction branch.

## `UnitLayoutQuery`

A layout query carries the facts needed to resolve one soldier placement, including:

- layout id;
- soldier index;
- row and column;
- row/column counts;
- live/expected count;
- optional forced file count;
- base spacing;
- deterministic seed;
- `formed_ratio`;
- optional blend source; and
- blend ratio.

`UnitLayoutSystem::offset()` resolves one query into local position/yaw. `UnitLayoutSystem::compute()` provides the same system at whole-unit scale.

The query is intentionally independent of the soldier's current world position. A layout is geometry, not a second movement system.

## Determinism and stable slots

Internal layouts are deterministic for stable inputs.

A soldier's slot is derived from logical identity and layout/query parameters, which gives several useful properties:

- moving the troop does not reshuffle soldiers;
- rotating the troop does not reshuffle soldiers;
- renderer and combat geometry can agree on the same slot;
- replay does not depend on presentation timing; and
- stable soldiers can keep recognizable local positions through transitions.

Random-looking variation is derived from deterministic seed/index input rather than from wall-clock randomness.

## Living-slot ownership

`FormationCombat::living_slot_indices()` is the shared boundary for deciding which internal presentation slots remain occupied.

Combat and renderer-side consumers use the same living-slot information instead of independently estimating survivor positions from health.

That matters because a formation can be visually sparse after casualties without renumbering every survivor into a new position on every frame.

## Doctrine-specific layout resolution

Troop formation profiles name generic layouts such as normal, marching, or defensive styles.

`UnitLayoutLibrary::resolve(doctrine, generic_name)` resolves in two steps:

1. try a doctrine-qualified name such as `<doctrine>.<generic>`;
2. fall back to the bare generic layout.

This allows doctrine-specific silhouettes while preserving a generic fallback.

For example, two factions can both request a close-order infantry role while using different rank stagger, depth, arc, grouping, or shell geometry.

## Data source

Formation data is loaded from:

```text
assets/data/formations/
```

on top of built-in defaults.

The shipped content can therefore refine formation geometry without requiring a new C++ enum or renderer branch for every doctrine variant.

See `assets/data/formations/README.md` for the current file schema.

## Unit-layout silhouette as faction language

Unit layouts carry faction identity through geometry as well as material/colour.

The current data/model can express distinctions such as:

- broad, regular, staggered rank systems;
- compact deep blocks;
- grouped files with visible seams;
- curved/arc fronts;
- dense procession-like columns;
- open shield walls; and
- closed defensive shells.

The important architectural point is that these differences are authored through the layout system. The renderer does not need a `switch (nation)` to decide where soldiers stand.

# Army-formation layer

Army-scale formation state lives in `game/formation/army_formation_types.h` and is owned by `ArmyFormationRegistry`.

An army formation is a committed group of troop entities with one doctrine, one intent, one anchor/facing, one set of options, and one slot plan.

## Army formation intents

The current intent enum is:

```cpp
enum class ArmyFormationIntent {
  FactionDefault,
  Line,
  Column,
  Defensive,
  Assault,
  Encirclement,
  SiegeEscort
};
```

Intent describes the tactical deployment requested by the player or AI.

Doctrine describes how that intent is expressed by the faction/army.

Those concepts are separate so that two doctrines can both support `Line` while producing different role placement, frontage, depth, reserve structure, and internal troop layouts.

## Formation options

`ArmyFormationOptions` refines a doctrine template without replacing it.

Current option families include:

### Flank preference

- `Balanced`;
- `StrongLeft`;
- `StrongRight`;
- `Split`.

### Movement policy

- `ReformAtDestination`;
- `MaintainFormation`.

### Ranged placement

- `Front`;
- `Rear`;
- `Skirmish`.

### Mixed-doctrine policy

- `CompositeByRole`;
- `SeparateContingents`;
- `CommanderDoctrine`;
- `MajorityDoctrine`.

### Numeric/boolean tuning

Options also include:

- frontage scale;
- depth scale;
- spacing scale;
- reserve rows;
- preserve-member-order; and
- doctrine lock.

This makes the player's fine-tuning controls and AI planner operate on the same data model.

## Army role tags

Doctrine templates match troop **role tags**, not hard-coded troop IDs.

A troop's formation data can declare roles such as line infantry, spear infantry, shielded troops, centre, reserve, and other current tags.

The planner can then say “place troops with this role in this part of the formation” rather than “place troop type X at slot Y.”

That makes new troop types compatible with existing doctrine templates when their role data is authored correctly.

## `ArmyFormation`

A committed formation stores authoritative group state such as:

- formation group id;
- doctrine;
- intent;
- anchor;
- facing;
- frontage;
- depth;
- spacing;
- options;
- phase;
- cohesion;
- member entity IDs;
- slot assignments;
- destination;
- advance progress;
- movement-plan state;
- plan revision;
- replan flag; and
- pending movement state.

Member entities carry a formation-membership component containing the group/slot back-reference.

The registry remains the source of truth for group state. Group geometry is not reconstructed by scanning member positions and guessing which formation they were supposed to belong to.

## Planning pipeline

`ArmyFormationPlanner` separates local geometry from world placement.

Conceptually:

```text
members + doctrine + intent + options
            │
            ▼
      build_layout()
            │
            ▼
   local formation slots
            │
            ├─ anchor
            ├─ facing
            └─ terrain/navigation context
            ▼
          place()
            │
            ▼
   world-space FormationPlan
```

`plan()` is the combined convenience path.

This split is important for interactive placement. Dragging a preview across the ground can reuse the same role/template layout while changing only world-space placement and terrain fitting.

## Layout signatures and preview reuse

The planner exposes a layout signature for inputs that affect local slot geometry.

Anchor and facing affect placement, not the local role/template layout itself. A placement UI can therefore avoid repeating expensive role/layout work when only the mouse position or facing changes.

The cache tests verify that the split path and full `plan()` path remain equivalent.

## Terrain fitting

A perfect geometric formation may not fit the world at its ideal coordinates.

`SlotTerrainFitter` resolves each desired slot against walkable terrain and separation constraints.

Every slot is classified as:

- `Valid` — ideal position is usable;
- `Adjusted` — a nearby valid position was found; or
- `Blocked` — no acceptable placement was found.

The result belongs to the plan and can be shown before the player commits the order.

## Slot fitting behavior

Terrain fitting searches outward from the ideal slot position and avoids assigning multiple troops to the same resolved location.

The planner therefore preserves two separate facts:

- **formation intent** — where the doctrine/template wanted the troop;
- **placement result** — where terrain allowed it to stand.

An adjusted slot is not necessarily an error. It means the shape can still be realized with a local nudge. A blocked slot means the planner could not place that member under the current rules.

## Formation phases

`FormationPhase` currently contains:

- `Reforming`;
- `Formed`;
- `Disrupted`;
- `Opening`;
- `Traversing`;
- `Arrived`.

Phase describes the group runtime state, not merely the last command that was issued.

This is useful because formation behavior spans several distinct transitions:

- assembling into a shape;
- holding a coherent shape;
- being disrupted;
- opening/adjusting for motion;
- traversing constrained movement; and
- arriving at the intended destination.

## Cohesion

`ArmyFormationRuntime` measures whether occupied, placeable slots are actually being held.

The current constants in `army_formation_registry.cpp` include:

- in-slot radius scale: `1.35 × spacing`;
- formed threshold: cohesion `>= 0.8`;
- disrupted threshold: cohesion `<= 0.45`.

Cohesion is therefore a measured group property rather than an assumption that the formation is “formed” because a move command completed.

## Cohesion and damage

`ArmyFormationRuntime::damage_taken_multiplier()` is applied in the combat damage pipeline.

This lets army-scale cohesion affect combat while remaining independent from unit-level defensive layouts.

A troop can receive both:

- a defensive-layout modifier from `DefensiveUnitLayoutService`; and
- an army-group modifier from `ArmyFormationRuntime`.

The two effects compose because they describe different spatial layers.

## Movement policies

### Reform at destination

`ReformAtDestination` lets troop entities route toward the new destination and assemble the target shape there.

This is robust through irregular terrain because the group does not try to preserve one rigid footprint through the entire route.

### Maintain formation

`MaintainFormation` keeps the group under formation runtime control as the center advances.

The runtime owns group movement-plan state, and route-follow speed applies `ArmyFormationRuntime::move_speed_multiplier()` together with the other movement modifiers that affect the troop.

Neither movement policy turns internal soldiers into navigation agents. The logical troop entity remains the navigation body.

# Constrained-route traversal

Narrow passages introduce a temporary **traversal layout** inside a troop entity.

This is separate from the authored normal/defensive unit layout and separate from the army group's strategic route.

`UnitTraversalLayoutSystem` publishes `TraversalLayoutFacts` into movement state.

Current facts include information such as:

- current traversal mode;
- target traversal mode;
- corridor/portal identity;
- measured corridor width;
- normal file count;
- chosen file count;
- presented spacing;
- lateral scale; and
- related transition state.

## Traversal precedence

The position contract is layered:

1. **Army formation and route** choose the logical troop root's strategic destination/lane/pace.
2. **Normal or defensive unit layout** defines the base soldier arrangement.
3. **Traversal layout** may temporarily remap soldier rows/files/local anchors to fit constrained ground.
4. **Combat/contact presentation** composes action, facing, hit response, and contact adjustments on that anchor.
5. **Renderer** reads the published final anchor; it does not run another positional integrator.

This layering allows a shield formation to pass through a narrow opening without pretending that the doctrine/defensive state disappeared.

## Soldier-level spatial boundary

`FormationCombat::soldier_spatial_anchors` is the shared boundary for systems that need exact soldier-level coordinates.

Consumers such as combat geometry, RPG/direct-control targeting, weapon traces, casualty effects, and selection/presentation can use the same resolved anchor precedence instead of calculating independent soldier positions.

Ordinary RTS entity selection still operates on troop/entity identity; the soldier-level query is used where exact internal geometry is required.

# Defence Mode

Defence Mode is a unit-layout state, not an army formation.

Each logical troop keeps:

- its world identity;
- troop root position;
- army-group membership;
- current order; and
- owner/nation identity.

Defensive mode changes the internal soldier arrangement and exposes the related movement/combat modifiers through `DefensiveUnitLayoutService`.

## Defensive profiles

Nation data selects:

- eligible troop types;
- the defensive layout;
- transition timing;
- movement/turn behavior; and
- directional combat modifiers.

Roman and Carthaginian infantry can therefore use different defensive geometry without creating a new army-scale intent for every internal stance.

## Defensive layout state

`UnitLayoutStateComponent` carries the authoritative transition state.

Formation data supplies `form_seconds` and `break_seconds` for the transition. Gameplay bonuses and baked defensive presentation are tied to the active formed state according to the current runtime rules.

The component is serialized and included in render/presentation state so save/load and snapshot rendering agree about whether the unit is normal, forming, formed, or breaking.

## Roman shell/testudo

The Roman defensive family uses a closed `Shell`-style arrangement with directional shield presentation for front, rear, flanks, and overhead positions.

The renderer applies the corresponding baked upper-body overlay poses while locomotion can continue through the lower-body authored movement path.

The important architecture point is that “testudo” is an internal soldier-layout/presentation state of one logical troop, not a second army object.

## Carthaginian shield wall

The Carthaginian defensive family uses an open `Arc`-style shield wall rather than the closed Roman shell.

Its authored geometry, poses, timings, and combat modifiers are data/runtime choices within the same defensive-layout system.

# Player formation placement

The player-facing planner uses the production `ArmyFormationPlanner`.

A placement preview is therefore a real formation plan with:

- current intent;
- current doctrine;
- selected members;
- active options;
- anchor;
- facing;
- local dimensions;
- placed slots; and
- blocked/adjusted status.

The preview is not a decorative approximation drawn independently of the actual committed plan.

## Placement gesture

Ground placement establishes the formation anchor. Orientation determines facing. The UI can expose frontage/depth/spacing and other options that modify the planner inputs.

The resulting troop footprints show where logical troop entities will be placed, not every internal soldier.

## Single-troop positioning

One troop can use the same positioning/facing gesture without registering a one-member army formation.

This keeps the useful “place and face this block” control while preserving the semantic boundary that an army formation is a group-level object.

## Formation panel data

The formation panel reads current planner/controller state and can show:

- available intents;
- current doctrine;
- current facing;
- current dimensions/ranks/files;
- slot placement counts;
- blocked/adjusted state; and
- advanced options.

Unavailable intents can remain visible with an explanation instead of changing the meaning/order of intent selection according to the current roster.

## Persistent group status

`FormationStatusBadge` represents committed formation state after placement is over.

It can report:

- intent;
- doctrine;
- phase; and
- cohesion.

This gives the player feedback on whether the group actually formed or became disrupted, not merely which intent was selected earlier.

# AI formation use

AI and player groups share the same planner/runtime.

`Game::Systems::AI::plan_ai_formation` builds formation members from the AI snapshot and resolves the doctrine/intent through the production planner.

AI station/muster logic decides where a group should assemble, but it still relies on formation terrain fitting and slot availability to determine whether the candidate is usable.

This prevents the AI from bypassing the same terrain and role constraints the player sees.

See [AI_ARCHITECTURE.md](AI_ARCHITECTURE.md) for station resolution and committed attack-wave behavior.

# Persistence

Army formations are authoritative game state and are serialized with the match.

The registry persists information required to reconstruct the committed group, including its doctrine, intent, options, anchor, members, slots, phase/cohesion-related state, and movement-plan state as defined by the current serializer.

Member back-references are restored consistently with the group registry.

Unit-layout transition state is also component state and survives save/load.

The save system does not infer formations from what the renderer happened to draw before the save.

# Data validation

`FormationDataLoader` applies authored formation data over the built-in defaults.

Validation covers the vocabulary/runtime references used by the planner and unit-layout system, including areas such as:

- unknown role tags;
- unknown army roles;
- unknown layout shapes;
- unknown formation intents;
- missing layout references;
- incomplete doctrine defaults; and
- non-positive/invalid spacing values.

The `content_validator` links the same production formation/data code, which reduces the risk of the validator accepting a format the runtime interprets differently.

# Testing

Formation behavior is covered at several levels.

| Area                             | Representative tests                                    |
| -------------------------------- | ------------------------------------------------------- |
| Unit-layout geometry/determinism | `tests/formation/unit_layout_test.cpp`                  |
| Army planner and doctrine roles  | `tests/formation/army_formation_planner_test.cpp`       |
| Registry lifecycle/persistence   | `tests/formation/army_formation_registry_test.cpp`      |
| Movement policies                | `tests/formation/formation_movement_test.cpp`           |
| Data overlay/validation          | `tests/formation/formation_data_loader_test.cpp`        |
| Terrain/navigation fitting       | `tests/formation/formation_terrain_navigation_test.cpp` |
| Cohesion and combat multiplier   | `tests/formation/formation_cohesion_test.cpp`           |
| Planner split/cache behavior     | `tests/formation/formation_planner_cache_test.cpp`      |
| Defensive unit layouts           | `tests/systems/defensive_unit_layout_test.cpp`          |
| Planner UI                       | `tests/ui/qml/tst_formation_panel.qml`                  |
| Status badge                     | `tests/ui/qml/tst_formation_status_badge.qml`           |
| Input/placement behavior         | `tests/core/input_command_handler_test.cpp`             |
| Key binding                      | `tests/ui/input_bindings_test.cpp`                      |

Arena scenarios prefixed with `unit_layout_` and `army_formation_` exercise the same runtime visually with real rendering and scenario commands.

# Debugging by layer

Formation bugs are easier to diagnose when the two formation layers are kept explicit.

## Soldiers wrong inside one troop

Inspect:

- troop formation profile;
- resolved `UnitLayoutId`;
- doctrine-qualified layout;
- layout query rows/files/count;
- traversal/defensive state; and
- soldier anchor precedence.

## Whole troop entities placed wrong in an army

Inspect:

- formation membership;
- doctrine/intent;
- role tags;
- `ArmyFormationOptions`;
- local planner layout; and
- terrain placement result.

## Formation looks right in preview but not after movement

Inspect:

- committed registry state;
- movement policy;
- movement-plan/replan state;
- blocked/adjusted slots;
- cohesion/phase; and
- route/traversal behavior.

## Defensive formation changes army grouping unexpectedly

That indicates a layer violation: Defence Mode should alter the internal unit layout and its modifiers, not create/replace the army-group identity.

## Soldier positions disagree between combat and rendering

Inspect `FormationCombat::soldier_spatial_anchors`, living-slot state, traversal state, and published presentation rather than adding another renderer-local offset rule.

# Architectural invariants

The current formation system depends on these invariants:

- unit layouts and army formations remain separate identities;
- internal soldier layouts are deterministic for stable inputs;
- living-slot identity is shared by simulation/presentation consumers;
- doctrine-specific layout resolution falls back to generic data;
- army roles are tag-driven rather than hard-coded troop IDs;
- committed group state belongs to `ArmyFormationRegistry`;
- terrain fitting reports `Valid`, `Adjusted`, or `Blocked` instead of silently stacking units;
- cohesion is measured from placed members;
- defensive layout and army cohesion modifiers compose rather than replace each other;
- constrained traversal remaps internal layout without taking over troop-root navigation; and
- AI/player placement use the same planner.

# Source map

| Concern                   | Source                                         |
| ------------------------- | ---------------------------------------------- |
| Unit layout types/system  | `game/formation/unit_layout.*`                 |
| Formation types/options   | `game/formation/army_formation_types.h`        |
| Army planner              | `game/formation/army_formation_planner.*`      |
| Registry/runtime/cohesion | `game/formation/army_formation_registry.*`     |
| Formation service         | `game/formation/army_formation_service.*`      |
| Formation data loader     | `game/formation/formation_data_loader.*`       |
| Defensive layout runtime  | `game/systems/defensive_unit_layout_service.*` |
| Traversal layout          | `game/systems/unit_traversal_layout_system.*`  |
| Movement facts            | `game/core/movement_facts.h`                   |
| Authored formation data   | `assets/data/formations/`                      |

The architecture documented here describes the current unit-layout, army-group, traversal, defensive-state, AI, persistence, and UI contracts. Historical implementation stories are not needed to understand those contracts and are deliberately kept out of the reference article.
