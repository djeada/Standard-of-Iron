# WP-H: movement, AI, wildlife and undead-zone splits

Behaviour is unchanged: same update order, same tie-breaking, same RNG draws, same per-tick allocations. The sim digests before and after are recorded in the hand-off report. This note records why each seam sits where it does.

## The MovementComponent access rule

`MovementComponent` keeps its raw fields (`vx`, `target_x`, `path`, `route_id`, ...) private to `MovementSystem` and `RouteFollowSystem` (plus `Serialization` and a test accessor). That boundary is deliberate, so the split does not add friends to the component. Code that only reads, or writes through the public setters (`stop()`, `set_manual_velocity`, `begin_order()`), moved to plain namespaces or classes. Code that writes raw fields is a class **nested** in the owning system and defined in its own file; a nested class of a friend keeps the friend's access, so the component header is untouched:

| Nested class                  | Header                           | Writes                                                     |
| ----------------------------- | -------------------------------- | ---------------------------------------------------------- |
| `MovementSystem::Assignment`  | `movement_orders_assignment.h`   | path, targets, goals, route identity                       |
| `MovementSystem::GroupIssue`  | `movement_orders_group.h`        | precise arrival, issuer retargets, group pace, route lanes |
| `MovementSystem::Motor`       | `movement_system_motor.h`        | velocity, travelled distance                               |
| `MovementSystem::Gates`       | `movement_system_gates.h`        | target/velocity resets of hold, melee lock, bypass         |
| `RouteFollowSystem::Steering` | `route_follow_system_steering.h` | current waypoint target                                    |

`MovementSystem::Assignment` is public because the route follower re-plans through it; everything else is an implementation stage.

## movement_system.cpp (1345 lines, `move_unit` 354)

`move_unit` is a 60-line orchestrator: bind the components into a `Mover`, end an escape that has arrived, unstick the body, then dispatch on `classify_movement_gate`. `game/README.md` still finds it in `movement_system.cpp`.

| Concern                                            | Home                                                                       |
| -------------------------------------------------- | -------------------------------------------------------------------------- |
| Heading, facing pass, formation turn rate          | `movement_system_heading.cpp` (`FacingPass` holds the per-step turn state) |
| Which steps a body may take, sweep, slide, unstick | `movement_system_collision.cpp` (`MotorCollision`, `sweep_through`)        |
| Velocity, acceleration, sweep, displacement        | `movement_system_motor.cpp`                                                |
| Hold / melee lock / bypass / direct control        | `movement_system_gates.cpp`                                                |
| Duel circling; owns the shared sway clock          | `movement_system_duel_footwork.cpp` (`DuelFootwork`)                       |
| Deferred path searches; owns queue and generations | `movement_system_path_requests.cpp` (`PathRequestQueue`)                   |

`m_duel_clock` and the two path-request containers left `MovementSystem`; the system keeps only the obstruction revision. Deferred routes are applied through a function pointer (`apply_deferred_route`) because assignment needs the friend access the queue does not have.

## movement_orders.cpp (1061 lines, `issue_move_units` 254)

`issue_move_units` is now `GroupIssue::issue`: prepare every unit, declare the slowest pace, then either route individually or plan one shared corridor and route each member through `MemberRouter`. The member routing keeps the original fallback order: escape a sealed pocket, straight walk (formation moves), own route (if preferred and not a long detour), lane across the group corridor, fresh search (deferred once eight synchronous searches were spent). `movement_orders_targets.cpp` holds the pure ground-resolution helpers. `prepare_move` stays in `movement_orders.cpp` because the ownership test asserts that only the order pipeline calls `begin_order()`; `begin_route(` lives in `movement_orders_assignment.cpp` and the test reads both files.

Tests added: `tests/systems/movement_orders_test.cpp` drives the order entry points (`CommandService::move_unit(s)`) and checks targets, order sequence, chase flags, group pace, mismatched input, facing intents, formation-slot followers and routing with a registered movement system.

## route_follow_system.cpp (1041 lines, `follow` 373)

`follow` binds a `FollowFrame` and chooses between a resting gate and `follow_route`, which reads as stages: refresh clearance, publish route facts, objective stall ladder, goal check, sync route, aim, progress, arrival, desired motion.

| Concern                                                                      | Home                               |
| ---------------------------------------------------------------------------- | ---------------------------------- |
| Gate classification, walkable-point rules, speed caps                        | `route_follow_system_gate.cpp`     |
| Stall ladder (replan, sidestep, abandon); state lives in the facts component | `route_follow_system_stall.cpp`    |
| Progress state machine, arrival bookkeeping                                  | `route_follow_system_progress.cpp` |
| Aim point, arrive radius, desired motion, route facts                        | `route_follow_system_steering.cpp` |
| Arrival, held-by-friends, short-route hold and recheck                       | `route_follow_system_arrival.cpp`  |

Abandoning an objective mutates the components only; `RouteFollowSystem` erases the route afterwards, so the route map stays owned by the system.

## ai_reasoner.cpp (1371 lines, `update_context` 307)

`update_context` is now: reset counters, expire attack records, tally friendly units, resolve the base anchor, expansion site, base manager and stations, plan the force, then average health, neutral barracks, nearby threats and progress. Order matters: `update_attack_wave` reads the macro targets and unit lists computed just before it and runs before `average_health` and `visible_enemy_count` are set.

| Concern                                                    | Home                       |
| ---------------------------------------------------------- | -------------------------- |
| Doctrine predicates (garrison, attack size, threat memory) | `ai_doctrine_rules.cpp`    |
| Counting the snapshot into the context                     | `ai_context_census.cpp`    |
| Outpost site choice and expansion decision                 | `ai_expansion_planner.cpp` |
| Builder, building and engine targets                       | `ai_macro_targets.cpp`     |
| Assault, reserve and harassment membership                 | `ai_force_assignment.cpp`  |

The state machine is one function per state plus `react_to_threat`, `break_deadlock` and `note_state_change`.

## builder_behavior.cpp (1831 lines, `execute` 468)

`execute` is now: gate, timer, review stalls, gather the free builders, one construction cycle, divide work parties, manage the gather crew. State moved with behaviour:

| Class / unit                                            | Owns                                                                |
| ------------------------------------------------------- | ------------------------------------------------------------------- |
| `WorkerStallWatch` (`builder_stall_watch.cpp`)          | per-worker watch, sour nodes, stalled set                           |
| `ConstructionLedger` (`builder_ledger.cpp`)             | repeat counter, deferred type, per-slot orders, blocked plan slots  |
| `GatherCrew` (`builder_gather_crew.cpp`)                | gather priority and its hold time                                   |
| `BuilderPool` (`builder_pool.cpp`)                      | the builders still free this cycle; nearest / strongest / last pick |
| `builder_town_plan.cpp`                                 | settlement census, plan step choice, plan reservations and keep-out |
| `builder_site_geometry.cpp`, `builder_site_planner.cpp` | clearances, free-site search, ring offsets, node on the ground      |
| `builder_intent.cpp`                                    | wish list order and affordability choice                            |
| `builder_orders.cpp`                                    | harvest, repair, field, clearing and construction commands          |
| `builder_affordability.cpp`                             | cost verdicts and stockpile priorities                              |

`BuilderBehavior` keeps the timer and the construction counter, and the counter is passed by reference to the two stages that advance it. Building names are `inline constexpr` pointers compared by identity, so they now live in one header.

## wildlife_system.cpp (1468 lines)

| Class / unit               | Owns                                                                                                                   |
| -------------------------- | ---------------------------------------------------------------------------------------------------------------------- |
| `WildlifeSpawner`          | groups, next id, seed, released waves, elapsed clock, factory registry; planning, initial spawn, wave release, respawn |
| `WildlifeCensus`           | per-tick animals, quarry, group runtime, threat and interest fields; pack queries, alerts                              |
| `WildlifePredation`        | bite wind-up, contact and landing, counting bites in the system stats                                                  |
| `WildlifeNatureActions`    | the brain's view of the world (no more friend adapter)                                                                 |
| `wildlife_persistence.cpp` | the save shape and its decode, kept next to each other                                                                 |

`WildlifeSystem` keeps settings, stats, brains and the per-animal update, split into `update_animal`, `think_if_due` and `refresh_census`. RNG order is unchanged: group seed draws in `plan_groups`, member scatter and rotation draws in `spawn_member`, think-cooldown jitter in `think_if_due`. `configure` still does not reset the wave clock or released waves.

## undead_awakening_system.cpp (1107 lines)

`UndeadRuntimeZone` moved to `undead_zone_runtime.h`. `UndeadShrine` owns shrine placement, the anchor structure, the capture lock and the garrison break with its reward; `UndeadGuardians` owns the leash poll clock and guard posts; `UndeadZoneMusic` owns the music poll and playing flag; `undead_zone_persistence.cpp` pairs save and restore. The system keeps wave logic and the update, now `update_zone` per zone.
