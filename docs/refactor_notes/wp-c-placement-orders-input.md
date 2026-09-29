# WP-C: placement, orders and input (issue #1559, PR 4)

Files: `app/economy/production_manager.*`, `app/orders/command_controller.*`,
`app/orders/army_formation_controller.*`, `app/input/input_command_handler.*`.
Behaviour is unchanged; this note records why the seams sit where they do.

## Placement (app/economy)

`ProductionManager` is now a coordinator that turns pointer gestures into three
collaborators plus the order path:

| Owner                           | State it owns                                                                                                                         |
| ------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------- |
| `PlacementSession`              | pending type, builders, owner and nation, position, preview rotation, harvest and food targets                                        |
| `WallPlacementSession`          | drag flag, anchor, wall rotation, plan request, planned segments; does the wall snapping and planning                                 |
| `ConstructionPreview` (QObject) | preview entity ids, active/valid/reason flags, segment summary; owns entity creation, cleanup and the hand-off to a construction site |
| `construction_pointer`          | pointer to world hit (ground, surface, harvest target, food target) and site search; no state                                         |
| `placement_refusal`             | `PlacementRuling` / wall faults / affordability to localized text, so enums become text only at the point a signal is emitted         |
| `placement_orders`              | the four `OrderRequest` shapes (build, wall plan, direct building); the harvest request is `App::Orders::harvest_order`               |

`ConstructionPreview` signals are forwarded to `ProductionManager` with
signal-to-signal connections, which are synchronous, so QML sees the same
emission order as before for each property.

### State machine

`placement_phase(placing, wall_drag_active, preview_active)` derives one of
`Idle`, `Aiming`, `Previewing`, `DraggingWall`. A live preview wins over a
drag, so `DraggingWall` means "drag started, plan still empty". Legal events
(`placement_allows`):

| Event                                 | Allowed when                                                     |
| ------------------------------------- | ---------------------------------------------------------------- |
| Start, MatchReset, Confirm            | always (Confirm while idle degrades to cancel, which is a no-op) |
| PointerMotion, PointerRelease, Cancel | not idle                                                         |
| PointerPress                          | not idle and the item is a wall or gate                          |
| Rotate                                | `Previewing` only                                                |

All three end-of-placement outcomes (cancel, confirm, harvest hand-off,
builder site hand-off) run through `ProductionManager::end_placement`, which
replaces five hand-copied reset blocks. The differences between the old
blocks were invisible: the harvest paths skipped wall and summary resets
that were already at their idle values.

### Invalid between preview and execution

Confirm re-derives everything from the world (the food target still exists and
is unclaimed, the harvest target is not taken, ground and affordability rule
again, the builder list is non-empty). Every refusal goes through
`reject` / `reject_and_invalidate`, which emit
`construction_placement_rejected` once and keep the placement armed. A command
that passes those checks but is refused by `submit_player_order` still reports
through `order_feedback` (the single accepted/rejected path is `submit_order`)
and ends the placement, exactly as before.

### Found, not changed

`start_builder_construction` computes the placement nation after the owner is
already set, so `pending_construction_nation_id` returns the stale reset value
(Roman Republic) for builder-started previews. The extraction keeps that
ordering (`PlacementSession::begin_with_builders`) because fixing it changes
what players see. Worth a follow-up with a rendering check.

## Orders (app/orders)

`CommandController` keeps the QML/engine contract (same methods, signals and
`void*` camera arguments) and routes to handlers grouped by intent, each
holding only `World`, `SelectionService`, an optional `PickingService` and the
shared `OrderIssuer`:

- `MovementCommands` (attack click/press, move-or-attack, minimap, unreachable)
- `ModeToggleCommands` (stop, hold, guard, guard click, gate, run; QObject that
  emits the mode signals, forwarded by the controller)
- `WorkerCommands` (auto-gather, food harvest, delivery, repair, dismantle)
- `RosterCommands` (recruit, divide, merge squads)
- `PatrolCommands` (owns the first waypoint)

Shared pieces: `PointerTarget` (screen point, viewport, camera),
`CommandResult` (moved to `command_result.h`), `submit_local_command`,
`worker_orders` (harvest request, builder crew, nearest-first ordering, used by
both the placement coordinator and `WorkerCommands`), `order_cues` (mount-aware
charge and move cue selection; `CommandController::charge_cue` and friends are
thin forwarders because the engine and tests call them).

Hold and guard share `tally_mode<Component>`; the three pointer worker orders
share `pointer_order`.

`CommandController::set_rally_at_screen` had no caller in `app/` or `tests/`
(the live path is `ProductionManager::set_rally_at_screen`) and is deleted.

## Formation (app/orders)

`ArmyFormationController` keeps its Q_INVOKABLE surface and signals. Its state is
split by lifetime:

- `FormationOptionsModel`: intent, options, doctrine override, preset scales and
  indices. Setters report whether they applied so the controller only refreshes
  when something changed.
- `FormationPlacement`: the placement gesture (units, position, facing, aim,
  frontage, drag) and its geometry.
- `FormationPreviewCache`: plan, members, layout and the "is the cached plan
  still right" test.
- `formation_readout`: typed `FormationOptionsReadout` and
  `SelectedFormationStatus`, converted to `QVariantMap` once at the QML
  boundary.

Translated strings stay in the controller (`tr()` context
`ArmyFormationController` is what the `.ts` files carry).

Two carried-over quirks: `on_formation_command` stores the troop list even when
none of them has a transform (no placement starts); rejected deployments cancel
placement before reporting, using a by-value anchor because cancellation zeroes
the placement position.

## Input (app/input)

- `viewport_state.h`: `ViewportState` alone (still reachable through
  `input_command_handler.h`).
- `ContextInteractionResolver`: builds the interaction request from the
  selection and hover, returns `ContextInteraction`.
- `FormationPointerAdapter`: ground-picking for formation move, drag, scroll,
  right-drag aim and confirm/cancel.
- `InputCommandHandler` stays the pointer router; `issue_command` /
  `issue_camera_command` replace fifteen copies of the spectator, null and
  cursor-reset preamble. The unguarded `m_cursor_manager->mode()` read in
  right-click handling is preserved.

## ClientContext audit

None of the four files reference `ClientContext`. Every extracted collaborator
takes its own narrow pointers or references (see the list above); none holds a
pointer back to its owner and none is a friend of the old class.
