# Standard of Iron — live work list

Rebuilt 2026-08-26 from a hands-on play session. The previous `todo.md` (the RPG
recovery plan) was deleted in `48e117a3`; `docs/RPG_PLAYABILITY.md` still points
at this file as the live checklist, so it lives here again. Only items that were
**reproduced in this session** are listed, each with the evidence that produced
it. Nothing here is carried over on faith.

## How this list was produced

Two sessions against the real binary under Xephyr `:99` (llvmpipe, ~15 fps, low
preset), 1920x1080:

1. **As a player** — main menu, skirmish setup, _The Two Fords_ (`map_rivers`),
   Carthage vs one CPU. Selection, move orders, ford crossing, building panel,
   barracks recruitment, camera, day/night, escape menu.
2. **As an observer** — computer-only skirmish on _Sunbaked Terraces_
   (`map_spanish_grove`, 4 slots, 2v2) via the new `--observe` flag, with
   `SOI_AI_TRACE=1` and `SOI_MOVEMENT_TRACE_DIR` set, run for ~500 s of
   simulated time at 4x.

Supporting instruments: `battlefield_gameplay_verifier`, `movement_trace_report`,
and the seven gtest suites (all green at HEAD before and after the changes here).

## Verdict

The presentation layer is well ahead of the game underneath it. Menus, the
skirmish setup screen, the HUD and the art direction all read as a finished
product. The **battle does not**: on the two maps played, the AI never fights,
and the player's own army cannot cross the map's only crossing. As it stands a
skirmish cannot be won or lost by playing well, because nothing on the other side
of the field ever moves.

---

## Landed in this session

- [x] **`--observe <map>` — watch a computer-only skirmish.** Builds an all-AI
      roster from the map's slots (alternating nations, two teams) and launches
      it. Also exposed as `MatchSetupViewModel::start_observed_skirmish(map)` for
      QML. `main.cpp`, `app/viewmodels/match_setup_view_model.*`.
- [x] **Spectator matches no longer end in instant defeat.** Spectator mode
      already existed (`skirmish_loader.cpp:370`, reachable by unchecking "You"
      in the skirmish roster), but the victory service kept evaluating the
      player-centric rules against an owner with no army, so the match ended
      `DEFEAT` after ~0.3 s with a permanent "ARMY BROKEN" overlay and no way
      back to the field. `VictoryService::set_spectator_mode()` now replaces
      those rules with last-team-standing and finalises as `"spectator"`;
      `IronOutcomeOverlay` and `BattleSummary` render that neutrally
      ("Battle Decided") and pick the winner by surviving key structures.
- [x] **The spectated local slot is now AI-controlled.** `MapTransformer` gave
      `local_player_id` `OwnerType::Player` unconditionally, so in a spectator
      match one of the four armies had no `AIInstance` at all and sat still all
      game (confirmed: `SOI_AI_TRACE` printed players 2/3/4 but never 1).
      `MapTransformer::set_spectator_mode()` fixes it, and
      `apply_skirmish_ai_strategies` no longer skips the local owner (the
      `is_ai` check already covers a real human).
- [x] **`SOI_AI_TRACE=1`** — one line per AI player every 10 simulated seconds
      (state, unit/builder/building counts, primary barracks, cumulative
      decisions and applied commands). This is what turned "the AI seems idle"
      into the numbers below; keep it.

---

## Priority 1 — the match is not a game yet

### 1. The skirmish AI plateaus in 40 seconds and never attacks

Two `--observe` runs on _Sunbaked Terraces_, 4 AI players, ~500 s of simulated
time each. From `SOI_AI_TRACE`:

- Every AI reaches **5–6 units and 2–3 buildings by t≈40 s and then never
  changes again**. Of those units, 3–4 are builders: peak fighting strength per
  AI is **one melee unit and one ranged unit**, all game.
- State histogram over the whole run: `Idle` and `Gathering` only, plus player 1
  pinned in `Defending` for **45 of 45 samples**. `Attacking`, `Retreating` and
  `Expanding` were **never entered by any AI**.
- The AI is not asleep — 6,626 completed decisions and 1,888 applied commands by
  t=400 s. It is issuing ~29 commands per 10 s that change nothing.
- Resources tell the same story: wood 250→130 and stone 120→40, but **gold and
  food never move at all** (250/200 at t=0 and at t=500).

What a player sees: the CPU's builders pile up against the south face of their
own barracks and stand there for the rest of the match. No army, no expansion, no
attack, no scouting. Matches the player session, where the CPU opponent never
appeared in ten minutes.

- [ ] Find why the AI never leaves `Idle`/`Gathering`. Start at
      `ai_reasoner.cpp` state selection — the transition into `Attacking` is
      never taken with 1 melee + 1 ranged unit, so either the readiness gate is
      unreachable at this army size or production is the real blocker.
- [ ] Find why production stops at ~2 fighting units.
      `ProductionBehavior::execute` prefers builders while
      `context.builder_count < context.macro_targets.builder_count`; check
      whether `macro_targets` ever lets that condition go false.
- [ ] Player 1 sat in `Defending` for the entire match.
      `ctx.barracks_under_threat` is cleared each update
      (`ai_reasoner.cpp:702`) and re-armed at `:914` — something near that base
      (the neutral/undead owner 99, which holds 4 buildings on this map) reads as
      a permanent threat and latches the AI into defence forever.
- [ ] There is **no harness for the economic AI at all**. The one AI scenario in
      `battlefield_gameplay_verifier` (`bot_skirmish`) hands the AI 12 knights
      already spawned, so it only exercises combat/movement AI — which is why
      every AI test is green while the AI cannot play a skirmish. Add a scenario
      that starts from a real skirmish opening (barracks + builder + healer +
      commander) and asserts army size and territory over a few simulated
      minutes.

### 2. A squad ordered across a ford gets stuck on the deck

_The Two Fords_ (`map_rivers`), 3-unit group (12 builders + healer + commander)
right-clicked onto the far bank:

- The group walked onto the crossing and **stopped**, showing the red
  `ActivityKind::Blocked` marker — which `unit_activity.cpp:228` only raises
  after `MovementComponent::get_stuck_time()` passes the blocked threshold.
- Re-ordering to the far bank moved them ~10 px in 18 s. Formation went
  `Forming up` → `Disrupted`, **cohesion 100% → 0%**.
- Ordering them back east also failed; they never left the deck in either
  direction.

The nav grid is not the problem: `MapSkirmishReachabilityTest` floods
`map_rivers` bank to bank and passes, and `map_bridge_coverage_test` and
`river_bank_walkability_test` are green. The gap is between the pathfinder and
the movement executor.

- [ ] Reproduce headlessly: drive a real squad (not a single body) across
      `north_ford` on `map_rivers` and assert it reaches the far bank. No test
      currently moves anything across a bridge.
- [ ] Prime suspects, all bridge-specific: `movement_orders.cpp:100`
      (`segment_traverses_navigation_portal` blocks every taut-path shortcut on
      the deck), `align_portal_waypoint` (snaps waypoints to the bridge
      centreline), and `local_avoidance_system.cpp:351`
      (`corridor_constrained` is forced true for anything on a bridge, so a
      12-body squad has to file through one line while avoidance keeps pushing
      it back).
- [ ] `slot_is_reachable` runs a full `find_path` per formation member on the
      main thread (`command_service.cpp:275`); on a corridor where most slots are
      unreachable this is also the worst case for the click latency already
      listed in `notes.txt`.

### 3. Both fords are authored wider than they are long

`assets/maps/map_rivers.json`: `north_ford` spans `(79.47, 60.62) → (86.35,
59.37)` — **7.0 long — with `width: 10.0`**. `south_ford` is the same. The river
is 7.0 wide, so the deck exactly covers the water with no landing on either bank,
and it is 40% wider than it is long.

In game this renders as a huge pale stone rhombus with drooping corners, no
parapets and no piers, whose ends do not meet the road on either side. It reads
as an untextured placeholder plane rather than a crossing.

- [ ] Re-author both fords: length ≥ river width + a landing on each bank, width
      ≈ the road it continues (the roads here are ~6–8 wide).
- [ ] Add a content-validator rule: a bridge whose `width` exceeds its span
      length is almost certainly a typo.
- [ ] Give the deck approach ramps and edges; `bridge_bank_landing()` already
      computes a landing, but nothing visual uses it.

### 4. The 60-second gameplay soak is red, and the CI gate is set just under it

`battlefield_gameplay_verifier --all` at HEAD:

| `--seconds`                              | exit | scenarios failing | issue              |
| ---------------------------------------- | ---- | ----------------- | ------------------ |
| 15 (**what CTest runs**)                 | 0    | 0                 | —                  |
| 20                                       | 1    | 1                 | `invisible_damage` |
| 25                                       | 1    | 2                 | `invisible_damage` |
| 30                                       | 1    | 4                 | `invisible_damage` |
| 60 (**the documented pre-release soak**) | 1    | **8 of 8**        | `invisible_damage` |

`damage occurred without a visible action` — after roughly 15–20 s of contact,
every scenario starts dealing damage with no authored combat action on screen.
`docs/GAMEPLAY_VERIFICATION.md` tells you to run the 60 s soak before a release
candidate; it has been failing.

- [ ] Fix `quality.damage_without_visible_action` at the source: something in
      sustained melee lands damage outside the authored strike window. See
      `docs/COMBAT_SYSTEM.md` and the `MeleeIntent`-is-the-authority rule.
- [ ] Then raise `tools/CMakeLists.txt:33` from `--seconds 15` to a duration past
      the failure onset, so the gate cannot pass by being shorter than the bug.
- [ ] `locomotion_flickers` runs 12–66 per scenario and is only failed when it
      exceeds `entity_updates / 100`, which never fires. Either tighten it or
      drop the check.

### 5. Movement quality: 91,777 findings in one quiet skirmish

`movement_trace_report` over the observed match (9,992,786 troop samples):

| finding                       |  count |
| ----------------------------- | -----: |
| `DirectionReversal`           | 58,969 |
| `RepathChurn`                 | 14,028 |
| `AngularAccelerationExceeded` | 11,871 |
| `GaitWithoutMotion`           |  4,913 |
| `HeadingOscillation`          |    584 |
| `Starvation`                  |    541 |
| `ProgressStall`               |    424 |
| `BlockedStepStreak`           |    418 |
| `MissingTerminalOutcome`      |     16 |
| `ArrivalRestart`              |      7 |
| `CollisionPenetration`        |      4 |
| `WaypointRegression`          |      2 |

This is a match in which almost nothing moved, which makes the numbers worse, not
better.

- [ ] `GaitWithoutMotion entity 3 slot 0 ticks 2056..250084 — locomotion state 2
held 4129.14 s at 0.000 m/s accepted`. A unit played its walk cycle for
      4,129 simulated seconds while standing perfectly still, and the analyser
      **accepted** it. This is the visible "treadmill" bug and the threshold that
      hides it.
- [ ] `Starvation — order active 90.0 s without a terminal outcome`, repeating
      every 5,400 ticks on the same entity forever. This is the AI builder stall
      from item 1 seen from the movement side: the order never resolves and never
      fails, so nothing retries it.
- [ ] `AngularAccelerationExceeded 43,200 deg/s²` — a full 720°/s yaw snap in one
      tick, 11,871 times.
- [ ] `RepathChurn` at 14,028 in a match with no combat suggests the repath
      trigger is firing on units that are standing still.
- [ ] The trace itself is unusable at this size: **12 GB of `troops.jsonl` in 19
      minutes**, and `soldiers.jsonl` was **0 bytes** (soldier samples were never
      recorded). Add sampling/size caps and fix the soldier writer.

---

## Priority 2 — legibility of the battlefield

### 6. You cannot tell whose troops those are

Player colour is chosen in the skirmish roster ("Colour Red") and is applied to
buildings, minimap blips and the roster UI — but at RTS zoom the soldiers
themselves are a uniform navy/gold silhouette for every faction. The only red on
a Carthaginian swordsman is a two-pixel sliver at the feet. Selection rings are
blue, which on a two-player match is the _enemy's_ colour.

- [ ] Give troops a team-coloured element that survives a top-down 45° view at
      default zoom — shield face, cloak or crest, not the boots.
- [ ] Make the selection ring read as "mine" rather than as a specific team
      colour, or tint it to the local player.

### 7. Night is unplayable and arrives fast

`day_length_seconds: 2400` with `time_mode: continuous`. At 4x that is a full day
in 10 real minutes; the observed match went from noon to full night and back
inside one session. At night the terrain, the roads and both armies collapse into
the same dark blue, and unit silhouettes are unreadable (see the play session
screenshots — the whole field is a flat navy wash).

- [ ] Put a floor under the night lighting curve for gameplay surfaces, or apply
      the day/night swing to sky and mood while keeping ground/units readable.
      See `docs/RENDER_ART_DIRECTION.md` and the "contrast without a black lift"
      rule.
- [ ] Consider making a continuous cycle opt-in per map; a fixed
      `time_of_day` is the safer default for skirmish.

### 8. Fog of war reads as a hole in the world, not as fog

Zoomed out on `map_rivers`, unexplored ground is drawn as near-black blobs inside
the terrain and the area beyond the vision circle becomes a flat blue-grey sheet,
so the played area looks like a green disc floating on an ocean with hard
polygonal edges. The minimap legend promises Visible/Explored/Unseen; the world
only shows lit/black.

- [ ] Darken and desaturate unexplored terrain rather than replacing it; keep the
      ground plane continuous to the map edge.
- [ ] Give the map edge a skirt or backdrop so a zoomed-out view does not end in
      a cliff over nothing.

### 9. Shadow and light artifacts

- [ ] **A hard-edged opaque navy quad sits on the ground beside every barracks**
      (~40 m to its west on _Sunbaked Terraces_) and **does not move as the sun
      crosses the sky** — it is identical at noon, at dusk and at night. It reads
      as a hole in the ground. Screenshots in the session scratchpad;
      `docs/RENDERING_ARCHITECTURE.md` shadow section and the cascade-fitting
      notes are the place to start.
- [ ] **Stray road-mesh triangles float above the terrain.** Two brown shards
      hang over the hillside east of the player's start on `map_rivers`,
      reproducibly, at multiple camera angles.
- [ ] **The campfire light pool is far too strong**: a hard-edged yellow ellipse
      ~20 m across that recolours the barracks and the surrounding ground orange.
      The flame itself is a flat billboard with visible triangle edges.
- [ ] Five shader uniforms are set but do not exist in the compiled program —
      `u_micro_bump_amp`, `u_micro_bump_freq`, `u_inverse_resolution`,
      `u_ground_ao_radius`, `u_ground_ao_strength`. Either the effects are dead
      or the names drifted; both are worth knowing.

### 10. `iron_ore` props glow violet

18 of them on `map_rivers`, 20 on `map_spanish_grove`. They render as saturated
indigo/violet crystals with white sparkles — the brightest thing on a Second
Punic War hillside, and nothing like iron.

- [ ] Re-shade to rusty brown/grey rock with an ore seam, or make it a mine
      entrance. See `docs/SETTLEMENT_ASSETS.md`.

---

## Priority 3 — HUD and UX

### 11. Three different "population" numbers on screen at once

Selecting the starting force on `map_rivers` showed, simultaneously:

- top bar: **`34 / 400`**
- selection panel: **"3 units · 3 types, 14 soldiers ready"**
- barracks panel: **"Available Population: 140 / 140"**

All three are correct and all three mean different things — the top bar counts
population _cost_ (builder 10 + healer 4 + commander 20), the selection counts
bodies, and the barracks pool is a depletable per-building manpower reserve
capped by `max_troops_per_player`. Nothing on screen says so.

- [ ] Label them: "Manpower 34/400" vs "14 soldiers" vs "Barracks reserve
      140/140", or unify the first and third.
- [ ] `builder` has `population: 10` but `individuals_per_unit: 12`
      (`assets/data/troops/base.json`); every other troop matches. Fix or
      document.

### 12. The right HUD panel is clipped at 1920x1080

Both the builder construction list and the barracks recruit grid are cut off by
the bottom of the screen — the last card row is half-visible and only reachable
by scrolling an unmarked scroll area. 1080p is the reference resolution.

- [ ] Bound the panel to the safe area and make the scroll affordance visible.

### 13. The battle report prints statistics as Roman numerals

The end-of-match report renders Kills / Losses / Trained / Villages / Score in
Roman numerals: `Kills N`, `Losses N`, `Trained XX`, `Score MM`. `N` is a
nonstandard zero and `MM` is 2000. This is unreadable as data.

- [ ] Keep Roman numerals for ordinals and chrome (slots, teams, mission
      numbers); use Arabic digits for measured quantities.
- [ ] The neutral/undead owner ("Iron Sepulcher tomb_1", owner 99) is listed as
      an army in the report with the highest score and zero of everything else.
      Exclude non-contending owners or label them.

### 14. Reopening the skirmish screen duplicates the battlefield list

Reproducible: open Skirmish (7 maps, "VII"), go back, open it again — 14 maps,
"XIV", every entry twice. Each visit appends another copy.

Cause: `MapCatalog::load_maps_async()` clears its own `m_maps`, but the engine's
accumulator never resets — `app/core/game_engine_composition.cpp:397` does
`m_catalogued_maps.append(map_data)` on every `map_loaded` signal.

- [ ] Clear `m_catalogued_maps` when `loading_changed(true)` fires.

### 15. Spectator HUD is still the player's HUD

An `--observe` match shows the full order palette ("Drag a box over your troops"),
the resource bar and "No Barracks Selected" for a spectator who owns nothing and
can order nothing.

- [ ] Replace the order/production panels in spectator mode with a
      per-army scoreboard, and make it obvious which army the camera follows
      (the resource bar currently shows whichever slot happens to be
      `local_owner_id`).
- [ ] There is still no way to start an AI-vs-AI match from the UI. The only
      route is to uncheck your own seat in the roster, which needs a map with
      three or more slots and reads like a mistake. Add an explicit "Observe"
      entry that calls `start_observed_skirmish`.

### 16. Smaller UI defects, all reproduced from a single launch log

- [ ] `CommanderMessagePanel.qml:35` — `Connections` handler `onMessage_changed`
      matches no signal on its target. The handler is dead.
- [ ] `ProductionPanel.qml` — nine `Unable to assign [undefined] to bool`
      warnings at 411, 499, 663, 800, 920, 998, 2533, 2554, 2735.
- [ ] Loading overlay logs `frame 1 of 5 presented at 1994ms (+-1347ms since the
previous one)` — the timer is not reset between loads, so the delta goes
      negative and formats as `+-`.
- [ ] Battlefield-list thumbnails are all the same generic placeholder square;
      the real preview only renders in the detail panel.
- [ ] Formation status sits on **"Forming up"** indefinitely and never reaches a
      settled state; it only changes to "Disrupted" when the group is stuck.
- [ ] A disabled "You" row in the skirmish roster stays at full brightness while
      a disabled CPU row dims.

---

## Priority 4 — load time, memory and startup cost

### 17. The audio bank is decoded, resampled and mastered once per mission load

Every mission load re-runs the full mastering pass (loudness, notches, tilt,
limiter, loop-seam) over the same clips: 97 distinct assets mastered 2x in one
`--observe` session and 3x in a session with two match loads. Mastering is the
expensive part of the load, and it is redone from scratch each time.

- [ ] Cache mastered PCM by asset id for the process lifetime.
- [ ] `MiniaudioBackend: Sound still decoding, skipping play:
"sound_ambient.weather_rain"` fires repeatedly during load, so the weather
      ambience is silently dropped instead of being queued.

### 18. 3,620 GL objects leaked when a match is loaded from the menu

`Buffer destroyed without a current GL context; leaking buffer` x2,439 and
`VertexArray destroyed without a current GL context; leaking vao` x1,181, in one
burst, immediately after the first skirmish finished loading from the main menu.
A direct `--observe` launch (no menu scene) leaks **zero**, so it is the teardown
of the previous scene's resources running without the render thread's context.

- [ ] Free those on the render thread, or defer to a context-current queue —
      `render/gl/buffer.cpp:24` and `:80` already detect the condition.

---

## Verified working — do not spend time here

Confirmed by hand this session; previously-listed concerns about these can be
dropped.

- Main menu, skirmish setup, campaign/tutorial entry points, settings, and the
  Escape → menu → resume path. Escape correctly reaches the menu from a live
  battle and the battle resumes.
- Map catalogue metadata: slot counts, "solo playable", scripted-opposition
  labels and per-map descriptions all match the JSON.
- Selection: box select, `X` select-all-troops, per-unit and multi-unit selection
  panels, unit stat readout, and the builder/barracks context panels all behave.
- Orders reach the simulation immediately — `first_response_seconds = 0.0333`
  (two ticks) in all eight verifier scenarios.
- Barracks recruitment: queue, progress bar, manpower deduction, resource
  deduction and rally-point spawn all work; the recruited unit arrives with the
  right headcount.
- Camera: pan (arrows/WASD), zoom (wheel), `Home` reset, and the minimap
  viewport indicator all behave. Minimap fog states and player blips are correct.
- Fog of war _logic_ is correct (reveal on movement, explored persists,
  `reveal_all` in spectator); only its presentation is at issue (item 8).
- Terrain walkability, river-bank walkability, bridge coverage and skirmish
  start-to-start reachability are all covered by green tests, and the grid-level
  results match what the game does.
- All seven gtest suites pass at HEAD: `simulation_tests` (617),
  `app_tests` (772), `campaign_tests` (140), `persistence_tests` (151),
  `ai_tests` (128), `combat_balance_tests` (681), `arena_tests` (118).
- Unit balance is documented, derived from a stated model, and gated by
  `balance_sim` — leave it alone until the AI actually fights, because no
  balance claim can be tested against an opponent that never attacks.
