# Changelog

All notable changes to Standard of Iron are recorded here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and
the project uses [semantic versioning](https://semver.org/spec/v2.0.0.html).
While the major version is 0, the save format and the mission and map schemas
may change in any release — see [Save compatibility](#save-compatibility).

## [Unreleased]

### Fixed

- **The sanitizer lanes had never run a test.** CTest discovers the test names
  by running each binary once with `--gtest_list_tests`, under a five-second
  budget. An AddressSanitizer build of the render suite needs about twenty-three
  seconds just to reach `main`, so discovery was killed on every scheduled run
  and the lane failed before executing a single test. Discovery now allows five
  minutes. Both lanes also go through `scripts/run-tests.sh` instead of calling
  CTest directly, which runs each suite as one process rather than spawning one
  per test case — the difference between minutes and hours under a sanitizer —
  and drops `--no-tests=ignore`, which would have reported success had discovery
  ever come back empty instead of erroring.
- The whole-project clang-tidy pass that `CONTRIBUTING.md` describes, and that
  pull requests explicitly defer to by tidying only the files they touch, was
  missing from the scheduled workflow entirely. It runs now and publishes its
  findings, with a missing clang-tidy treated as a failure rather than as files
  silently left unchecked. It does not gate on the findings themselves yet —
  there are about 120 of them on the tree already, and a gate that starts red
  is one nobody reads.
- The coverage report piped `gcovr` into `tee` without `pipefail`, so a failed
  report still published an empty summary and passed.
- The defects the sanitizer lanes found once they could run:
    - **Use after free in four tests.** They read an entity through a raw pointer
      after the system under test had destroyed it — the wall builder queue and
      the civilian delivery — which is a heap-use-after-free. They hold the entity
      id across the update now. `world.get_entity(p->get_id()) == nullptr` is a
      dangling read by construction, so all four instances of that shape went.
    - **Leaked QML singletons.** `IconArtLibrary` and the test-side `GlyphProbe`
      hold no state and expose only static methods; the instance exists so QML has
      something to call them through. Each registration allocated a fresh one that
      nothing freed. This alone failed the QML suite while all 233 of its tests
      passed. One shared instance each now.
    - **A leaked `QTemporaryFile` per mission written** in the wave archetype
      tests. `setAutoRemove(false)` is what keeps the file on disk after the
      helper returns, so the object never needed to be heap-allocated.
    - Process-lifetime allocations inside D-Bus, the GL driver and the QML engine
      are listed in `tools/lsan-suppressions.txt` with the reason for each. A leak
      with one of our own frames in it is not suppressed.
    - **The gameplay verifier asserted a wall-clock tick budget.** Its 33 ms
      hitch check was the only thing still failing it under AddressSanitizer,
      where a tick measured 106 to 135 ms on a busy machine and 7 to 22 ms on an
      idle one — the instrumentation and the load, not the game. Sanitizer and
      coverage builds define `SOI_INSTRUMENTED_BUILD` and skip that one timing
      claim; every behavioural check the verifier makes still runs everywhere.

### Changed

- **The simulation kernel is one static library per domain, and the build
  enforces the layering.** `game_sim` used to hold combat, movement,
  navigation, formations, economy, units, terrain and wildlife in a single
  archive, with `scripts/module_rules.json` counting the forty-three edges that
  pointed the wrong way between them. Those edges are paid down and the kernel
  is now `engine_core` → `soi_world` → `soi_navigation` → `soi_units` →
  `soi_formations` → `soi_movement` → `soi_economy` → `soi_combat` →
  `soi_wildlife` → `game_sim`, each linking only the layers below it, so a
  domain that starts reaching for one above it fails to link. `game_sim` still
  links all of them, so every existing consumer keeps its one-line link. The
  module checker no longer carries a baseline: the first wrong-way include
  fails the build, and so does a source listed in a target its module does not
  own. What moved to make that possible: the ambient service binding
  (`OwnerRegistry::instance()` and friends) resolves through a leaf in
  `engine_core` that the session fills in, instead of being defined in the
  session; the troop file's formation block is carried on the catalogue entry
  and read by the formation library afterwards, instead of the catalogue loader
  writing the role registry; the content bootstrap that loaded formations,
  troops and nations moved out of `NationRegistry` into
  `Game::Systems::initialize_default_content`; the formation runtime raises a
  flag after a replan and a movement-side system issues the orders; a unit's
  run-mode stamina helper sits with the registries; walls and gates are
  navigation, and the map-to-world spawner is a unit concern.
- **A binary that uses per-match services owns its session.** There is no
  longer a lazily created default `SessionContext` behind
  `SessionContext::active()`; whether it was linked in depended on which
  archive members the linker happened to pull once the kernel was split. The
  game, every tool and every test main construct a session and make it active,
  and an accessor called with none bound aborts with a message saying so.
- **Every order goes through the command queue, from every issuer.** Trading
  at a marketplace, a commander's aura, rally and flag rally, entering and
  leaving formation mode, deploying an army formation, sending builders to a
  construction site or a resource, delivering civilians to a barracks and
  repairing a structure used to be applied by app code, the AI applier and the
  arena harness writing gameplay components themselves — three copies of the
  same assignment logic, each with its own idea of the rules. Each is now a
  `Game::Command` payload (`Trade`, `UseCommanderAbility`, `SetFormationMode`,
  `DeployFormation`, `ReleaseFormation`, `StartConstruction`, `StartHarvest`,
  `DeliverCivilians`, `RepairStructure`) that is validated and dispatched like a
  move, so a player click, a computer opponent and a scripted wave are judged
  by the same code — and every payload is plain data, which is what a replay
  file or a network transport carries. The app keeps only feedback: it may ask a
  service whether an order would be accepted to phrase a refusal, then submits.
  Consequences a player can notice: builder construction is paid for when the
  crew is assigned (it was free for the human player before, and only the AI
  paid); build times come from one table for both issuers, so the computer
  opponent's tower now takes 20 s rather than 12, its barracks 10 s rather
  than 15 and its marketplace 10 s rather than 12; and the marketplace
  buy/sell buttons submit an order the queue applies at the top of the next
  tick rather than changing your stock inside the click.
- **The last three client-side mutations are payloads too, and the boundary
  is enforced.** Wall plans (`PlaceWallPlan`: the drag's anchor and target on
  the wall grid, re-planned from the world when applied so a stale preview
  cannot place a wall the match would refuse), crew-less building placement
  (`PlaceBuilding`) and the builder "placement preview" flag are gone from
  `app/`. `WallPlanService` and `StructurePlacementService` (game side) give
  the HUD its preview and refusal text and the dispatcher its ruling from one
  function each; the `is_placement_preview` flag on `BuilderProductionComponent`
  is removed — starting a placement is now a client gesture that touches no
  builder, so cancelling it changes nothing and a crew keeps working until the
  order lands (ordering the same crew back to its own tree is allowed, and the
  dispatcher re-seats the claim). `scripts/check-command-boundary.py` fails
  the build if anything under `app/` or `ui/` calls a mutating service or
  writes a builder's task fields, with the commander's first-person mode as
  the one allow-listed exception.
- **Replays.** `--record-replay <file>` writes the launch, every accepted
  command with its tick, and the world digest every 30 ticks; `--replay
<file>` launches what the file describes and lets it drive the match with
  local input and the AI shut out; `--replay-verify` exits 0 if every recorded
  digest matched and 12 at the first tick that did not, and `--skip-briefing`
  starts a scripted mission unpaused. `game/command/command_codec.{h,cpp}` is
  the one place a payload is written and read (JSON), with a round-trip test
  over every alternative of the variant that fails when a payload is added
  without a sample.
- **Determinism is checked, not assumed.** `game/session/world_digest.h`
  fingerprints the simulation (entities, stock, tick, rng draws);
  `battlefield_gameplay_verifier --determinism-runs N` runs every scenario N
  times and prints the first divergent tick and entity; `ctest` carries
  `simulation_determinism` and `headless_replay_round_trip`. The one
  nondeterministic gameplay draw found — commander crit rolls seeded from
  `std::random_device` — now comes from the session's `DeterministicRng`,
  which the ambient binding exposes.
- **The computer opponent decides on a schedule, not on a race.** `AISystem`
  used to apply a decision on whatever tick the worker thread finished it,
  so two runs of one match disagreed on when the AI moved — the determinism
  check caught it on its first run. A decision now lands a fixed six updates
  after the snapshot it was taken from, and the simulation waits for the
  worker if it is not done by then (it normally is, by a wide margin).
- **The simulation with no window.** `SessionContext::advance()` /
  `step()` own the wall-time-to-ticks loop the frame orchestrator used to
  hold, and `soi_headless` (a session, the runtime system registry, the AI
  and that loop, with no Qt Quick or renderer linked) records and verifies
  replays of the battlefield scenarios; the map-to-match setup that would let
  it host a real skirmish is still in `app/core/skirmish_loader.cpp`.
- **Ambient lookups can only go down.** `scripts/check-ambient-instances.py`
  counts `X::instance()` / `SessionContext::active()` per directory against
  `scripts/ambient_instance_budget.json` and fails the build when a directory
  grows one; `--write` lowers the ceiling after a clean-up.
- **Construction costs and build times live in
  `assets/data/construction/catalog.json`**, loaded at content bootstrap;
  the C++ table is the fallback and a test keeps the shipped file in step
  with the wall constants the preview quotes.
- **`ProductionService` has one production entry point.** The barracks and
  home paths were two functions with the same shape; `start_production` now
  takes the building and applies that building's rules (commander
  recruitability and manpower for barracks, committed civilians for homes,
  queue depth for both), and `can_start_production` gives the same ruling
  without placing the order, which is what the HUD reads to explain a refusal.
- **Marketplace availability is read from the world, not counted.** Whether an
  owner can trade used to be a per-owner counter kept in step by the
  marketplace factory, the damage pipeline and nation collapse, and rebuilt on
  load; it is now a query over the standing buildings, so there is nothing to
  register, unregister or rebuild.

- **Infantry no longer skate.** The walk clip drew the planted foot 1.08 m under
  the body per cycle and was played at a cadence of about 0.9 s whatever the
  unit's speed, which is a walk for someone travelling 1.2 m/s. Infantry travel
  2.0–2.5 m/s, so nearly half of every step was the foot sliding across the
  ground — the single largest reason the walk read as gliding rather than
  walking. The stride and the cadence are now derived from one number, the
  distance the body
  covers in a cycle: the clip takes a 0.79 m step and the cycle time is that
  distance divided by the actual ground speed, so the foot that is down stays
  where it was put at any speed a unit is given. The same rule drives the run.
  Two tests measure the retreat of the planted foot against `speed × cycle_time`
  and fail if they drift apart.
- The rest of the walk was rebuilt around the longer stride: the feet track near
  the midline instead of under the shoulders (0.17 m apart, not 0.40 m), the
  pelvis leans over the leg that is carrying the weight instead of away from it,
  the shoulders and pelvis counter-rotate in time with the arms rather than a
  quarter cycle out of phase, the pelvis drops on the swing side, the heel
  leaves the ground halfway through the stance instead of in the last third, and
  the ankle now peaks early in the swing while the knee is folded. The arms
  swing on an arc around the shoulder: driving the hand fore and aft used to
  push it past the arm's reach limit, where the clamp locked the elbow straight
  and ate most of the swing.
- The first-person camera bob and its footstep cue are derived from the walk
  clip's stride distance, so the step you hear is the step the legs take.
- `humanoid_preview --report` measures a gait as well as drawing it: stride
  travel, the retreat of the planted foot per cycle, foot lift, hand swing,
  pelvis bob and sway, shoulder twist and step width, plus the ground speed each
  clip is skate-free at for a given cycle time.
- The scheduled workflow is called **Weekly** and its jobs, cache keys and
  documentation say so. The cron moved to Mondays some time ago; only the name
  had gone on claiming otherwise.
- The sanitizer and coverage lanes build only the test binaries instead of the
  whole application, which is what they then run.

### Removed

- `game/systems/battlefield_definitions.{h,cpp}` and `game/audio/music.{h,cpp}`,
  which nothing compiled or included.
- `BuilderProductionComponent::is_placement_preview` (and its save field);
  `App::Utils::structure_work_position`/`barracks_delivery_target_position`;
  the app's private copies of the wall-plan and site-validity logic;
  `InputCommandHandler::reset_movement`; `app/utils/movement_utils.h` is now a
  declaration header with the bodies in `movement_utils.cpp`.
- `ProductionService::start_production_for_first_selected_barracks/home` and
  `set_rally_for_first_selected_barracks`; the AI applier's private
  `BUILD_TIME_*` table; `App::Utils::structure_work_position` (now
  `CommandService::structure_work_position`, one definition shared by the
  dispatcher, the arena and the tests); and the unused `reset_movement`
  wrappers on `GameEngine` and `CommandController`.
- The `Temporal` soldier-cull reason left the profiler with the humanoid
  prepare pass but `ui/gl_view.cpp` still named it, so the executable did not
  build; the reference is gone.

## [0.1.0] — 2026-08-09

The first tagged release, and the first build published as a download rather
than as something you compile yourself.

### Campaign and content

- The **Second Punic War** campaign: nine missions from the crossing of the
  Rhône through the Alps to Zama, with Trebia, Trasimene, Ticino and Cannae in
  between.
- Thirteen maps, covering the campaign battles plus standalone skirmish terrain
  (forest, mountain, rivers, the Spanish grove, the Iron Sepulcher watch).
- Rome and Carthage as playable nations, each with its own units, ornaments and
  commander roster.

### Gameplay

- Real-time battles with formations, including testudo and shield-wall
  defensive layouts, and a formation planning interface.
- A first-person commander mode alongside the top-down army view, each with its
  own control scheme.
- Economy: gathering timber, stone and iron, hauling to barracks stockpiles, and
  a builder Auto Gather standing order.
- Siege play against walls, gates and towers.
- Ambient settlement life and wildlife — wolves, sheep, horses and elephants
  with their own gaits and behaviour.

### Presentation

- A custom OpenGL 3.3 renderer with multi-pass drawing, batching and culling,
  plus an optional GPU-driven crowd path on drivers that support compute
  shaders.
- A CPU rasteriser fallback (`--force-software`) for machines without a 3.3
  driver. It is a diagnostic, not a supported way to play.
- The "Iron and Ember" interface: activity medallions, Roman numerals, and a
  selection summary that regroups itself as a selection grows.

### Accessibility

- Every gameplay command is rebindable to a key or mouse button, with conflict
  detection scoped per control context.
- Team palettes and ring patterns for colour identification, and a camera motion
  scale.
- Audible feedback for controls that refuse an action.

### Languages

- English, German, Spanish, Brazilian Portuguese, and Arabic. Arabic switches the
  whole interface to a right-to-left layout.

### Platforms

- Linux (AppImage), macOS (DMG) and Windows (ZIP), each built and smoke-tested
  through a real OpenGL self-test before publication.
- **macOS builds are universal**: Intel and Apple Silicon both run natively. The
  architectures are read from the installed Qt rather than assumed, and the build
  fails if the binary does not come out carrying them.
- The renderer now asserts at startup that the driver actually granted an OpenGL
  3.3 Core context, rather than assuming the one it asked for. All three release
  workflows fail if it did not.
- Linux and Windows now prefer OpenGL 4.5 Core, exposing the existing 4.3 GPU
  crowd path and 4.4 persistent-buffer path when available; macOS explicitly
  requests Apple's 4.1 ceiling and keeps the complete 3.3 fallback.
- The macOS and Windows toolchains are now exercised from Linux, where the
  project is actually developed: every translation unit is reparsed with Clang
  and libc++, every shader is compiled by a spec-literal GLSL front end, and
  the sources are scanned for what MSVC and NTFS reject. `make portability`
  runs it locally; CI runs it on every pull request and every build. The
  entries under **Fixed** below are what it found on its first run.

### Fixed

- The Windows package no longer copies Qt 6.8's Mesa 11.2 software renderer,
  which exposes only OpenGL 3.0 on the hosted runner. It bundles a pinned,
  checksum-verified Mesa 26.1.6 llvmpipe runtime and verifies a real 4.5 Core
  gameplay frame before publishing.

- **Optimised builds had deleted every NaN and infinity check in the game.**
  `-ffast-math` (and MSVC's `/fp:fast`) licenses the compiler to assume no
  operand is ever NaN or infinite, so `std::isfinite` folds to `true` and the
  clamp behind it disappears — verified on the project's own release flags.
  Around forty guards were affected, on volumes read back from the settings
  file, camera angles and impact geometry, and so was every use of
  `infinity()` as a sentinel in pathfinding, AI target selection and bounding
  boxes. Debug builds were unaffected, which is why it never showed in
  development, and the three shipped platforms did not agree on the outcome.
  Linux and macOS now build with `-fno-finite-math-only` and Windows with
  `/fp:precise`.
- Two places computed a value from a variable and incremented it in the same
  function call, where the order is unspecified and compilers differ: the
  Roman market stall's produce, and the balance simulator's spawn jitter — the
  latter meaning a seeded simulation did not have to produce the same
  battle on Windows as on Linux.
- `stone_instanced.frag` declared a function named `noise3`, which is a GLSL
  built-in with a different return type. Mesa does not declare the built-in so
  it compiled here; a spec-literal front end rejects the shader, and a shader
  that fails to compile is a silently missing object rather than a crash.
- Eight files used `M_PI`, which MSVC's `<cmath>` does not define. They
  compiled on Windows only because Qt's `qmath.h` defines it as a fallback and
  happened to be included first; they now use `std::numbers`.
- **A crash on drivers that advertise compute shaders on a context below 4.3.**
  The GPU crowd-culling path is guarded by a capability probe that accepted an
  extension string, but its shaders are `#version 430` and need OpenGL 4.3 to
  compile at all; on such a driver the game segfaulted inside the GL driver
  partway through the first frame. The probe now gates on the context version,
  which is both necessary and sufficient. Verified against simulated 4.5, 4.1
  (Apple's cap), 3.3 and 3.1 drivers.
- macOS bundles are re-sealed after deployment and asset copying. Apple Silicon
  refuses to run a binary whose signature does not match its contents, and every
  step after `macdeployqt` had been invalidating it.

### Known limitations

- The AI is capable but unfinished: it lacks siege groups, flankers, and
  regroup-after-failed-push logic. `docs/AI_ARCHITECTURE.md` is candid about
  where the gaps are.

### Audio provenance

- Thirty-two battle and alert effects that had been generated with AudioCraft
  were **rebuilt from CC0 recordings** and are no longer restricted. The recipe
  is `tools/audio_field/battle.py`. Rebuilding them also fixed two defects: 28
  of the 32 decoded above 0 dBFS and clipped, and every alert was padded to
  exactly 10 seconds because that is AudioCraft's generation length.
- Every effect now carries a `source` tag in the audio manifest — `synth` for
  the 106 generated by this repository's own code, `field` for the 69 cut or
  composed from CC0 recordings.
- The thirty-one voice lines were recorded by the project author.

### Distribution restriction

The twenty music tracks were generated locally with Meta's AudioCraft, whose
model weights are licensed CC BY-NC 4.0 — **non-commercial use only**. Standard
of Iron is MIT-licensed and distributed free of charge, which is consistent with
that. Selling the game, or bundling it into anything sold, would require
replacing those tracks first — they are now the only part of the game carrying
any such restriction. `THIRD_PARTY_LICENSES.md` records the details.

## Save compatibility

Saves are keyed to a schema version. When the game finds a save database written
by a different version, it **discards it and starts a clean one** — there is no
migration path, and campaign progress goes with it.

While the project is at 0.x this is the intended behaviour, and any release may
trigger it. Before 1.0 the policy needs to become one of: migrate forward, or
warn the player before the wipe rather than only writing to the log.

[unreleased]: https://github.com/djeada/Standard-of-Iron/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/djeada/Standard-of-Iron/releases/tag/v0.1.0
