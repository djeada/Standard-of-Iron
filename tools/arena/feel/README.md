# Commander feel playback routes

`locomotion.action.json` is the first focused route for the RPG feel plan in
`todo.md`. It uses the production duel mission and live combat; it is not the
complete terrain/water/bridge acceptance course.

From the repository root, after building `standard_of_iron`:

```sh
build/bin/standard_of_iron \
  --mission-file tools/arena/promos/film/steam/duel.mission.json \
  --skip-briefing --graphics-preset medium \
  --action-fixture tools/arena/feel/locomotion.action.json \
  --film artifacts/rpg-feel/review --film-fps 30 --film-seconds 9 \
  --film-size 960x540
```

| Time (seconds) | Action to inspect                                             |
| -------------- | ------------------------------------------------------------- |
| 0.2–1.2        | Enter commander control and strafe right without a lock       |
| 1.5–2.5        | Acquire a lock, then strafe right; direction must not reverse |
| 2.8–3.5        | Strafe left around the target                                 |
| 3.8–4.7        | Retreat and dodge backward                                    |
| 5.1–6.1        | Jump and land                                                 |
| 6.2–7.8        | Attack, then guard under live enemy pressure                  |
| 8.0–9.0        | Return to RTS control                                         |

The route is a visual integration check, not an assertion that every action must
succeed: live combat can refuse an action based on stamina or the current phase.
Exact stamina boundaries and blocked-dodge velocity are tested by
`CommanderMotorTest`. The fixture's required coverage only verifies selection;
inspect the video and tests before claiming the behavior passed.

For real-time play, launch the same mission without `--film` or `--action-fixture`.
Select the commander and press Return. Repeat the route manually, then press
left/right while acquiring and releasing lock-on. Roll into a wall or friendly
traffic, repeatedly spend stamina, and verify refusals don't grant free movement.
Use the configured jump binding from Settings; Space is dodge by default.

Preserve matching baseline/candidate captures and their worktree diffs. Film time
is controlled and cannot measure real-time input latency or frame pacing. Do not
run multiple renderers while measuring performance. Keep the broader outstanding
checks in `todo.md` open.

## Resource continuity during mode changes

`mode_resources.action.json` repeatedly dodges and switches out/in before stamina
recovery is due, then guards and returns to RTS. Use the same mission and command
above with this fixture, `--film-seconds 8`, and a fresh output directory. Compare
stamina changes and mode transitions before/after; the exact resource boundaries
and guard-break lockout are verified by `CommanderModeResourcesTest`, not inferred
from a HUD bar. The live battle may interrupt an action, which should be recorded
rather than mistaken for a successful authored dodge.

## Ford traversal slice

`traversal.mission.json` loads a small production-game inspection map. In grid
coordinates, the commander starts at (48, 34), facing the ford at (48, 48).
The river runs east–west. A separate bridge crosses at x=72, and a hill lies
beyond the ford at (48, 76). Water away from the two crossings is blocked.
The ford uses the existing authored 0.55 speed multiplier and 0.45 m depth;
it is temperate so chill does not obscure the entry/exit comparison.

```sh
build/bin/standard_of_iron \
  --mission-file tools/arena/feel/traversal.mission.json \
  --skip-briefing --graphics-preset medium \
  --action-fixture tools/arena/feel/ford.action.json \
  --film artifacts/rpg-feel/ford-review --film-fps 30 --film-seconds 13 \
  --film-size 960x540
```

The route holds forward+sprint for 11.5 seconds, crossing the ford and resuming
sprint on the far bank. Inspect the reduced travel speed, wading pose, foot
support and stamina bar. Selection coverage only proves selection; numerical
water behavior is checked in `CommanderMotorTest`.

For keyboard play, omit the action fixture and film arguments. Click the
commander in army view and press Return to enter RPG mode. (Action fixtures run
only during film/benchmark playback.) Walk/sprint into the ford, roll in the
water, reverse onto the bank, then approach the bridge and hill. These are focused
inspection assets, not the full acceptance course (stairs, walls, weapon targets
and crowded combat remain to be added).
