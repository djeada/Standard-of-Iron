# WP-G: attack_processor.cpp

`attack_processor.cpp` was 2,144 lines and `process_attacks` was 498 lines. It is now
about 520 lines and `process_attacks` is about 25 lines; the longest function in the file
is `process_attacker` (about 40 lines). Behaviour, iteration order, RNG consumption and
effect order are unchanged (headless digest identical).

## Stages of one attacker (`process_attacker`)

1. Eligibility: `load_eligible_attacker` (pending removal, stagger, missing components,
   dead, wildlife, formation reserve rank). Reserve/wildlife still clear RTS tracking.
2. Lock upkeep: `maintain_melee_lock` (structure-lock release, `process_melee_lock`
   unless a mounted charge is resolving, target sync, drop of a finished lock's target).
3. Stats and clock: `load_attack_stats`, then the attack clock advances.
   Units without an `AttackComponent` use a local scratch clock, as before.
4. Target selection: `select_target` = melee-locked target, else the ordered target
   (`select_ordered_target`: reach test, chase steering, drop when no longer pursued), else
   `acquire_nearby_target` (auto-acquire, only when no `AttackTargetComponent` remains).
5. Engagement: `engage_target`, charge-impact wait, body-contact lock while cooling down.
6. Strike: `strike_target` -> `scaled_attack_damage` + `apply_attack_effect` (bow action,
   arrow volley, deferred melee action, or plain damage through the one damage authority
   in `damage_processor`). `deal_damage_to_rpg_commander` stays in this file because the
   layering test allows only a fixed set of callers.
7. No target: `settle_without_target` (orphaned presentation, held order drop, guard home).

## New owners (all `game/systems/combat_system/`, target `soi_combat`)

| File                      | Owns                                                                                   |
| ------------------------- | -------------------------------------------------------------------------------------- |
| `melee_lock.*`            | melee lock lifecycle, facing ledger, touch-lock scan, body-contact test                |
| `attack_chase.*`          | chase plans per target kind (structure, elephant, ranged, melee), move-intent throttle |
| `attack_control.*`        | attack animation begin, attack delay hash, face target, stop move, drop target         |
| `attack_stat_modifiers.*` | hold-mode and high-ground bonuses, tactical damage multiplier                          |
| `arrow_volley.*`          | RTS arrow volley geometry and spawning                                                 |
| `rts_commander_attack.*`  | commander/routine RTS melee and bow action selection, swing cadence                    |
| `attack_eligibility.*`    | `FormationRanks`, healer-priority test                                                 |

## Notes

- `TickContext` holds only per-tick borrowed references (world, query, projectiles,
  ledger, chase intents, ranks); per-attacker data travels in the small `Attacker`,
  `AttackStats` and `TargetChoice` values.
- The dead `if (target_reached)` assignment in the chase branch (unreachable because it
  sat in the not-reached branch) was dropped.
- `release_rts_arrow_volley` keeps its declaration in `attack_processor.h`.
- Source-text guard `commander_control_regression_test` still finds
  `CombatRules::participates_in_rts_melee_lock` in `attack_processor.cpp`.
