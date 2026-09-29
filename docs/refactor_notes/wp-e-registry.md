# WP-E: army_formation_registry.cpp split

`army_formation_registry.cpp` (1,617 lines) held six concerns. It is now:

| File                          | Owns                                                                                                                                                                                        |
| ----------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `army_formation_registry.cpp` | `ArmyFormationRegistry`: group storage and the entity to group index. Only `link_member` / `unlink_member` / `clear` / `take_from_other_group` write the index.                             |
| `army_formation_codec.cpp`    | JSON for one `ArmyFormation` (`Codec::formation_to_json/from_json`). Pure value in, value out; restore drops transient state (morph, stale move plan) here.                                 |
| `army_formation_morph.cpp`    | Reform-in-place maths (`Morph::start/advance/target/pace`): assignment that keeps places in shape, start frame, path measurement, per-tick interpolation.                                   |
| `army_formation_march.cpp`    | Corridor march: traversal class, corridor building, the pack centroid, the per-group advance step split into spread measurement, waypoint skipping, final-leg reopening and steer-and-step. |
| `army_formation_cohesion.cpp` | Shape survey and phase classification, straggler tracking, damage and speed multipliers given a formation.                                                                                  |
| `army_formation_runtime.cpp`  | `ArmyFormationRuntime` only: thin orchestration (`update`, `replan`, `begin_move`) over the modules above. Holds three accumulators, no group state.                                        |
| `army_formation_tuning.h`     | Constants shared by cohesion, march and runtime.                                                                                                                                            |
| `army_formation_ambient.h`    | `ambient_formation_registry()`, defined next to `instance()`.                                                                                                                               |

## Decisions

- Membership authority: the registry alone mutates the index. The runtime never
  touches it; it calls `remove_member`, `apply_plan`, `remove_group`. The
  component mirror (`ArmyFormationMembershipComponent`) stays in the runtime
  because it needs a `World`.
- `ArmyFormationRuntime`'s public API and header are unchanged. Its statics
  forward to the modules so callers and tests are untouched.
- The ambient-instance ratchet exempts only the registry definition file. The
  runtime's seven previous `instance()` sites now go through
  `ambient_formation_registry()` (one definition, in the exempt file) instead of
  being counted as new call sites in `game/formation`. The behaviour is
  identical (it still resolves the ambient session); the sites were not
  converted to `for_world` because that could change which registry a mixed
  session sees. Converting them is a separate, behavioural change.
- `Morph::start` calls `ArmyFormationRuntime::sync_membership_components`, as
  the original did after reassigning occupants.
- Behaviour is unchanged: evaluation order, early-exit points and float
  expressions were moved verbatim; `advance_maintained_groups` is now
  `is_advancing` + `March::advance_group`.
- Qt defines `slots` as a macro: the codec avoids it as a parameter name.
