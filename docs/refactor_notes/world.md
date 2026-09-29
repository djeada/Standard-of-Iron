# World split (WP-F, #1559)

`game/core/world.cpp` went from 1,792 to about 650 lines. `World` is still the ECS facade: entity CRUD, handle table, system scheduling, access verification, observers, `World::update` and `World::publish_render_snapshot` stay in `world.cpp`.

## New units (all in `engine_core`, `game/core/`)

- `world_motion_presentation.{h,cpp}`: `begin_motion_presentation_frame`, `finalize_motion_presentation_frame`, `publish_movement_trace_frame`. They use only `World`'s public API (`each`, `entities_with`, `get_entity`), so nothing was exposed for the move. The 233-line finalize lambda is now `finalize_motion_entity`, a short orchestrator over `observe_motion` (pure reads into `MotionObservation`), `update_chase_intent`, `classify_motion_state`, `apply_motion_state`, `apply_motion_velocity`, `apply_motion_target`, `apply_motion_direction` and `apply_motion_source`. The order of writes to `MotionPresentationComponent` is the original order. The trace sample builder is `publish_movement_trace_sample`.
- `world_creature_presentation.{h,cpp}`: `publish_creature_presentation_frame` (declared in the private header, used by `World::update`) and the public `publish_creature_presentation(s)` (still declared in `world.h`). The 283-line publisher is split into `publish_target`, `add_*_inputs` (death, construction, combat, melee lock, hit/cast), `apply_action_sample`, `apply_formation_hit_gate`, `apply_healing`, `apply_commander`, `apply_guard_and_layout`, `apply_hold_showcase_authored` and `presentation_changed`. The dodge override in `apply_commander` runs after the formation hit gate, as before, and the revision bump compares against the previous component exactly as before.
- `world_render_snapshot.{h,cpp}`: the snapshot copy lists (`copy_authoritative_snapshot_components`, `copy_presentation_snapshot_components`, `copy_render_components`, still declared in `world.h` because tests and `production_system_test` call them) and the render signature (`render_entity_is_stable`, `render_entity_signature`). Buffer selection and the publish loop remain in `World::publish_render_snapshot` because they write `World` members.

## Guard tests

`tests/save/snapshot_contract_test.cpp` greps the copy lists; it now reads `world_render_snapshot.cpp`.

## Not done

`World::publish_render_snapshot` is still about 110 lines because it mutates the snapshot world's private buffers; splitting it further would need friend access or exposing members, which the package rules forbid.
