# Settlement Life

A village whose people stand still is scenery. This document covers the two systems that
keep the civilian side of a map moving on its own: the **errand loop** that sends
residents about their settlement, and the **standing gather order** that keeps a worker
running its own round between the tree line and the barracks yard.

Neither system takes a decision away from the player. Both retire the moment the player
gives an order, and both are cheap enough to leave running on every map.

## Residents and their errands

`SettlementLifeSystem` owns `SettlementResidentComponent`. A resident has a **hearth**
(the point its life is centred on), a **roam radius**, and one errand at a time:

| Errand      | Meaning                                              |
| ----------- | ---------------------------------------------------- |
| `Settling`  | No errand yet; pick one on the next think.           |
| `WalkingTo` | Walking to the errand point, with a travel deadline. |
| `Working`   | Standing at the errand point for `planned_dwell`.    |

Every `k_think_interval` (0.35 s) a resident advances that small state machine. When it
needs a new errand it collects the candidates inside its roam radius — friendly
non-wall buildings, and the "life" world props (fire camps, tents, supply carts, weapon
racks, plants, olive trees) — and rolls one of three destinations:

- **At a building**, standing off past the building's own collision footprint so it never
  tries to walk into the wall.
- **At a prop**, standing off by a fixed clearance.
- **On the street**, a point somewhere between a quarter and 85% of the roam radius from
  the hearth.

A proposal closer than `k_min_errand_distance` to where the resident already is gets
rejected and re-rolled, which is what stops residents shuffling on the spot. Destinations
are snapped to walkable ground before the move is issued, and the walk is a
`ScriptedMove` so it does not read as a player order anywhere else in the codebase.

Every errand carries a **travel allowance** proportional to its distance. Miss the
deadline, or go idle before arriving, and the resident abandons the errand and picks
another instead of standing in the road forever.

### Errand roles: what a resident does once it arrives

Arriving is only half of it. Each errand at a building or a prop rolls a
`SettlementErrandRole`:

- **`Labour`** (62%) — the resident works with its hands for the dwell. The dwell is
  extended by `k_labour_dwell_bonus` so the work reads at the game camera instead of
  flickering past.
- **`Loiter`** — the resident simply stands, and the humanoid ambient idle system takes
  over with its usual shuffles and weight shifts.

Street errands are always `Loiter`.

On arrival the resident turns to **face its focus** — the building or prop it walked to,
or the hearth for a street errand — through `TransformComponent::desired_yaw`, so a
worker at a market stall faces the stall rather than whatever direction it happened to
arrive from.

### How labour is drawn

There is no separate civilian work animation. `World::publish_creature_presentation_entity`
feeds a labouring resident into the **same construction action inputs** a builder uses,
with `work_elapsed` as the elapsed time and `k_settlement_labour_cycles_per_second` as the
cycle rate. The humanoid rig therefore plays its hammer/saw/chisel loop, which at the RTS
camera reads as "busy with both hands" — exactly what an ambient villager needs.

`work_elapsed` is advanced **every frame**, not on the 0.35 s think tick, because the
pose phase is derived from it; ticking it at think rate produced a visibly stepped
animation.

Two exclusions matter, and both are load-bearing:

- **A real builder job wins.** The construction inputs are only taken from the resident
  when `BuilderProductionComponent::in_progress` is false.
- **A dead resident is never labouring.** A villager cut down mid-errand used to keep
  `Working`/`Labour` frozen on its component, and feeding that to the pose pipeline
  alongside the death sequence **segfaulted `village_raid`** in the optimised build — a
  creature cannot be both constructing and dead. Two guards close it: the system resets a
  dead resident's errand to `Settling`/`Loiter` instead of skipping it outright, and the
  presentation requires `DeathAnimationComponent == nullptr` before it reads the resident
  at all.

## The alarm: residents run from armed strangers

A villager who keeps hammering a stall while a raider walks up to him is worse than a
villager who stands still. Every `k_alarm_interval` (0.3 s) the system builds one list of
**dangers** — everything that `can_use_attack_mode`, minus civilians, plus wolves — and
each resident checks it.

Wolves have to be named explicitly because `can_use_attack_mode` excludes every wildlife
spawn: without that clause a pack walked into a hamlet and the residents kept running
errands around it while it ate them.

An enemy (by `OwnerRegistry::are_enemies`) inside `k_alarm_radius` (11) puts the resident
into the `Fleeing` errand: it drops its work, drops any attack target it had picked up,
and is sent `k_flee_distance` (11) away from the threat. The direction is **away from the
danger, blended halfway toward the hearth** when the hearth is on that side, so a crowd
scatters inward through its own village rather than each villager running off the map on
its own vector.

A new leg is issued when the last one runs out (`k_flee_leg_seconds`) or the resident goes
idle before arriving, so the flight tracks a threat that keeps advancing. The all-clear
uses a wider radius (`k_all_clear_radius`, 15) than the alarm, so a resident does not
stop dead the moment it crosses back over the alarm line and immediately re-trigger.

Two rules keep this from becoming a bug factory:

- **The alarm never touches combat state.** A resident that already has an
  `AttackTargetComponent` is skipped entirely — parked in `Settling` and left to the
  combat system. Residents scatter as raiders _approach_; once one is actually engaged,
  combat owns it. Pulling a live attack target out from under the engagement,
  melee-lock and formation-contact bookkeeping mid-tick is not a supported move, so the
  alarm does not try. This is why civilians are excluded from retaliation
  (`can_retaliate`): a wounded villager used to be handed an attack target, which made the
  alarm skip it from then on, so the whole hamlet turned round and walked back into the
  pack it had just run from. A civilian that is _locked_ in melee still fights.
- **A player order still beats fleeing.** The claim check (`CivilianDeliveryComponent`,
  `PlayerOrderIntentComponent`) runs before the alarm.

## Adoption: who becomes a resident

Before this existed, `SettlementResidentComponent` was only ever attached by arena
scenarios and by save loading — so villages were alive in the arena and dead in the
shipped game, where every civilian a home produced stood where it spawned until the
player clicked it.

`adopt_idle_civilians` closes that. Every `k_adoption_interval` (2 s) it looks for
civilians that are:

- alive, of `SpawnType::Civilian`, and not already residents;
- **unclaimed by the player** — no `CivilianDeliveryComponent`, no
  `PlayerOrderIntentComponent`, no attack target, and no movement target;
- within `k_adoption_radius` (20) of a friendly, non-wall building.

Such a civilian is given a resident component whose hearth is that **nearest friendly
building**, not its own position, so a crowd produced by one home gathers around the
settlement rather than around each individual spawn point.

The last condition is why this cannot animate a civilian stranded in open country: with
no friendly building nearby there is no settlement to be a resident of, and the civilian
is left alone.

### Release: an ordered civilian stops being a resident

Adoption is for civilians nobody is managing. The moment the player moves a civilian by
hand — a `PlayerMove`, a `FormationMove`, or a stop — `OrderService` **releases** it:
the resident component is marked `released` (and added, if the civilian did not have
one) and the system skips it from then on. Because adoption also skips anything that
already carries the component, a released civilian is never re-adopted.

The rule is the same one the standing gather order follows: _order it and it is yours to
manage_. Without it a civilian walked somewhere deliberately would stroll off to the
nearest village a few seconds after arriving, which is a lost unit as far as the player
is concerned — and it is exactly what broke `riverside_mill_town`'s carrying party, which
crossed its bridge, arrived, and then wandered out of its own destination check.

### Re-anchoring

While a resident is busy elsewhere — being walked to a barracks for its manpower, under
attack, or carrying out a player move — the system parks it (`Settling`, `Loiter`, work
cleared) and marks the hearth unassigned. The hearth is re-derived from the nearest
friendly building the next time the resident is free. Walk a civilian from one town to
another and it settles into the town it is standing in, instead of trudging back to the
old one.

## The standing gather order

The harvest loop used to stall on its last step. A worker chopped a tree, hauled the load
to a barracks yard (see [RESOURCE_STOCKPILE.md](RESOURCE_STOCKPILE.md)) — and then stood
there. Every single tree in a forest was a separate click.

`BuilderProductionComponent` now carries a standing order:

| Field                 | Meaning                                           |
| --------------------- | ------------------------------------------------- |
| `has_gather_order`    | This worker is on its own round.                  |
| `gather_product_type` | Which harvest job to repeat.                      |
| `gather_anchor_x/z`   | The last node it worked — the centre of its round |

`ProductionSystem` sets it the moment a harvest **succeeds**, so every path that can
assign a harvest job — the player's build card, the arena's `HarvestResource` step —
gets the loop for free without knowing about it.

`GatherLoopSystem` then does the resuming. Every `k_think_interval` (0.6 s) it looks for
a worker with a standing order that is genuinely free: not building, not repairing, no
task target, no queued wall sites, **not carrying a load**, no attack target, and not
walking anywhere. It finds the nearest unreserved node of the right kind within
`k_search_radius` (22) **of the anchor**, reserves it, and assigns the same job the
player would have. Searching from the anchor rather than from the worker is deliberate:
the worker returns to the stand of trees it was working, instead of taking whatever happens to
grow next to the barracks.

The worker is sent to a point `k_work_standoff` (1.8) short of the node, on the side it
approaches from, so it stands beside the tree it is felling.

### What ends a round

- **A manual move or a stop.** `OrderService` clears the standing order alongside the
  other auxiliary orders on a `PlayerMove` or `FormationMove`, and on `apply_stop`.
  Taking the worker somewhere by hand is how you take it off the job. A `ScriptedMove` —
  which is what hauling a load home is — does not count, or the round would end on its
  own first trip.
- **An exhausted area.** No unreserved node of the right kind within the search radius of
  the anchor, and the order is dropped rather than left re-scanning forever. A farm round
  is the one exception: while a friendly farm still stands near the anchor the worker
  waits for the next crop instead of retiring — see
  [FOOD_AND_FARMS.md](FOOD_AND_FARMS.md).

A **builder job in between does not end the round**. Borrow a woodcutter to raise a
house or mend a wall and it goes back to the tree line when the job is done, because
`GatherLoopSystem` keys off `gather_product_type` and only ever acts on a worker with no
job at all. That is the whole point of the feature: the interesting order is the one the
player gives, and going back to work afterwards is not one.

### Why the AI is excluded

`ProductionSystem` only records the standing order for workers **without**
`AIControlledComponent`. The faction AI already re-issues harvest jobs on its own
cadence, and its builder behaviour picks workers by `!movement.has_target` — a worker
kept permanently busy by a standing order would never be free for the AI to send to a
construction site. Leaving the AI on its existing loop keeps its economy behaviour
byte-for-byte unchanged.

## Saving

Both features are authoritative state and are serialized:
`has_gather_order` / `gather_product_type` / `gather_anchor_*` on the builder, and
`role` / `focus_x` / `focus_z` / `work_elapsed` on the resident. Dropping them would
either abandon a worker mid-round or reset every villager to the middle of the street on
load.

## Seeing it

`village_day_life` is the reference scene, authored for ambience capture rather than for
a contact test:

```bash
build/bin/arena_app --scenario village_day_life
```

The camera sits close enough to read hands and gait. Two woodcutters and two quarriers
are given one harvest order each at the start and are never told anything again — every
further trip they make is the standing gather order. Ten residents work the lane, the
market and the house yards, a flock grazes the meadow, and birds pass overhead. The
scenario asserts `OwnerHarvestsResource` at a threshold of 8, which four workers cannot
reach with one job each; the expectation fails if the gather loop stops resuming.

`village_harvest_cycle` remains the wider, AI-driven economy scene. See
[docs/SETTLEMENT_ASSETS.md](SETTLEMENT_ASSETS.md) for the rules arena settlement
scenarios have to satisfy.
