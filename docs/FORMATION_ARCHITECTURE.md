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
- `MaintainFormation`;
- `DoctrineDefault` — the option's default: the chosen template's `default_movement` decides.

A player (or the AI) who picks a policy overrides the template; otherwise choosing
Column alone gives the doctrine's march policy (`maintain_formation` for every
shipped column). The planner resolves the effective policy into
`ArmyFormationPlan::movement_policy`, the commit stores the resolved value on the
group, and the formation panel shows it next to "Doctrine decides".

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

`plan()` is the combined convenience path: `build_layout()` followed by
`fit_to_ground()` (see [Terrain fitting](#terrain-fitting)). The placement preview
runs the same `fit_to_ground()` on its cached layout, so the preview is the plan
the commit will produce.

This split is important for interactive placement. Dragging a preview across the ground can reuse the same role/template layout while changing only world-space placement and terrain fitting.

## Layout signatures and preview reuse

The planner exposes a layout signature for inputs that affect local slot geometry.

Anchor and facing affect placement, not the local role/template layout itself. A placement UI can therefore avoid repeating expensive role/layout work when only the mouse position or facing changes.

The cache tests verify that the split path (`build_layout()` + `fit_to_ground()`)
and full `plan()` path remain equivalent.

## Footprints and separation

A slot is sized for the whole troop standing in it: `FormationSlot::half_width`
and `half_depth` are the troop's measured (and intent-shaped) extents, and every
slot of a plan shares the plan's facing. Two troops therefore overlap exactly when
their centre offset, projected on the formation's lateral and depth axes, is inside
the summed half extents plus the plan's `footprint_gap` (half a lane,
`0.45 × spacing`, at least `0.225 m`).

`build_layout()` guarantees disjoint rectangles in two steps. A pairwise
relaxation pushes overlapping neighbours apart, and because relaxation can stop
short in a dense block, a front-to-back sweep then moves any slot that still
touches an earlier one directly behind it. Slots only ever move rearward in the
sweep, so rank order is kept and the pass always terminates. The same pass runs
again after previous slot ids are restored (see
[Slot identity](#slot-identity)), so no reshuffle can reopen an overlap.
`ArmyFormationPlanner::first_overlap()` checks a placed plan on world positions.

## Slot identity

Slot ids are the group's stable ordering. When a group is replanned with
`preserve_previous_slots`, a troop takes back its previous id only by swapping
with a troop of the same kind (troop type, head count, passability): a slot was
laid out for its original occupant's line and size, and any other swap would move
a troop into a line it does not belong to. Measured extents are deliberately not
part of that key — they drift while a unit reshapes on the march — and the
separation pass absorbs the difference.

A group that holds a formation keeps its committed shape as
`ArmyFormation::reference_slots`. While the membership is unchanged, every replan
re-places that reference (rotated and translated to the moving anchor) instead of
re-deriving a layout from the troops' current positions, so ranks and files never
swap on the march or in a turn. Losing a member invalidates the reference; the
next plan is built fresh for the survivors and becomes the new reference.

## Silhouettes

What a formation looks like is fixed by the pictograms of the formation panel
(`FormationPanel.qml`): Line is two full rows, Column three files deep, the
faction default 5/5/3, Defensive two rows and a reserve behind a gap, Assault a
wedge behind its skirmishers, Encirclement a horseshoe, Siege Escort two rows
and the engines behind. The doctrine template decides who stands where; a final
pass (`regularize_silhouette`) then lays the troops out on clean rows with uniform
gaps (1.6 m between troops, 2 m between ranks, scaled by the spacing option).

- Roles form tiers in the template's front-to-back order. The fighting core
  (centre, screen, vanguard) takes the pictogram's rows; ranged, siege, command
  and reserve troops get rows of their own, so they are never level with the
  line in front of them. When a rear tier exists, it is the pictogram's short
  last row, and the core takes only the full rows. Skirmishers stand a clear rank
  ahead.
- Cavalry wings stand at the ends of the front rank (one rank forward for an
  encirclement).
- A dragged frontage is the span between the outermost troop centres; rows then
  hold as many troops as fit, with gaps stretched to that width. Without one,
  rows follow the pictogram, the frontage and depth options, and the template's
  `max_frontage` (counting the wings) and `max_depth` (a column too deep gains a
  file).
- Troop sizes are measured, not estimated: `layout_reach_for_files()` returns the
  real soldier extents for every file count a template may ask for, so a
  reshaped troop (a column's deep blocks, riders who keep their own shape) is
  planned at the size it will actually have.

## Slot assignment for new orders

A fresh order assigns troops to slots at world placement time
(`ArmyFormationPlanner::place`), among troops of one kind (type, head count,
passability), by the minimum total travel distance (Hungarian assignment). Straight
paths of a minimum-total-distance assignment never cross, so the army does not
braid through itself on the way. Runtime replans keep their stable ids instead
(`preserve_previous_slots`).

## Terrain fitting

A perfect geometric formation may not fit the world at its ideal coordinates.

`SlotTerrainFitter` resolves each desired slot against walkable terrain and separation constraints.

A candidate position is accepted only when:

- its whole troop rectangle is free of every rectangle already claimed;
- the core of the rectangle (the inner half in each axis) is passable for the
  troop's class — heavy troops (`can_enter_forest == false`) test with
  `Passability::Heavy`. The outer soldiers of a unit bend around a tree or a wall
  corner on their own (the unit layout does that), the core must stand on
  passable ground; and
- it lies in the same connected navigation region as the formation anchor, so a
  slot is never offered across a river the troop cannot reach.

A slot that fails at its ideal position is searched for within about one troop's
reach (`max(slot spacing, 2 × larger half extent + gap)`), rearward first and then
fanning out to the flanks, so a displaced troop stays behind its own file. Beyond
that reach the slot is `Blocked` rather than flung across the map.

`fit_to_ground()` then treats the shape as a whole. A plan *keeps its shape* when
at most 12% of its troops are displaced (always at least one, so a small group is
not compressed over a single boulder) and no more than 12% are blocked. If the
first placement does not keep its shape:

1. the whole shape slides — back off a map edge or a river bank by up to half its
   depth, or sideways by a quarter of its frontage (never for a group marching
   along a corridor, whose anchor is its progress along the route); then
2. the frontage narrows (×0.62 per attempt, up to three times), deepening the
   files. The plan records `narrowed`.

The attempt with the least displacement wins (blocked troops weigh four times an
adjusted one, plus the distance troops were moved and how far the anchor slid).

Every slot is classified as:

- `Valid` — ideal position is usable;
- `Adjusted` — a nearby valid position was found; or
- `Blocked` — no acceptable placement was found.

The result belongs to the plan and can be shown before the player commits the order.

## Slot fitting behavior

Terrain fitting searches outward from the ideal slot position and never lets two troop footprints overlap.

The planner therefore preserves two separate facts:

- **formation intent** — where the doctrine/template wanted the troop;
- **placement result** — where terrain allowed it to stand.

An adjusted slot is not necessarily an error. It means the shape can still be realized with a local nudge. A blocked slot means the planner could not place that member under the current rules.

## Rejected plans

A plan that cannot be fielded (the doctrine lacks the intent, the selection lacks
the required roles, or nothing fits) is reported, never substituted:

- `ArmyFormationService::preview()` / `commit()` return `valid = false` with the
  reason, every slot `Blocked` and every position on the anchor;
- the placement controller refuses to deploy an invalid preview and shows the
  reason;
- `DeployFormation` rejects an invalid plan without issuing movement orders;
- a plain group move whose preview is invalid falls back to the same
  shape-preserving move; and
- the AI's `placements_for()` falls back to the doctrine's own default line, and
  if even that fails returns every placement `Blocked`.

No path produces the old unrelated scatter grid.

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
- formed travel requires all placeable members in their slots and facing within
  4° of their assigned direction;
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

Both policies end the same way: every troop stands on the slot the placement
preview showed, facing the ordered way. They differ in how the group gets there.

### Reform at destination (fastest)

Each troop goes straight to its final slot. The order is *synchronised*: every
troop walks its own route at the pace that makes it arrive together with the
slowest-arriving troop (`MoveOptions::synchronize_arrival`; paces come from the
routes' real lengths once they are assigned, and never drop below 30% of a
troop's speed). On open ground this reads as the army flowing into the shape.

### Maintain formation ("Hold the shape")

On open ground the group moves as one body: a **frame morph**
(`ArmyFormation::morph`). The formation's frame — anchor and facing — is
interpolated from where the troops stand to the ordered place and facing, and
every troop's slot is that frame plus its local offset; if the old and new shapes
differ, the local offsets are interpolated too. Slots start exactly under their
troops, so nobody is dragged, and the duration is set so that no slot moves
faster than 85% of its troop's speed (`move_speed_multiplier()` is 1 during a
morph, so a troop that falls behind may catch up). A formed group that receives
a new order keeps every troop at the same place within its shape
(`keep_places_in_shape`), so a march or a wheel never reshuffles the ranks and the
motion is rigid (`FormationMorph::rigid`, phase `Traversing`; a morph that changes
the shape reads as `Reforming`). A turn of more than 135° does not swing the whole
shape round: the troops about-face in place instead.

Before a morph starts, every troop's path is sampled; if any would cross ground it
cannot stand on (a river, a wall, a settlement), the ground will not carry the
shape as one body, and the group falls back to the synchronised per-troop move of
*Reform at destination*, assembling on arrival. The old anchor march (a moving
anchor dragging replanned slots behind it, compressing at bridges) stalled on real
maps and is no longer started.

**Slot following.** A clear route uses its exact endpoint rather than intermediate
grid-cell centres. A walkable moving anchor keeps its continuous position instead
of snapping the entire shape to the grid. Runtime dispatch updates each member's
short route to its slot, preserving the movement order and velocity; it does not
fit a second shared lane through the group centroid. Members within 3 m of their
slots share the group's facing even when their correction is lateral or backward.
Longer assembly moves and obstructed paths still use normal navigation. The
movement component's `following_formation_slot` flag is transient, reset by a new
order and re-established by runtime dispatch.

### Routes of shaped orders

A shaped order (a deployment or a group move) first tries each troop's own route
to its slot, and keeps it when it is close to direct (at most 1.35× the straight
line plus 2 m): scattered trees or a boulder should not make the whole army
converge on one corridor, bunch at its mouth and loop back to the slots. Real
detours (a bridge, a gate) still use the group's shared corridor with one lane per
troop. The corridor is planned for the toughest member: one troop that cannot
enter forest makes the route use `Passability::Heavy`, and the widest navigation
body sets the clearance. Formation orders arrive precisely (`precise_arrival`), so
the finished shape lands within centimetres of the preview.

Known limit: troops that must change places (the doctrine puts the spears in front
of swordsmen who start ahead of them) still pass through each other's blocks in
transit; local avoidance steers by core radius and does not treat a troop as its
whole block. `FormationUxLab` (`SOI_UX_LAB=1`) measures this together with time to
form, stalls, heading travel and final error, on open ground and on legs across
obstacles found automatically on the shipped maps.

A deployment also sets each troop's file count (`formation_files_override`), which
changes its internal rank/file layout at once. The humanoid renderer eases that
change: when a soldier's local offset jumps by more than 0.25 m in one frame, the
drawn soldier walks to the new offset at 2.5 m/s (`ease_soldier_offsets` in
`render/humanoid/runtime/instance_prepare.cpp`). Smaller per-frame changes are
tracked exactly, so bodies stay on their selection rings while formed. Changes
beyond 12 m, a new soldier count, or an about-face still snap. Without this, every
soldier of a reshaped troop jumped 1–4 m in the frame after the order.

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
