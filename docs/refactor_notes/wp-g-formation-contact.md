# WP-G: formation contact processor split

`formation_contact_processor.cpp` (1,948 lines, `publish_formation_presentation` 637 lines,
`walk_formation_slot` 434 lines) is now a 16-line entry point, `update_formation_contacts`,
that runs two phases in the original order:

1. `publish_formation_contacts` (`formation_contact_fronts.cpp`): pair broad-phase and
   signature cache (the thread-local `g_pair_cache` lives only there), front building
   in sorted attacker order, and publication onto `FormationContactComponent`.
2. `publish_formation_presentation` (`formation_presentation_publisher.cpp`): per formation
   entity, builds an immutable `EntityFrame`, runs `publish_soldiers`, then commits the
   presentation component.

## Ownership

| File                                   | Owns                                                                                                                                                                          |
| -------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `formation_local_frame.h`              | Actor-local to world conversions and the per-slot hash; inline so hot paths keep inlining.                                                                                    |
| `formation_combat_roles.*`             | Role hash, crowd-brawl rule, role to action, front/pair assignment, retained target hysteresis.                                                                               |
| `formation_structure_facade.*`         | Facade shift and nearest facade anchor for formations attacking a building.                                                                                                   |
| `formation_slot_directive.*`           | Per-slot directive stages (seed, engagement, yaw hold, fallen offset, contact close speed, relocation, facade clamp, walk).                                                   |
| `formation_presentation_publisher.cpp` | Thread-local layout pool and foreign-fighter grid, per-entity facts and commit.                                                                                               |
| `formation_soldier_walk.*`             | `walk_formation_slot` as named stages: timing, snap, crowd relax, aim, turn response, catch direction, steering (mounted or on foot), step plan, constrain, land, write-back. |
| `formation_foreign_grid.*`             | Spatial hash of soldiers from other fighting formations.                                                                                                                      |
| `formation_soldier_gait.*`             | Gait hysteresis and run speed.                                                                                                                                                |
| `formation_reform_walk.*`              | Squad-reform walkers heading to their new slots.                                                                                                                              |
| `formation_terrain_constraint.*`       | Ground and terrain step clamping, pull onto terrain.                                                                                                                          |

## Invariants kept

- Order inside one entity: hit tick, layout resolve, presentation get-or-add, facts, facade,
  damage carriers, reform tick, then slots. `previous_soldiers` is copied before the resize and
  before any slot is overwritten; each slot's `previous` is a copy taken before it is rewritten.
- The layout pool is a `std::deque` so `const&` layouts stay valid while opponents' layouts are
  resolved during the same tick. `LayoutCache::begin_tick` resets the index only.
- The foreign grid is filled once from last tick's published soldiers before any entity is rewritten.
- The walk stages do the same float operations in the same order as the former monolith; gait
  settling stays the last step, and the snap path (reset or external reform) still returns early.
- No behaviour change: the bot_skirmish headless digest is unchanged.
