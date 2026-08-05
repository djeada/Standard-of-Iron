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

- **Rome — the drilled lattice.** Broad frontage, few ranks, evenly spaced
  files, and every rank offset half a file from the one in front
  (`rank_stagger` 0.5), so the block reads as an interlocking quincunx instead
  of a plain grid. Zero echelon, zero rank arc, near-zero jitter: the texture
  comes from the lattice, not from noise. Bracing drops the stagger and the
  ranks lock square. Testudo is a `shell` shape, the tightest block in the game.
- **Carthage — the knotted crescent.** Narrower frontage over more ranks, so a
  Carthaginian unit is a compact mass where a Roman one is a line. Files are
  gathered into knots of two to four men (`file_grouping` / `group_gap`) with
  visible seams between them and a small per-knot depth wobble, and every rank
  bows forward at the centre (`rank_arc`). Positional and facing variation sit
  well above Rome's but stay under `min_separation_scale`, so the host looks
  gathered rather than scattered. Its defensive layout is an `arc` shield line,
  not a testudo.
- **Iron Sepulcher — the compressed slab.** The tightest of the three in both
  axes with almost no variation and a procession-like column. Solemn rather
  than chaotic; the distinctness comes from compression, not demonic motifs.

Nation data reinforces the same split before geometry is applied at all:
`max_units_per_row` gives Roman troops a wide, shallow frontage and
Carthaginian troops a narrow, deep one, so the two silhouettes differ even at
identical soldier counts.

`tests/formation/unit_layout_test.cpp` locks this in with shape metrics rather
than eyeballing: per-rank curvature, file-gap uniformity, the quincunx offset
between ranks and mean facing deviation must stay separated for every paired
role, and `formation_data_loader_test.cpp` re-checks the separation after the
shipped JSON overlays are applied so a data edit cannot quietly erase it.

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
save/load with its doctrine, intent, anchor, options, cohesion and slot
assignment intact.

### Cohesion is measured, not assumed

`ArmyFormation::phase` used to be set from the last order — a group was
`Formed` because a move had finished, not because anyone checked where the
units were standing. `ArmyFormationRuntime::refresh_cohesion` now measures it
every 0.35 s: `cohesion` is the fraction of placeable, occupied slots whose
occupant is within `1.35 × spacing` of the slot it was assigned. That fraction
picks the phase — `Formed` at 0.8 and above, `Disrupted` at 0.45 and below,
`Forming` in between.

The measurement is what makes the phase worth having, because the phase feeds
combat. `ArmyFormationRuntime::damage_taken_multiplier` is applied in
`damage_application.cpp` next to the defensive-formation multiplier:

- **Formed** — damage taken scales from 1.0 at cohesion 0.8 down to 0.88 at
  cohesion 1.0, so closing the last gaps in a line is worth something.
- **Disrupted** — damage taken is 1.08.
- **Forming**, or no formation at all — 1.0.

The band is deliberately narrow. Holding a line should reward good play without
deciding the fight on its own, and the multiplier composes with everything else
in the damage pipeline rather than short-circuiting it.

`formation_cohesion_test.cpp` covers this end to end, including the two wiring
claims that are easy to get wrong: that the runtime's own `update()` performs
the measurement without being asked, and that `apply_unit_damage` — the
function combat actually calls — really does deal less damage to a formed line
than to a disrupted one.

### Terrain fitting

`SlotTerrainFitter` searches outward from each slot's ideal position
independently, in rings, and refuses any candidate already claimed by another
slot. A slot that finds nothing after six rings is marked `Blocked` rather than
being dropped onto a shared fallback point.

This is the fix for the old behaviour, where every unwalkable slot resolved to
the same `resolved_center` and several units stacked on one tile. Slots come
back tagged `Valid`, `Adjusted` (nudged) or `Blocked` (no room), and the
placement preview colours them accordingly before the player commits.

### Planning is split so dragging is cheap

`ArmyFormationPlanner::plan` is two halves, and callers that move only the
anchor can skip the first:

- `build_layout` — slot offsets in formation-local space. A pure function of
  the members, the doctrine template and the options. It carries a
  `signature` hashing exactly those inputs.
- `place` — rotation, terrain fitting and slot status. This is the half that
  depends on the anchor and the facing.

`plan()` is still `place(build_layout(...))` and returns exactly what it always
did; `formation_planner_cache_test.cpp` asserts that equivalence slot by slot,
and that the signature moves for every input the layout reads while staying put
for the anchor and the facing.

`CommandController` uses the split during placement: it collects members once
per session, rebuilds the layout only when the signature changes, and skips the
refresh entirely when the anchor has moved less than 0.05 world units and the
facing less than 0.25°. Before this, every mouse-move event ran a full replan.

Slot fitting itself was quadratic — `SlotTerrainFitter` linearly scanned every
already-claimed position for each of up to 72 candidate points per slot.
Claimed positions now live in a hash grid keyed on the minimum separation, so a
free-space query only touches the nine cells that could hold a conflict.
Measured on this machine, per-plan cost went from 28.1 µs to 16.3 µs at 20
units, 251 µs to 115 µs at 80, and 464 µs to 191 µs at 160 — the advantage
grows with army size, which is what a quadratic-to-linear change should look
like. `NoTwoSlotsLandOnTopOfEachOther` guards the separation floor the grid has
to keep enforcing.

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

- The whole intent catalogue, in a fixed order, each carrying the reason it is
  unavailable to the current selection when it is
  (`formation_intent_unavailable_reason`).
- Drag placement: drag length sets frontage, drag direction sets facing. Wheel
  turns the deployment, `Ctrl`+wheel changes depth, left-click deploys, and
  right-click or `Esc` cancels and releases the group.
- Right-clicking the ground with two or more troops selected: the press opens
  the planner at that point, dragging away from it aims the block, and the
  release deploys.
- A fine-tuning section that scales the faction template rather than replacing
  it — frontage, depth, spacing, cavalry wing, missile placement, reserve rows,
  slot order, movement policy, mixed-army doctrine policy, and an explicit
  doctrine override.

The preview is planned with `ArmyFormationPlanner::plan` and rendered as unit
footprints, never as individual soldiers.

### Positioning troops is what opens the planner

Ordering two or more troops to a spot is already a formation decision, so the
right-click that gives that order is what raises the planner — there is no
command-bar button to press first. `InputCommandHandler::on_right_press` calls
`begin_move_placement_at_position`, which starts placement only when the
selection holds **at least two** troops and otherwise reports back that it did
nothing, so a single unit takes the plain move order it always did rather than
being asked to pick a deployment for itself. `on_right_release` issues that
ordinary move whenever the press did not consume the gesture.

`rts.order_formation` (`F`) remains a real entry in the binding catalogue and
still opens the planner around the current selection, which is what
`TheFormationOrderOwnsTheKeyItIsDocumentedWith` checks. The command bar used to
carry a Formation button whose letter was written next to the label rather than
read from the catalogue; the button is gone, and with it the drift.

### The arrow is the facing, not an offset from it

`m_formation_facing_degrees` is the yaw the formation will actually be committed
with — one number that the preview request, the commit request and the on-ground
arrow all read. Until the player aims the block themselves it tracks
`ArmyFormationService::auto_facing`, so a formation the player only positions
faces the way the troops are marching; a drag, a wheel turn or a right-drag sets
it explicitly and the anchor moving no longer overrides that choice.

It used to be an offset that the planner added to `auto_facing` while the
renderer drew it raw, so the arrow pointed one way and the deployed line faced
another — off by however far the units had to march. `on_right_drag_orient` fed
it a world angle, which meant the error was the whole auto-facing term.
`ThePlacementArrowPointsWhereTheUnitsEndUpFacing` pins the arrow, the previewed
plan and the committed `desired_yaw` to the same angle.

Nothing is set by a modifier held during a click any more. `Ctrl`, `Shift` and
`Alt` on mouse-down used to set tight spacing, preserve order and cycle the
strong flank, and none of them ever set the value back: one `Ctrl`-click tightened
every formation for the rest of the session, and `Alt` advanced the flank one step
on each click with nothing on screen saying so. Those are ordinary controls in
the fine-tuning section now, where their state is visible and reversible.

`formation_intents()` returns the whole catalogue rather than only what the
doctrine can field. The number keys pick the nth entry, so filtering the list
made a digit mean a different formation from one selection to the next. Entries
the selection cannot field are shown blocked, carrying the reason, which also
teaches the player what a formation needs instead of hiding it.
`FormationSlotsKeepTheirMeaningAcrossSelections` locks the ordering down.

### Choosing a formation is a picture, not a word

Each choice is a card with a seven-by-four dot silhouette of the deployment it
produces (`FormationShape`), its name, its hotkey and — for whichever card the
pointer is over — one line on what it is for. A player who has never opened the
panel can see that Column is narrow and deep and that Line is wide and shallow
without reading anything. The patterns are a lookup keyed on the intent id, and
`tst_formation_panel.qml` asserts every intent has one and that no two are the
same, because two identical silhouettes would be worse than none.

### The panel avoids QtQuick.Layouts on purpose

`FormationPanel` is built from plain `Column`/`Row`/`Grid` positioners inside a
`Flickable`. It used to be nested `ColumnLayout`s and `RowLayout`s anchored with
`anchors.fill: parent` to a card that was itself sized from those layouts'
implicit size. Showing the panel re-entered `QQuickLayout::rearrange` through
`updatePolish`, and the grid engine walked items that the re-entrant pass had
already freed — a hard crash in `qmlAttachedPropertiesObject` every time the
player asked for a formation. The panel has a fixed content width, so a layout
engine bought nothing to begin with.

Two rules fall out of that and apply to any HUD panel sized from its own
content: do not anchor a layout to a parent whose size comes from that layout,
and do not give a `Repeater` inside a layout a model that is a JS array rebuilt
from live properties — the delegates are destroyed and recreated underneath the
layout pass. `FormationShape` iterates a constant cell count for the same
reason. `test_showing_a_populated_planner_survives_layout` brings the populated
panel up and expands it, which is all the old structure needed to die.

The HUD hands the panel the vertical band it has free between the wave tracker
and the command bar (`max_height`); the panel scrolls inside it rather than
growing off the top of the screen when the fine tuning is expanded.

### The panel reports state, not just accepts input

Every advanced control is a two-way binding. `formation_options()` returns a
`*_index` for each dropdown derived from the live options, so reopening the
panel, hitting **Reset to faction default**, or changing depth with the wheel
all move the controls the player is looking at. Before this the dropdowns were
write-only: they always rendered their first entry no matter what the group was
actually set to, and the three whose display order did not match the enum
reported a different value than the one in force. The dropdown order now
follows the enum so the index is the value.

`FormationPanel` also shows the plan as numbers while the preview is on the
ground — ranks × files, frontage and depth in metres, and placed-of-total slots
coloured by whether anything is blocked — plus a separate line for slots that
were merely nudged, which is not a warning and should not read as one.

Ranks and files come from `ArmyFormationPlan::depth_bands`, which buckets every
slot in the plan by local depth. The per-slot `rank` and `file` fields restart
at zero for each line rule, so reading the maximum of those would report the
deepest single line rather than the depth of the whole block — a three-line
template would have claimed to be two ranks deep. Banding the finished plan is
what makes a Column read as deeper and narrower than a Line, which is the
assertion `RankAndFileCountsDescribeTheWholeBlock` makes. Number keys 1–9 pick
the nth intent in the catalogue, matching the card order; the binding lives in
`GameView`'s key handler with the rest of the hotkeys rather than the panel
stealing focus.

The doctrine override is built from `DoctrineRegistry::ids()` rather than a
hardcoded faction list, so a new doctrine appears in the dropdown without a UI
edit — the old list silently reported "Automatic" for anything it did not know
about.

`FormationStatusBadge` is the piece that outlives placement. Selecting units
that belong to a committed group shows the intent, the doctrine, the phase and
a cohesion bar, so the player can tell a line that formed up from one that came
apart without re-entering placement mode. It samples cohesion on a timer
because the simulation measures it on its own cadence rather than pushing a
signal. Phase is carried by both colour and text, which
`tst_formation_status_badge.qml` locks in along with the fallback for an
unknown phase.

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

| File                                                    | Covers                                                                                                |
| ------------------------------------------------------- | ----------------------------------------------------------------------------------------------------- |
| `tests/formation/unit_layout_test.cpp`                  | determinism, faction distinctness, min separation, role and stance layouts, low counts                |
| `tests/formation/army_formation_planner_test.cpp`       | role placement, doctrine resolution, mixed-army policies, intent availability, terrain fitting        |
| `tests/formation/army_formation_registry_test.cpp`      | group lifecycle, membership, slot retention, save/load                                                |
| `tests/formation/formation_movement_test.cpp`           | both movement policies, speed multiplier, no slot stacking                                            |
| `tests/formation/formation_data_loader_test.cpp`        | overlay semantics, every validation failure mode, and that the shipped data keeps the factions apart  |
| `tests/formation/formation_terrain_navigation_test.cpp` | slots against rivers, bridges, hills, walls and buildings; reachability; no stacking                  |
| `tests/formation/formation_cohesion_test.cpp`           | measured cohesion, phase thresholds, the damage multiplier through `apply_unit_damage`, save/load     |
| `tests/formation/formation_planner_cache_test.cpp`      | layout/place equivalence, layout signature, slot separation under the hash grid, per-plan cost report |
| `tests/ui/qml/tst_formation_status_badge.qml`           | the persistent badge: phase labels, distinct phase colours, unknown-phase fallback                    |
| `tests/ui/qml/tst_formation_panel.qml`                  | the planner survives being shown and expanded, one silhouette per intent, the band the HUD gives it   |
| `tests/core/input_command_handler_test.cpp`             | who opens the planner, arrow and committed facing agree, slots keep their meaning across selections   |
| `tests/ui/input_bindings_test.cpp`                      | the formation order owns the key it is documented with                                                |

These are headless and run in `simulation_tests`. The terrain suite is a real
test rather than a tautology: disabling `resolve_terrain` makes four of its
cases fail, which is the check that it is actually exercising the fitter.

The Arena scenarios prefixed `unit_layout_` and `army_formation_` cover the
visual side and need a real GPU context; `tests/tools/arena_scenarios_test.cpp`
asserts headlessly that each one exists, spawns groups, carries expectations,
issues a `FormationMove`, and — for the terrain fixtures — actually declares the
rivers, bridges, elevation patches, walls and buildings it claims to test.

## Showing it off

The three `promo_` formation scenarios are army-scale versions of the same
material, authored for video rather than for acceptance: one faction, several
hundred soldiers, and a scripted cycle through the intents. They drive the
planner through `ScenarioCommandKind::FormArmy` — `FormationMove` only
translates a shape that already exists — and the same test file asserts that
they field an army, name real groups, and show at least three intents. See
[PROMO_CAPTURE.md](PROMO_CAPTURE.md).
