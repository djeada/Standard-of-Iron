# Formation Architecture

Standard of Iron has two formation layers because they solve two different problems. A **unit layout** places the soldiers inside one logical troop entity. An **army formation** places several troop entities relative to one another. They share data about doctrine and battlefield intent, but they do not share identity or ownership.

| Layer | Arranges | Primary owner | Identity |
| --- | --- | --- | --- |
| Unit layout | Soldiers inside one troop entity | `Game::Formation::UnitLayoutSystem` | `UnitLayoutId` |
| Army formation | Multiple troop entities | `ArmyFormationPlanner` and `ArmyFormationRegistry` | `FormationDoctrineId` + `ArmyFormationIntent` |

Keeping those layers separate lets a troop change its internal stance without leaving an army formation, and lets an army redeploy without redefining how every soldier stands.

## Independent formation state

Four concepts coexist at runtime:

- **Unit layout** controls the geometry of soldiers inside one troop entity.
- **Army formation** controls the geometry of multiple troop entities.
- **Combat stance** controls how a troop reacts to threats.
- **Order** controls what the troop is doing now: moving, attacking, guarding, patrolling, and so on.

A troop can therefore remain part of a Roman line while its soldiers form a defensive shield layout and the troop itself follows a movement or guard order.

## Unit layouts

The internal layout system is defined by `game/formation/unit_layout.h`. `UnitLayoutStyle` contains the geometry needed to build a soldier pattern: lateral and depth spacing, rank stagger and arc, file grouping, jitter, weapon clearance, minimum separation, column width, and shape-specific values.

The supported layout shapes are:

- `Ranks`
- `Wedge`
- `LooseOrder`
- `Column`
- `Cluster`
- `Circle`
- `Shell`
- `Arc`

`UnitLayoutSystem::offset()` resolves a `UnitLayoutQuery` into a local soldier offset and yaw. The query carries the layout id, row and column information, spacing, a deterministic seed, and the current formation transition ratio. `UnitLayoutSystem::compute()` exposes the same mechanism for a complete unit.

### Stable soldier identity

Internal layouts are deterministic. A soldier's placement is derived from stable query inputs rather than its current world position. This matters to rendering and combat because the same logical soldier must keep the same slot while a troop moves, turns, takes casualties, or changes presentation state.

`FormationCombat::living_slot_indices()` is the shared boundary for deciding which presentation slots are still occupied. Renderer and combat-side consumers use the same surviving-slot information instead of deriving independent rosters.

### Doctrine-specific layouts

Troop formation profiles name generic layouts such as close-order infantry, marching layouts, or defensive layouts. `UnitLayoutLibrary::resolve(doctrine, generic_name)` first looks for a doctrine-qualified variant and then falls back to the generic layout.

That gives the data layer room to express different silhouettes without changing code. Roman, Carthaginian, and Iron Sepulcher troops can all request the same broad role while resolving to different spacing, rank structure, arcs, shells, or file groupings.

Formation data is loaded from `assets/data/formations/` on top of built-in defaults. See `assets/data/formations/README.md` for the authoring schema.

## Army formations

Army-scale state lives in `game/formation/army_formation_types.h` and is owned by `ArmyFormationRegistry`.

The current formation intents are:

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

Intent answers **what shape of deployment is wanted**. Doctrine answers **how a faction expresses that intent**. The two are deliberately separate.

### Formation state

`ArmyFormation` stores the authoritative state for a committed group:

- group id;
- doctrine and intent;
- anchor and facing;
- frontage, depth, and spacing;
- formation options;
- phase and cohesion;
- member entity ids;
- placed slots;
- destination and movement-plan state;
- planning revision and replan flags.

Member entities carry a membership component that points back to the group. The registry remains the source of truth for group geometry and membership.

### Formation options

`ArmyFormationOptions` exposes the choices that can change a doctrine template without replacing it:

- flank preference: balanced, strong left, strong right, or split;
- movement policy: reform at destination or maintain formation;
- ranged placement: front, rear, or skirmish;
- mixed-doctrine policy: composite by role, separate contingents, commander doctrine, or majority doctrine;
- frontage, depth, and spacing scales;
- reserve rows;
- member-order preservation;
- doctrine locking.

The planner uses those options together with troop role tags and the active doctrine template to build a formation layout.

## Planning and terrain placement

`ArmyFormationPlanner` separates formation geometry from world placement.

`build_layout()` produces formation-local slots from members, doctrine, intent, and options. `place()` rotates that layout into world space and fits its slots to terrain. `plan()` combines the two operations.

This split is important for interactive placement: moving an otherwise unchanged preview does not require the role and template work to be rebuilt from scratch.

### Slot status

Every placed slot has one of three statuses defined in `army_formation_types.h`:

- `Valid` — the ideal position can be used;
- `Adjusted` — terrain fitting moved the slot to a usable nearby position;
- `Blocked` — no acceptable position was found.

The preview and committed formation use this placement result rather than assuming that an authored geometric pattern is legal everywhere on the map.

## Cohesion and combat

`ArmyFormationRuntime` updates committed formations and exposes their combat effect.

The current cohesion thresholds are defined in `army_formation_registry.cpp`:

- a slot counts as in position inside `1.35 × spacing`;
- cohesion at or above `0.8` is formed;
- cohesion at or below `0.45` is disrupted.

Formation phases are richer than a single formed/unformed flag. `army_formation_types.h` currently defines `Reforming`, `Formed`, `Disrupted`, `Opening`, `Traversing`, and `Arrived`.

The combat damage path combines defensive unit-layout effects with `ArmyFormationRuntime::damage_taken_multiplier()`. This keeps internal defensive stance and army-scale cohesion independent while allowing both to affect the same incoming damage calculation.

## Movement policies

Two movement policies are part of the public formation contract.

**Reform at destination** lets troop entities route independently to the new deployment and form the requested shape at the destination.

**Maintain formation** keeps a moving group under formation runtime control while the formation center advances. Route-follow speed applies `ArmyFormationRuntime::move_speed_multiplier()` alongside other movement modifiers.

Neither policy turns individual soldiers into pathfinding agents. Navigation remains attached to logical troop entities; soldier positions are presentation and combat geometry inside those entities.

## Constrained routes

Narrow crossings add an internal traversal-layout layer rather than replacing the authored unit layout.

`UnitTraversalLayoutSystem` publishes `TraversalLayoutFacts` into movement state. Those facts include the current and target traversal mode, the relevant corridor/portal information, chosen and normal file counts, spacing, and lateral scaling. Movement and presentation can therefore agree on whether a block is using its normal frontage or temporarily closing ranks to pass through constrained ground.

The precedence remains:

1. army formation and route choose the troop root's strategic destination;
2. normal or defensive unit layout chooses the troop's base soldier arrangement;
3. traversal layout may temporarily remap that arrangement for constrained movement;
4. combat/contact presentation composes action and hit state on the resulting soldier anchors;
5. the renderer consumes the published result rather than running another position simulation.

`FormationCombat::soldier_spatial_anchors` is the shared soldier-level spatial query boundary for systems that need exact internal positions.

## Defence Mode

Defence Mode is an internal unit-layout state, not an army formation.

`DefensiveUnitLayoutService` reads the active unit and nation profile and exposes the movement, turning, charge, targeting, and damage rules associated with that defensive state. The service is used by movement, combat, route following, and damage application.

`UnitLayoutStateComponent` carries the transition state, while nation data selects the eligible troop types and defensive layout. A unit can therefore form its defensive soldier geometry without receiving a new army group id or changing the other troop entities around it.

The two shipped visual families are intentionally distinct: Roman defensive infantry uses the closed shell/testudo family, while Carthaginian defensive infantry uses an open shield-wall family. Those differences are expressed through formation data and baked presentation assets rather than by a separate army-formation type.

## Player placement

The player-facing formation planner uses the same army planner and slot fitting as committed gameplay.

The control surface exposes the current intent catalogue and formation options. A placement preview is a troop-footprint plan, not a cloud of individual soldier markers. Ground placement sets the anchor; orientation sets the facing; the planner reports resulting dimensions and blocked or adjusted slots before the order is committed.

A single troop may still be positioned and faced through the placement gesture, but `ArmyFormationService` does not need to create a one-member army group for that operation. Army formation state starts where there is actually a group to arrange.

`FormationStatusBadge` is the persistent view of committed group state. It reports the selected group's intent, doctrine, phase, and cohesion after placement mode has ended.

## AI formations

AI and player armies share the same formation planner.

`Game::Systems::AI::plan_ai_formation` builds planner members from an AI snapshot and resolves a doctrine and intent through the normal formation machinery. AI formation choices are therefore subject to the same role, doctrine, terrain, and availability rules as player formations rather than using a second geometry implementation.

AI settlement and muster logic may choose a station and an intent on its own cadence, but the resulting troop placement still goes through the shared formation service.

## Save/load

Army formations are persistent game state. The registry serializes committed groups with their doctrine, intent, anchor, options, members, slots, movement state, and cohesion-related data so a restored battle does not have to infer formations from troop positions.

Internal unit-layout transition state is also serialized as component state. Save/load therefore restores both formation layers rather than reconstructing either from rendering output.

## Validation

Formation content is validated against the same vocabulary the runtime loads. Validation covers unknown layout shapes, role tags, army roles, intents, invalid spacing, references to missing layouts, and incomplete doctrine definitions.

The main automated coverage includes:

| Area | Representative tests |
| --- | --- |
| Unit-layout geometry | `tests/formation/unit_layout_test.cpp` |
| Army planning and terrain fitting | `tests/formation/army_formation_planner_test.cpp` |
| Registry lifecycle and persistence | `tests/formation/army_formation_registry_test.cpp` |
| Movement policies | `tests/formation/formation_movement_test.cpp` |
| Formation data loading | `tests/formation/formation_data_loader_test.cpp` |
| Terrain/navigation fitting | `tests/formation/formation_terrain_navigation_test.cpp` |
| Cohesion and damage | `tests/formation/formation_cohesion_test.cpp` |
| Defensive layouts | `tests/systems/defensive_unit_layout_test.cpp` |
| Formation UI | `tests/ui/qml/tst_formation_panel.qml`, `tst_formation_status_badge.qml` |
| Input integration | `tests/core/input_command_handler_test.cpp`, `tests/ui/input_bindings_test.cpp` |

Arena scenarios prefixed with `unit_layout_` and `army_formation_` exercise the same systems visually. Promotional formation scenarios also drive the planner through the arena tooling; see [PROMO_CAPTURE.md](PROMO_CAPTURE.md) for that capture pipeline.