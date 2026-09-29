# WP-G: combat action processing and damage application

Behaviour-preserving split of `combat_action_processor.cpp` (1,408 lines) and `damage_application.cpp` (1,117 lines). Iteration order, event order and RNG use are unchanged; the headless digest is the proof.

## Damage application (one authority)

`apply_unit_damage` in `damage_application.cpp` is still the only place health is reduced. It now reads as stages over a small `HitContext`: resolve the attacker, scale damage, subtract health, `present_formation_hit`, `publish_hit_event`, `queue_structure_impact`, then either `react_to_survived_hit` or `resolve_death`. The event order (CombatHit, then hit feedback and retaliation, then BuildingAttacked, or UnitDied on death) is the original order.

| Unit                     | Owns                                                                                                                                                                                            |
| ------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `death_variant`          | which collapse a casualty plays, from blow direction and slot (`infantry_death_variant`, structure fall heading), plus `begin_death_sequence`                                                   |
| `formation_casualties`   | preferred hit slot, roster shape, the per-slot hit flinch, queuing soldier casualties in three helpers (plan, slot pick, store), front-rank vacancy fill                                        |
| `blood_stains`           | stain cap, per-species scale, `spawn_blood_stain`                                                                                                                                               |
| `hit_feedback_processor` | per-tick knockback (existing) and now the entry side: `apply_hit_feedback` in stages (begin reaction, impact strength, impulse, turn toward attacker), `apply_melee_reaction_feedback`, stagger |

`damage_application.h` stays the public damage-domain header; the functions it declares are defined in the unit that owns them. Callers did not change.

## Commander action processing

`combat_action_processor.cpp` keeps dispatch: cancel, bow draw, event handling, `process_authored_combat_action`. The rest moved to:

| Unit                          | Owns                                                                                                   |
| ----------------------------- | ------------------------------------------------------------------------------------------------------ |
| `combat_action_predicates`    | action-id classification, advanced-commander checks, reach, "does the sweep decide contact"            |
| `commander_signature_effects` | strike form/span/intensity, presentation cues, signature sweep and shot effects                        |
| `commander_root_motion`       | authored lunge: step, target assist turn, stop distance, walkability                                   |
| `authored_action_reaction`    | hit stop, poise, stagger, launch after an authored hit                                                 |
| `rts_melee_contact`           | RTS melee contact (reach and cone eligibility, resolution, exchange presentation)                      |
| `action_contact_damage`       | weapon-trace hit (window, ignore lists, hit, reaction, book-keeping), radial damage, mount body impact |

`rts_melee_contact` and `action_contact_damage` call each other (a trace hit can resolve as an RTS melee contact; an advanced-commander hit triggers radial damage). They share only public functions declared in their headers.

## Non-obvious points

- `slots` is a Qt keyword macro; the ignore-list storage is named `entries`.
- `IgnoredTargetLists` holds spans into its own array, so it is non-copyable.
- `nearest_soldier` takes a mutable `Entity&` because `live_soldier_targets` does.
