# Standard of Iron — live work list

Rewritten 2026-08-26, after a hands-on play session and eight batches of repair
work against what it found. Everything below was **reproduced**; everything
marked done was fixed and is held by a test that fails without the fix.
`docs/RPG_PLAYABILITY.md` points at this file as the live checklist.

## How the findings were produced

Two sessions against the real binary under Xephyr `:99` (llvmpipe, ~15 fps, low
preset), 1920x1080:

1. **As a player** — main menu, skirmish setup, _The Two Fords_ (`map_rivers`),
   Carthage vs one CPU.
2. **As an observer** — a computer-only skirmish on _Sunbaked Terraces_ through
   the new `--observe` flag, with `SOI_AI_TRACE=1` and `SOI_MOVEMENT_TRACE_DIR`
   set.

Instruments: `battlefield_gameplay_verifier`, `movement_trace_report`, and the
gtest suites.

## What the Xephyr re-test found (2026-08-27)

A second pass with the real binary under Xephyr, after all the work below
landed: menu, skirmish setup, a player match on _Amber Delta_, and a
computer-only match watched for nine minutes of simulated time with
`SOI_AI_TRACE=1`.

**Confirmed fixed, by looking at them:**

- The battlefield list stays at seven entries across visits, and every row now
  draws its own map.
- **Observe** starts a computer-only match; the HUD is the army board, the top
  bar says SPECTATOR, and the match runs to a decision instead of ending in an
  instant defeat.
- The computer plays. Over nine minutes: three barracks built, 46 troops
  trained, armies of eight to eleven with four to seven melee, and **one AI
  destroyed another's base outright**. The state histogram is the finding
  turned inside out - `Attacking` 79 samples, `Expanding` 1, where the first
  pass saw neither, ever:

  | state | first pass | now |
  | --- | ---: | ---: |
  | Attacking | 0 | 79 |
  | Expanding | 0 | 1 |
  | Defending | pinned | 122 |

- The Iron Sepulcher holds its ground with six troops and never marches, and no
  neighbour is latched into permanent defence by it.
- The campfire lights a small pool; the barracks beside it keeps its own colour.
- Escape reaches the menu and the battle resumes.

**Still open, seen again in this pass:**

- Fog of war is worse than "a hole in the world": with the camera over
  unexplored ground the whole viewport is a featureless tan wash with no terrain
  at all. Item 5 below.
- The selection ring is still blue, which in a two-player match is the enemy's
  colour. Item 4.
- The formation badge sits on **Forming up** at 50% cohesion indefinitely for a
  two-unit group that has arrived and stopped. The phase vocabulary fix landed -
  the HUD knows every phase the simulation can report - so what is wrong now is
  that the simulation never reports a settled phase for this group. New item 10.

**Worth checking:** the army board showed `Kills 0` for every army while losses
ran to 35. That may be honest - the neutral garrison does the killing and no
player scores it - but nothing in this pass proved it either way.

## Where the game stands

The battle is a game again. A squad ordered across a ford arrives, the computer
runs an economy and fields troops, the sixty second gameplay soak is green, and
the movement analyser is a gate rather than a tool nobody ran. What the computer
still will not do is **attack**: it builds, it holds, it never marches. That is
the top of this list.

Measured over the eight capture scenarios, 60 s each:

| | before | now |
| --- | ---: | ---: |
| Movement findings | 91,777 | **302** |
| `battlefield_gameplay_verifier --all --seconds 60` | 8 of 8 scenarios failed | **passes** |
| Bodies of a 12-strong squad crossing a ford | 0 | **12** |
| AI fighting troops fielded in two minutes | 0 | 2–3 |
| GL objects leaked loading a match from the menu | 3,620 | **0** |
| Shader warnings per launch | 5 | **0** |

---

## Priority 1 — the computer will not fight

### 1. The AI never attacks

`SOI_AI_TRACE` over the skirmish-opening harness, both sides, two minutes:

```text
t=110.1 player=1 state=3 units=10 melee=0 ranged=2 builders=5 buildings=3
t=110.1 player=2 state=3 units=8  melee=1 ranged=1 builders=6 buildings=3
```

State 3 is `Defending`. Across a whole match neither side entered `Attacking`,
`Retreating` or `Expanding` even once, and the fighting half of the army stayed
at two or three units against five or six builders.

- [ ] Find the gate on the transition into `Attacking`. `AttackBehavior` wants an
      engagement assessment that a three-unit army may never satisfy; if the
      threshold is an absolute army size, a computer that cannot grow past three
      units can never reach it. That is a deadlock, not a decision.
- [ ] One AI sat in `Defending` for the whole match with nothing near it but the
      Iron Sepulcher, which is a **defensive bonus nation and correctly never
      attacks**. A Roman or Carthaginian plan must not read a faction that will
      never come at it as a permanent threat. `ctx.barracks_under_threat` is
      cleared each update (`ai_reasoner.cpp`) and re-armed by proximity; the
      re-arm needs to know the difference.
- [ ] The army is income-bound, not plan-bound. A home raises a fixed number of
      civilians and is then finished for good, and a civilian is the only thing
      that refills a barracks. The home target now grows when the recruitment
      buildings are starved, but that is moot if the builders are not gathering:
      check that `GatherBehavior` keeps them on wood, stone and iron once
      construction is done. Gold and food were never gathered at all — 250/200 at
      t=0 and unchanged at t=500.
- [ ] `ai_skirmish_opening_test` asserts what the shipped build can do today.
      Raise its bars as the above lands; the point of the harness is that the
      numbers only go up.

### 2. Movement residue

`movement_trace_report` over the eight capture scenarios, 60 s, stride 6:

| finding | count |
| --- | ---: |
| `DirectionReversal` | 281 |
| `GaitWithoutMotion` | 18 |
| `HeadingOscillation` | 3 |

`Starvation`, `RepathChurn`, `AngularAccelerationExceeded`, `BlockedStepStreak`,
`ProgressStall`, `CollisionPenetration`, `ArrivalRestart`, `WaypointRegression`
and `MissingTerminalOutcome` are all at zero.

- [ ] 281 direction reversals is the last large number — "2 direction reversals
      over 15 deg in 0.50s", concentrated in the melee scrums. Either the bodies
      really are shuffling on the spot in a fight, in which case the player sees
      it, or the check is too tight for a body turning to face a target. Decide
      which, then fix whichever is wrong.
- [ ] The 18 remaining treadmill cases run up to 3.7 s. They are bodies whose
      motor never published a velocity: `sample.accepted_vx` is written only when
      the motor steps, so a body carried by separation or by a depenetration push
      reads as 0 m/s while its gait says it is walking. Record the observed speed,
      not only the motor's.

### 3. The soldier half of the movement trace has no producer

`soldiers.jsonl` is 0 bytes after every run. The writer, the JSON parser, the
analysis and the report all exist and are fed nothing: `MovementSoldierSample`
carries `body_root`, `shadow_root`, `ring_root`, `lod`, `culled` and
`interpolation_alpha`, so it is a **render-side** sample — and the renderer never
emits one. Every per-soldier check in the analyser (anchor jumps, marker
mismatch, slot identity, layout dwell) has therefore never run on anything.

- [ ] Emit it from the renderer's per-soldier presentation walk, or delete the
      half that cannot be fed. A gate that silently measures nothing is worse
      than no gate at all.

### 10. A settled group still reads "Forming up"

Two units ordered across the map arrive, stop, and hold - and the badge stays on
**Forming up** at 50% cohesion for as long as you watch. The HUD is not the
problem: `FormationPhaseVocabularyTest` pins all six phases the simulation can
report, and the catalogues carry "In position", "Opening ranks" and "Filing
through". Something upstream never leaves the forming phase for a small mixed
group.

- [ ] Find what the simulation reports for a two-unit group that has arrived,
      and either report the settled phase or say plainly why 50% cohesion is not
      settled.

---

## Priority 2 — what the battlefield looks like

### 4. You cannot tell whose troops those are

Player colour is chosen in the roster ("Colour Red") and it reaches buildings,
minimap blips and the roster UI. The soldiers are a uniform navy-and-gold
silhouette for every faction; the only red on a Carthaginian swordsman is a
two-pixel sliver at the feet.

- [ ] Give troops a team-coloured element that survives a top-down 45 degree view
      at default zoom — shield face, cloak or crest, not the boots.
- [ ] The selection ring is blue, which in a two-player match is the *enemy's*
      colour. Make it read as "mine".

### 5. Fog of war reads as a hole in the world

Zoomed out, unexplored ground is drawn as near-black blobs inside the terrain and
everything beyond the vision circle becomes a flat blue-grey sheet, so the played
area looks like a green disc floating on an ocean with hard polygonal edges. The
minimap legend promises Visible / Explored / Unseen; the world shows only lit and
black.

- [ ] Darken and desaturate unexplored terrain rather than replacing it, and keep
      the ground plane continuous to the map edge.
- [ ] Give the map edge a skirt or backdrop so a zoomed-out view does not end in a
      cliff over nothing.

### 6. Shadow and light artifacts

- [ ] **A hard-edged opaque navy quad sits on the ground beside every barracks**
      (~40 m to its west on _Sunbaked Terraces_) and does not move as the sun
      crosses the sky — identical at noon, at dusk and at night. It reads as a
      hole in the ground.
- [ ] **Stray road-mesh triangles float above the terrain.** Two sharp tan shards,
      road-coloured with aliased edges, hang over the hillside east of the
      player's start on `map_rivers`, at every camera angle, with no road within
      forty metres. The ribbon is sampled every 0.6 tiles with seven lateral
      samples and takes the *max* of five height taps, so it ought to sit above
      the ground rather than through it — which points at the draw, not the
      geometry. Two hypotheses worth separating before touching anything: the
      ribbon z-fights the terrain in patches, or those triangles belong to a
      segment that is otherwise buried and only its corners emerge.
- [ ] **The campfire light pool is far too strong** — a hard-edged yellow ellipse
      ~20 m across that recolours the barracks and the ground orange. The flame
      itself is a flat billboard with visible triangle edges.

### 7. Bridge decks do not look like crossings

The runtime decks **are** longer than they are wide: the loader stretches an
authored span out to the banks, and `MapBridgeCoverageTest.RuntimeDecksAreLongerThanTheyAreWide`
now pins that. (An earlier entry here claimed both fords were authored wider than
long and blamed the map data. That was wrong — the authored spans are short by
design and `fit_bridge_span_to_riverbanks` fits them.) What is wrong is the
*look*: no parapets, no piers, a warped deck whose ends do not meet the road, and
a black under-edge over the water.

- [ ] Give the deck edges, approach ramps and abutments. `bridge_bank_landing()`
      already computes a landing and nothing visual uses it.

### 8. Battlefield-list thumbnails are placeholders

Every map in the skirmish list shows the same generic square; the real preview
renders only in the detail panel.

- [ ] Render the per-map preview into the list rows, or drop the icon.

---

## Priority 3 — load time and lifecycle

### 9. The audio bank is re-decoded and re-mastered on every mission load

Returning to the menu unloads the `Mission` and `Lazy` assets
(`game_engine.cpp`), so the next match decodes, resamples and re-runs the full
mastering pass — loudness, notches, tilt, limiter, loop seam — over the same 97
clips. Measured 2x in one `--observe` session and 3x in a session with two match
loads. The unload is deliberate and worth keeping; recomputing the analysis is
not.

- [ ] Cache the mastering *analysis* by asset id for the process lifetime and
      re-apply it on reload. That keeps the memory the unload was for and drops
      the CPU cost.
- [ ] `MiniaudioBackend: Sound still decoding, skipping play:
      "sound_ambient.weather_rain"` fires repeatedly during load, so the weather
      ambience is silently dropped rather than queued.

---

## Landed — with the test that holds each one

Each of these fails the named test if reverted.

**The battle**

- Squads cross every shipped ford. Three route defects, all found by reproducing
  the stuck squad headlessly: a body with clearance was answered by point
  sampling alone and so stepped over one-cell spurs; portal waypoints were
  snapped onto crossings without asking the navigation grid; and snapping
  collapsed the run of waypoints on a deck into a single point, inventing one
  long illegal leg. `map_crossing_traversal_test`,
  `PathfindingTest.ASegmentWithClearance*`.
- The computer opens a match instead of stalling on its first builders. Five
  faults in one chain: every nation lists commanders at the top of its priority
  table and a barracks refuses to recruit one; the plan had a single candidate
  and no fallback; homes were built and never staffed; civilians were raised and
  never walked to a barracks; and the home-only lifetime cap was being applied to
  barracks. `ai_skirmish_opening_test` (6 tests).
- The sixty second gameplay soak is green. An action's `last_damage` outlived the
  action that produced it, so the next sample read a fresh hit with no action
  running — `damage occurred without a visible action`, in all eight scenarios.
  The CTest gate now runs sixty seconds, not fifteen, so it can no longer pass by
  being shorter than the bug. `tools/CMakeLists.txt`.
- The movement analyser is a gate: `movement_quality_gate_test` (32 tests) over
  the eight capture scenarios.
- A body no longer plays its walk cycle at a standstill. The gait was selected
  from any displacement down to a millimetre, and the stall clock that would have
  caught it was computed and never read.

**Spectating**

- `--observe <map>`, plus an **Observe** button on the skirmish screen.
- A spectated match is no longer declared an instant defeat, and the slot the
  camera follows is AI-controlled rather than an empty seat that stood still all
  game. `VictoryServiceTest.ASpectated*` (5),
  `MapTransformerStructureTest.AnObserved*` (3).
- The spectator HUD is a board of the armies on the field, not the player's order
  palette. `tst_spectator_hud.qml` (5).

**Presentation**

- The day runs in real time. The speed control used to move the sun, so 4x put a
  whole day into ten minutes of play.
- Night is readable and still plainly darker than noon. `TimeOfDayReadabilityTest`.
- Iron ore takes its ghost-light from the map's undead presence, so the valley
  whose own description says "Nothing haunts this valley" has ordinary rock
  instead of violet crystal. `MapSupernaturalPresenceTest`.
- GL objects freed on the wrong thread are queued and deleted by the next thread
  that has a context, instead of being abandoned with a warning.
  `GlDeferredDeleteTest`.
- The five shader uniforms compiled out of reduced quality variants are looked up
  optionally, so a launch is quiet.

**HUD and setup**

- The battlefield list no longer duplicates itself on every visit.
  `MatchSetupMapListTest`.
- The three "population" numbers on screen name themselves: manpower in the
  field, soldiers in the selection, and a building's recruitment reserve.
- The bottom bar is tall enough to show a row of recruit cards at 1080p.
  `DesignTokens.test_the_rts_bottom_bar_*`.
- Battle-report statistics are Arabic digits — the Roman `N` for zero is gone —
  and ambient factions are no longer listed as armies.
- Formation status covers every phase the simulation can report; an arrived line
  used to read "Forming up" forever. `FormationPhaseVocabularyTest`,
  `tst_formation_status_badge.qml`.
- The dead `Connections` handler in `CommanderMessagePanel` and the nine
  `Unable to assign [undefined] to bool` warnings in `ProductionPanel` are gone.
  `tst_commander_message.qml`, `tst_production_panel.qml`.
- The loading overlay no longer prints `+-1347ms`. `LoadingOverlayLogTest`.

**Tooling**

- `make translations-check` works on a current Qt: the `number` merge heuristic
  was dropped after 6.4, and passing it to a newer lupdate is rejected outright.
  All five catalogues carry the new strings.
- The movement trace is bounded. An unconfigured run reached 12 GB, which the
  report tool could not read back inside two minutes.
- `battlefield_gameplay_verifier` says so when a run produces no verdict, and its
  locomotion-flicker budget can actually fire.
- `SOI_AI_TRACE=1`, `SOI_CAPTURE_TRACE=1`, and `AICommandApplier`'s refusal count
  with a reason per refusal: a computer that looks busy and does nothing is now
  measurable.
- `soi_headless` was a zero-byte build artifact, so `headless_replay_round_trip`
  failed on every run.

## Verified working — do not spend time here

- Menus, skirmish setup, campaign and tutorial entry, settings, and
  Escape → menu → resume.
- Map catalogue metadata: slot counts, "solo playable", scripted-opposition
  labels and per-map descriptions all match the JSON.
- Selection: box select, `X` select-all, per-unit and multi-unit panels, unit
  stats, and the builder and barracks context panels.
- Orders reach the simulation in two ticks — `first_response_seconds = 0.0333`
  in all eight verifier scenarios.
- Barracks recruitment: queue, progress, manpower and resource deduction, rally
  spawn, and the right headcount on arrival.
- Camera: pan, zoom, `Home`, the minimap viewport indicator, fog states and blips.
- Fog of war *logic* — reveal on movement, explored persists, `reveal_all` in
  spectator. Only its presentation is at issue (item 5).
- Terrain walkability, river-bank walkability, bridge coverage, and skirmish
  start-to-start reachability.
- Unit balance is documented, derived from a stated model and gated by
  `balance_sim`. Leave it until the computer actually fights: no balance claim
  can be tested against an opponent that never attacks.
