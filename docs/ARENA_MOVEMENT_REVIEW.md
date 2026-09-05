# Arena movement and animation review — 2026-09-05

Reviewed the production Arena renderer at 30 simulation/render samples per second,
using fixed cameras, seed 1337, captured PNG sequences, and per-frame JSONL traces.
Artifacts are under `artifacts/movement-review/` (not committed). These observations
cover animation and movement; no meshes or combat damage values were changed.

## Coverage

| Cases                                                                | Arena scenarios                                                                                                              |
| -------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------- |
| Infantry start, walk, stop, reverse; walking and running alone       | `infantry_locomotion_matrix`, `rpg_locomotion`                                                                               |
| Mounted ranks start, ride, stop, reverse; charge and melee           | `mounted_locomotion_matrix`, `mounted_knight_action_transition`, `shield_wall_cavalry_impact`                                |
| Elephant travel, stop, reverse, group movement and contact           | `elephant_locomotion_matrix`, `elephant_trample`                                                                             |
| Crowds, crossing traffic and obstacles                               | `crossing_formations`, `path_building_alley`, `nav_ruin_melee`                                                               |
| Infantry approach, attack, recovery and chase/disengagement          | `swordsman_action_transition`, `attack_to_chase`, `spear_walk_contact`                                                       |
| Sheep graze, wander and flee; wolves prowl and attack; birds scatter | `wildlife_grazing_herd`, `wildlife_herd_flees_troops`, `wildlife_wolf_pack`, `wildlife_wolf_ambush`, `wildlife_bird_scatter` |

The commander trace includes Walk and Run. The cavalry impact trace includes Walk,
Run, RidingCharge, and AttackSword. Elephants use their authored walking gait;
the review does not imply an elephant gallop was tested. Wildlife includes sheep,
wolves, and birds, the species available in these scenarios.

The low-poly infantry, riders, horses, elephants, sheep, wolves, and birds retain
their existing silhouettes and proportions. Captures did not motivate a mesh
change in this pass. The more noticeable problems were temporal: crowded mounted
reversals, abrupt gait changes, and overlapping/unevenly participating melee ranks.
Close-up horse and wolf sequences are also saved under `closeup/`; wolves remain
partly obscured by infantry and combat effects at contact.

## Findings and improvement pass

- Crowded contact resolution applied the entire overlap immediately, once per
  neighbor. It now shares a per-body correction budget of 2 m/s across all contacts,
  capped at 0.15 m for a long frame. Holding and melee-locked bodies remain anchored.
- Formation orientation followed the movement target even when the route's current
  desired direction differed. It now uses the route follower's desired direction,
  retaining the existing turn-rate and sideways-translation limits.
- Retargeting an unfinished horse gait blend discarded the intermediate gait.
  The next blend now starts from the previous blended gait and retains locomotion
  playback when a stop interrupts a start.
- Horse and elephant stride cursors were invalidated during idle. They now stay
  parked through a stop instead of restarting at an unrelated wall-clock phase.
- Sheep/wolf foot phases integrated filtered speed, lagging acceleration and
  continuing to step after stopping. Phases now consume measured travel once;
  speed filtering and hysteresis still select the gait. Long frames use their
  actual elapsed time when estimating speed.
- A wolf's filtered run speed could hide the beginning of a stationary bite.
  Once measured travel stops for a bite or flinch, the contact clip can begin
  immediately through the existing animation crossfade.
- Supporting formation members could inherit the strike carrier's authored phase
  and clip. Those overrides now remain with the carrier during formation melee,
  letting other soldiers use their own attack, recovery, and guard presentation.

The original infantry, mounted, and crossing traces contained no moving-idle samples
(root speed above 0.2 m/s with an Idle animation). Mounted rank reversals still
showed fast catch-up motion, peaking near 7.7 m/s. This pass does not claim to remove
all horse overlap or per-soldier foot-skating; the mounted diagnostic foot positions
describe the rider, not the horse's planted hooves.

## Reproduction

```bash
cmake --build build --target arena_app render_tests combat_balance_tests simulation_tests app_tests -j 8
build/bin/arena_app --batch --scenario mounted_locomotion_matrix \
  --fps 30 --seed 1337 --capture-interval 0.5 \
  --animation-diagnostics --profile --watchdog-multiplier 20 \
  --artifact-dir artifacts/movement-review/verified
```

Use the scenario IDs in the coverage table to reproduce the other cases.
`before/` holds the original release executable's captures; `after/` holds the
first rebuilt locomotion checks. The current Arena also requires GPU timings for
its frame-budget expectation, so final acceptance runs use `--profile` and are
stored separately in `verified/`. A screenshot or a scenario PASS alone does not
prove planted-foot accuracy or smooth transitions between every pair of frames.

## Validation

The build completed successfully. All 274 targeted tests passed:

| Test selection                                                                         | Passed |
| -------------------------------------------------------------------------------------- | -----: |
| Render, gait, wildlife clip selection, soldier turns, combat presentation              |    109 |
| Local avoidance/contact, wildlife fights, routes, melee engagement, movement ownership |     39 |
| Movement motor, formation movement/cohesion, melee exchanges                           |     39 |
| Wildlife simulation, birds, movement presentation                                      |     42 |
| Command service, including route-facing regression                                     |     45 |

Nine new regressions cover interrupted horse starts, elephant stride continuity,
wildlife distance tracking, long-frame speed, bite entry, formation clip ownership,
dense contact budgets, locked-fighter separation, and route-facing. This is a
targeted validation pass, not a claim that the entire repository test suite ran.

Before the changes, 16 of the 18 reviewed scenarios passed their scenario checks.
`path_building_alley` missed its destination after taking a long detour.
`spear_walk_contact` reported missing combat indicators, soldiers without attack
animations, and a terminal-pose stall. The clean-capture option suppresses overlays,
so the indicator errors must be distinguished from movement/animation findings.

Final acceptance passed 16 of 18 scenarios, with the same two scenario failures
as the baseline. These runs use visible overlays. The alley still misses its destination;
the dense spear fight still reports 32 living-soldier participation findings,
two missing indicators, and one group-level `soldiers_never_fought` finding.
Some non-attacking members have guard/support roles, so these findings need a
role-aware follow-up rather than forcing every soldier to swing. A terminal-pose
stall appeared in the baseline and an earlier rebuilt capture, but did not recur
in the final isolated spear run; it is not claimed fixed.

Concurrent capture runs produced frame-budget warnings. The alley, spear, and
bird-scatter scenarios were rerun without other Arena processes; their timing
warnings cleared without changing performance thresholds or scenario expectations.
The final cavalry-impact run also passed. No final scenario reports a frame-budget
failure. Captured images are wall-clock snapshots, whereas the trace advances in
fixed simulation steps; screenshot indices are not exact simulation timestamps.
