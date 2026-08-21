# Changelog

All notable changes to Standard of Iron are recorded here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and
the project uses [semantic versioning](https://semver.org/spec/v2.0.0.html).
While the major version is 0, the save format and the mission and map schemas
may change in any release — see [Save compatibility](#save-compatibility).

## [Unreleased]

### Added

- **Farms, and food that means something.** Both nations can raise a **Farm**
  (40 wood, 10 stone) that grows grain in sixty-second cycles — furrows, sprouts,
  green stalks, then a golden field — and a builder reaps it with **Collect** for
  60 food, after which it sows itself again. Builders can also **slaughter a
  sheep** for 35 food; the animal is held still while they work and the herd
  respawns on the map's own timer. Both loads are hauled to the barracks yard,
  which now stacks grain sacks, and a carrying builder shoulders a sheaf. Reaping
  starts a standing round like felling does, except that a farmer waits by a green
  field for the next crop instead of walking off. **Auto Gather** treats ripe farms
  as nodes and gained a _Food first_ setting. Ripe farms and sheep light up as
  interaction targets, the Farm card and a crop readout sit in the production
  panel, and the map editor and arena know the building.

    Food is what civilians eat: **recruiting a civilian at a Home costs 20 food**,
    so homes turn food into manpower and farms keep that running. The rest of the
    economy was rebalanced around it so gathering matters again — troops cost
    roughly a third of a wood trip and a third of an iron trip apiece (archer 12
    wood; swordsman 12 wood, 10 iron; cavalry 20–22 wood, 10–14 iron), builders
    cost 10 wood, and the AI keeps a wood and iron reserve for recruiting instead
    of gathering only for buildings. The full table is in
    [docs/FOOD_AND_FARMS.md](docs/FOOD_AND_FARMS.md).

- **Builders work like builders.** Construction no longer borrows the sword-swing
  clip: the humanoid bake gained dedicated hammer, saw, chisel, kneeling-chisel and
  sickle work loops, and the tool in a builder's hand follows the job — a mallet at
  the tree line, a chisel at the quarry and the ore seam, a sickle in the field, and
  the seeded hammer/saw/chisel mix on a building site. Cornered builders now fight
  with the mallet in hand instead of bare fists, and civilians defend themselves
  with a wooden cudgel instead of their knuckles.

- **The camera says what it does, and every command takes two keys.** Bindings
  hold a primary and an alternate chord at once, so panning answers to the
  arrow keys _and_ to `WASD` with neither being second-class; each has its own
  button in Settings › Controls, conflict checking covers both, and **Default**
  restores the pair. Making room for `WASD` moved **Attack** to `C` and **Stop**
  to `Z`.

    The two rotations are no longer both called the wrong thing. `Q`/`E` rotate
    (yaw) and keep their keys. What shipped as "orbit camera left/right" on
    `R`/`T` never orbited — it raised and lowered the camera — so it is now
    **Tilt camera up/down** on `Ctrl+Up`/`Ctrl+Down`, which also gives `R` back
    to the commander rally flag it used to share. Zoom and reset gained keys of
    their own (`PgUp`/`PgDown` and `Home`) instead of living only on the wheel
    and a top-bar button, and every camera command now carries a sentence in
    Settings saying what it does. A keymap saved by an older build follows the
    rename, and anything you rebound yourself stays yours.
    See [docs/CAMERA_CONTROLS.md](docs/CAMERA_CONTROLS.md).

- **One unit is positioned the way an army is.** Right-pressing the ground with
  a single troop selected opens the same planner the army gets: the press marks
  where it will stand, dragging is the direction its front will face, and the
  release sends it — a shield wall can finally be told which way to look
  without stacking two units to get the gesture. The panel switches to
  **Position ‹unit›** with a live facing readout instead of offering Line and
  Column to one block, the fine-tuning section stays out of the way, and no
  one-member "army formation" is registered — pulling one unit out of a formed
  line no longer risks shrinking that line to itself. `F` with one troop opens
  the same planner in place, and its drag aims rather than drawing a front.

- **Aiming a formation looks like aiming.** While the right button is held the
  arrowhead sits under the cursor with a beam back to the anchor and a pivot at
  the block's centre, the footprints are on the ground from the press itself
  rather than after the first mouse move, and the panel's readout — facing,
  ranks × files, in-place count — updates as you drag or turn the wheel instead
  of freezing on the numbers from the last placement. The hint at the bottom of
  the panel now describes the gesture actually in progress (hold and release
  for the right-drag, click to deploy for `F`), and the field manual's
  **Giving orders** and **Formations and lines** entries explain the drag.

- **The skirmish screen reads like the campaign screen and says what the match
  will be.** It now fills the window the way the war table does — a titled
  header with the **← Back** pill, a full-height **Battlefields** list whose
  rows carry the slot count, and a briefing that puts the map preview beside
  the description with **Slots**, **In play**, **Sides** and **Opposition**
  read out as chips, so a map that brings its own enemies no longer looks the
  same as one that needs a second player. The roster became an **Order of
  Battle**: every seat is one row of labelled chips for colour, nation,
  commander and team, the seat you play is marked **You** and can be moved to
  another slot by clicking it, and the dead space below now holds your
  commander's role, bonus, aura and rally line — text the game already had and
  never showed. Footer keys spell out `↑ ↓`, `Enter` and `Esc`, and **Play**
  explains in a tooltip why it is refusing when the setup is not startable.

### Changed

- **Large battles cost a fraction of what they did, and the engine can now say
  why.** A thousand-unit battle line spent about half a second of CPU on every
  simulation tick. Nearly all of it came from one line: the combat mode
  processor asked "is there an enemy in melee reach?" by walking every unit in
  the world, once per attacking unit, computing formation contact geometry for
  each — roughly 620 full-world scans and 620,000 entity handles resolved per
  tick. It now asks the world's spatial index for the units actually inside the
  attacker's reach. The same battle line ticks in about a fifth of the time,
  with the world digest unchanged.

    The scaffolding around that fix is the more durable part.
    `Engine::Core::WorldSpatialIndex` belongs to the world it indexes, is
    rebuilt once per tick, and now answers the patrol, healing and combat
    proximity questions that used to scan everything.
    `World::view<A, B>()` iterates matching entities without allocating, and the
    materialising query it replaces was renamed `collect_entities_with` so that
    its cost is visible at the call site. `scripts/check-world-scans.py` keeps
    the number of full-world scans from growing back.

    `Engine::Core::SystemProfiler` reports per-system tick times, the queries
    each system opened and how many candidates they examined, and attributes
    materialising scans to the source line that made them — which is how the
    line above was found. `build/bin/sim_benchmark` is the repeatable 1k/5k/10k
    workload behind those numbers.

    Simulation phases are now named (`Engine::Core::SystemPhase`), structural
    changes can be deferred to the barriers between them
    (`Engine::Core::DeferredMutations`), and a system may declare what it reads
    and writes so a future scheduler can check rather than assume. Nothing
    reordered: a test asserts the phases only ever advance down the registry.

    The renderer's battle optimiser stopped pretending to cull. Its
    `should_render_unit` hook had been `return true` with a counter behind a
    mutex; the real culling is frustum and fog, well before it. What remains —
    animation throttling and the batching ratio — reads one immutable
    per-frame snapshot instead of a lock and three atomics per unit, and the
    object belongs to the renderer rather than to the process.

### Fixed

- **Animals beside a campfire no longer shine like reflectors, and the
  commander portrait is lit again.** Two separate things. The rain sheen was
  applied at full strength to fur and hide, so a wet sheep or horse next to a
  fire glinted like a wet helmet; coats now take a fifth of it, and the
  firelight term has a soft knee so pale wool warms up instead of blowing out.
  Separately, the uniform-handle rework left one raw `glUniform3fv` for the
  rigged role-colour array, so every single-draw rigged creature (the
  commander's portrait bust, a lone horse or elephant, a sheep that missed the
  instanced batch) rendered with garbage palette colours - usually black. It
  now goes through `Shader`, a source test forbids raw `glUniform*` outside it,
  and the new `wildlife_firelight` arena fixture (sheep, wolves, a horse and an
  elephant around one fire in the rain at night) exists to review both.
  Creatures also take only half the moon key after dark, so a white coat under
  the night sky reads as moonlit rather than floodlit.
- **Troops, horses and elephants no longer look like they hover.** Every
  creature now carries a grounding blob - a soft occlusion ellipse centred
  under its footprint, tilted to the slope and elongated along its facing -
  whether it is marching, fighting or standing, at every preset; it used to
  exist only for a few dozen idle soldiers, so anything moving had nothing
  anchoring it to the ground and the noon sun hid the cast shadow under the
  body. The blobs draw in a handful of instanced batches, so a thousand of
  them cost a few draw calls. Where the cascaded shadow maps are off (Low) the
  blob keeps a sun-offset lobe so there is still a shadow. On the soldiers
  themselves the lowest span of the figure sits in its own occlusion, boots
  and straps are leather again instead of team-coloured slippers, helmets,
  mail and blades get a real metal highlight and sky reflection, leather a
  soft sheen, skin a warm shadow side, and the shadow-side floor came down so
  the figures have form under the new light.
- **The world casts shadows again.** Trees, ruins, tents, boulders, statues and
  ore never reached the shadow map: the scatter shaders take their
  view-projection from the shared frame block, and the shadow pass left the
  camera's matrix in it, so every prop landed at the wrong depth. The cascades
  were also sliced against the camera far plane instead of the shadow distance
  (2.5x too coarse on High), and the biases were expressed in depth units that
  turned into metres on the far cascade. Cascades are now fitted to the ground
  the camera can actually see, biases are authored in metres with a normal
  offset that keeps shadows planted at the feet, sampling is hardware PCF with
  a penumbra that sharpens on a clear day and widens under cloud, a tall caster
  outside the camera-distance band still throws its evening shadow into it,
  boulders and terrain cast, the skyline mountain ring does not, and the last
  cascade fades out instead of ending on a line. Villages at four o'clock look
  like villages at four o'clock.
- **Firelight no longer dims inside a sun or moon shadow.** Every receiver shader
  added its local lights before applying the directional shadow, so a campfire
  beside a house lit the ground less on the shaded side of the wall. The local
  term now joins after the shadow.
- **Main builds again after the farms merge.** The humanoid clip table gained the
  five construction clips at indices that collided with the taunts, and the clip
  count stayed at 63; the constants now follow the manifest order (taunts 61-62,
  construction 63-67, 68 clips).
- **Formations turn like soldiers now, not like a lawnmower blade.** A unit
  changing direction used to spin every soldier rigidly around its anchor, so
  the outer files strafed sideways along arcs with their bodies locked to the
  unit's facing. Each rendered soldier now walks to his slot at a plausible
  foot speed, pivots on his own feet toward where he is going, and eases back
  square once he has re-dressed — cavalry wheels instead of sliding. Purely a
  render-side change; the simulation's positions and hit math are untouched.
- **Twelve switches were silently ignoring enum values that had been added
  since they were written.** Each listed every case it knew about and had no
  `default`, so a newer enumerator quietly took the fallthrough: elephants were
  excluded from run mode, the three commander attack actions got no baked
  weapon trace and no sword animation, nine of the newer terrain props were
  never given an animation time, wildlife had no formation profile, and the map
  editor's Gate tool had no name in the status bar. Every case is now spelled
  out, keeping today's behaviour, and an unhandled enumerator is a build error
  from here on. Gates and wildlife also join the structures that deal no
  structure damage, which is what the rest of that switch already said.

- **A command-line `--quality` override wrote its value into a copy and threw
  it away.** The real assignment happened twenty lines later through a
  `const_cast` on a getter's return; the copy above it did nothing.
  `GraphicsSettings` has a `set_shader_quality` now, and the `const_cast` is
  gone.

- **The healer renderer kept a second copy of its base class's visual-spec
  cache.** It redeclared both members, so it baked into its own pair while the
  base's stayed empty — the other four renderers of that shape correctly use
  the inherited ones. Removed, along with a `sizeof`-sized pile of smaller dead
  code the newly promoted warnings found: two accumulators in the biome
  renderer that cost four array reads per terrain sample and were never read, a
  dropped shade in the Carthaginian light armour, an unused shader-cache field,
  a dead `operator==`, and eleven lambda captures that captured nothing.

    A hundred and fifty-three range-for loops over `QJsonArray` bound `const
auto&` to a temporary, because that iterator yields `QJsonValue` by value:
    they read as a reference into the array while quietly constructing a copy
    per iteration. All of them now say `const auto`, and the diagnostic is an
    error.

- **A finished AI decision no longer reads a nation that has been freed.** An
  AI job runs on a worker thread and holds raw pointers into the global
  registries for as long as it takes: its context points at a `Nation` inside
  `NationRegistry`, and the production behaviour walks that nation's troop
  list. The job outlives the test body that started it, so a fixture holding
  its world as a member and clearing the registries in teardown freed the
  nation under a thread still reading it. AddressSanitizer caught it as a
  heap-use-after-free in `ProductionBehavior::execute`, and it is a plausible
  cause of the weekly sanitizer and coverage lanes going red on a suite that
  passes without instrumentation: a use-after-free read returns whatever is
  there now.

    The game already observes the ordering — `SkirmishLoader` stops the AI
    workers before it touches global state — so the fixture now does the same
    through `tests/support/ai_quiesce.h`, which explains the contract for the
    next fixture that keeps a world alive across teardown.

- **The Windows build compiles again, and a Linux gate now says when it will
  not.** `game/audio/cue_ids.h` declared a `std::array` of every audio cue
  without including `<array>`. libstdc++ and libc++ both hand that header out
  as a side effect of including something else, so the file built in every
  Linux and macOS lane; MSVC's standard library does not, and the first Weekly
  packaging run failed on it two days after it was written. Ninety more files
  were leaning on the same courtesy for `<utility>`, `<cstddef>`,
  `<algorithm>` and a dozen others, and now spell out what they use.

    A fourth portability pass reads the include graph rather than compiling
    anything, because no compiler on a Linux machine can see this class of
    break — both of the standard libraries here leak the header that MSVC
    withholds. It runs on every pull request, which is where a Windows problem
    has to be caught: Windows and macOS are only built weekly. The Clang and
    libc++ pass that `CONTRIBUTING.md` has described for months but that no
    workflow ever ran now runs in the weekly lint job, which already builds
    what it needs. And the six ECS component types that were forward-declared
    as `struct` and defined as `class` were reconciled: the same divergence one
    step further along, since the Microsoft ABI mangles the two apart.

- **Commanders can be chosen again on the skirmish screen.** The screen looked
  for the commander list on a property that no longer exists, so every lookup
  came back empty and both seats were stuck on a placeholder called
  "Commander" that would not cycle. It asks the match-setup model directly now,
  and the roster shows the real commander, their nation's default, and what
  each one brings.

- **Teams are numbered from I, and only as many as there are players.** Team
  numbering started at the zero glyph — a two-player match read "Team N" versus
  "Team I" — and cycling ran one team past the roster, so two players could be
  spread across three teams. Teams now start at I, the mark beside the number
  matches it, and cycling stops at the number of seats.

- **Right-drag placement works again.** Every right press in the battle view
  tried to write a camera flag that had been made read-only, and the resulting
  script error aborted the handler before the order was ever forwarded — so the
  drag-to-face planner never opened and every right-drag ended as a plain move
  to the release point. The stray write is gone and a regression test keeps it
  out.

- **A battle no longer opens with your nose against one soldier's shield.**
  Every map authors the camera that frames its engagement — 18 units on the
  48-tile Sepulcher Watch, 273 on the 650-tile field at Cannae — and the opening
  shot and the Reset button both ignored all of it and snapped to a flat 12
  units on every map alike. Both now derive their framing from what the map
  authored, so a campaign mission opens on your camp with the battlefield around
  it and Reset returns to that same view instead of diving to ground level.

- **The camera tells you how to move it.** A compact legend lists all seven ways
  to move the view — edge scroll, keyboard pan, right-drag, wheel zoom, `Q`/`E`
  rotate, the minimap, and the Follow and Reset buttons — with the live key
  bindings and whether edge scrolling is currently on and how wide its band is.
  It opens by itself on a new profile's first battle, is dismissed for good once
  you close it, and comes back from a button in the top bar. The field manual
  gains a matching **Camera** tab, and both read one list, so a rebound pan key
  updates everywhere. Settings now says what the edge-scroll slider actually
  does — it sets both the width of the band and the speed inside it — and prints
  the resulting band in pixels at your interface scale.
  See [docs/CAMERA_CONTROLS.md](docs/CAMERA_CONTROLS.md), which also carries a
  manual regression checklist for windowed, fullscreen, high-resolution,
  interface-scaled and right-to-left layouts.

- **The battle runs at 3× and 4×, and the speed is never hidden.** The top bar
  now offers 0.5×, 1×, 2×, 3× and 4×, and `+` and `-` step through them without
  leaving the field. The active speed stays lit while the game is paused (it
  used to grey out, so a paused game showed no speed at all) and can be changed
  from there, the narrow-window selector lists every speed instead of three, and
  the bar reads the speed back from the simulation, so a save loaded at 3×
  displays 3×. The mission briefing points at the control before the first
  battle. `--game-speed` starts a directly launched mission at a chosen speed
  for scripted runs and benchmarks.

    The simulation was made safe for those speeds first. Each frame's fixed-step
    budget now scales with the speed — 8 steps at 1×, 32 at 4× — so 4× covers the
    same 133 ms real frame that 1× did rather than losing a quarter of its ticks
    on any frame slower than 62 ms. The stall guard that caps a long frame now
    clamps real seconds instead of speed-multiplied seconds, so it no longer
    fires four times as early at 4×. Ticks are still fixed at 1/60 s at every
    speed, so a battle plays out identically whether it was run at 0.5× or 4× and
    whatever frame rate delivered it; commands are still stamped and drained per
    tick, so their order does not change either. When a machine genuinely cannot
    keep up, the ticks that could not run are counted and logged rather than
    silently discarded. A speed stored in a save is checked against the offered
    set on load, so an out-of-range value cannot come back.

- **A guided tutorial.** The main menu gains a **Tutorial** entry beside
  Skirmish and Campaign that opens _Field Training_, a scripted first battle on
  the new Training Meadow map. Fifteen steps teach, in order, selection and
  movement with the order feedback, attacking a held Roman scouting party,
  felling timber, quarrying stone and mining iron and where the loads end up,
  raising a Home and what its cost and placement rules mean, recruiting and the
  population pool that limits it, assembling an army, breaking a Roman raid,
  Guard/Hold/Patrol, the commander's aura and rally, the camera, pause and game
  speed, the Objectives screen, and finally taking the enemy camp. Every step
  shows an on-screen objective with a completion state and progress, and when
  the thing the player is trying does not work the card says why (`Collect and
Build are only available to builders…`, `A Home costs 50 wood and 15 stone;
you have…`, `A red outline means the site is blocked…`). The card can be
  paused, hidden, skipped, replayed and ended at any time; the raid is held
  back until the army step is reached, so the tutorial runs at the player's
  pace, not the clock's. New tutorial missions declare `"tutorial": true`, and
  a map that exists only to carry one hides itself from the skirmish list with
  `"skirmish_hidden": true`.

- **A Field Manual.** The `?` button in the top bar (and a **Field Manual**
  entry in the main menu) opens a reference on the basics, economy, buildings,
  army, commander and controls, lists the current key bindings, and shows every
  tutorial step with its state. Opened from a battle it pauses the game and
  resumes it on close.

- **Every order now answers back.** Attack, move, guard, patrol, hold, stop,
  build, gather, deliver, repair and rally-point orders — whether issued through
  the command buttons or a right-click — go through one submission path that
  reports accepted or rejected with the target or destination and a
  player-readable reason. An accepted order drops a fading ring on the ground
  (or on the target, following it), plays its cue and shows a short label next
  to the cursor; a refused one plays the error cue, shows a dashed grey ring
  and says why (`No enemy under the cursor.`, `The selected units cannot
attack.`, `That target is already gone.`, …) instead of silently doing
  nothing. Build and gather placement rejections now use the same in-HUD
  notice rather than a modal error dialog.

- **Reading a fight is no longer guesswork.** Left-clicking an enemy unit or
  any building (yours or theirs) inspects it: the selection panel shows its
  name, health, activity and how many of your units are on it, and a bold ring
  marks it in the world. Every target your selection is attacking carries a
  pulsing lock ring with a sword glyph, enemies attacking your selected units
  wear a red chevron ring, and the command banner names the current target with
  its health bar. Hits now pop compact damage ticks in the RTS view (red for
  damage taken, gold for damage dealt, a skull on the killing blow), coalesced
  per target and capped so a large battle stays legible; hidden units never
  leak numbers. Arrows and bolts flying at your army glow warm red and leave a
  longer trail, your own volleys glow bright, and ordinary arrows now flash on
  a connected hit and puff dust on a miss. Lock-ring pulses, chevron motion,
  the miss puffs and the damage-tick rise all switch off under Reduced motion.
  A new arena scenario, `combat_feedback_capture`, exercises melee, arrows,
  misses, a wall under siege and a killing blow in one capture.

### Fixed

- **Edge scrolling could switch itself off for the rest of the session.** The
  flag that suspends it during a drag was set on right-press and cleared on
  right-release, so a drag that never got its release — a modal opening over it,
  the window losing focus, a lost grab — left it latched and edge scrolling dead
  until the next mission. The same went for a pan key released while alt-tabbed
  away. The flag is now derived from the live pan state instead of being
  assigned, a cancelled press clears the drag, and an inactive window suspends
  scrolling rather than letting the camera drift on in the background.

- **Edge scrolling did nothing after closing a panel.** The overlay's timer was
  started from the mouse-enter event, and an overlay that becomes visible under a
  cursor already at rest never gets one — so returning from the menu, settings or
  the objectives screen left edge scrolling dead until the pointer was moved out
  of the window and back in. The timer now follows the cursor position directly.

- **The edge band did not grow with the interface scale.** It stayed 12 logical
  px on every layout, so at 200% interface scale on a high-resolution panel it
  was a sliver next to every other target. It now scales with both the
  sensitivity setting and the interface scale, and never falls below 4 px.

- **World hover leaked through the HUD.** The check for "is the cursor over a HUD
  panel" read `topPanelHeight`/`bottomPanelHeight`, which do not exist — the
  properties are `top_panel_height` and `bottom_panel_height` — so it always
  answered no. Units highlighted and building and formation previews followed the
  cursor while it was over the command panel. Edge scrolling is deliberately left
  alone by this check so that every screen edge still scrolls, including the
  bottom edge behind the command panel.

- **Three QML singletons were not singletons.** `StyleGuide`, `EconomyGuide` and
  the new `CameraGuide` carry `pragma Singleton`, but the module's generated
  `qmldir` only marks a type as a singleton when the source file says so in
  CMake. Without it they registered as ordinary components and every
  `StyleGuide.x` reference silently evaluated to `undefined`.

- **The top bar's objective row had a broken width binding**, referring to an
  `objectiveIcon` that does not exist; the icon's id is `objectiveGlyph`.

- Right-click attacks gave no visible or audible feedback at all, and the
  left-click attack-mode path tried to re-pick its target at screen position
  (0, 0), so the attack arrow could land on an unrelated unit. Both paths now
  use the unit that was actually clicked.
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

- **The graphics presets mean what they say, and are applied once.** High is
  now the complete game — every shader feature, full creature detail
  everywhere, four 4096 cascades, the whole post chain, 4x MSAA — and the
  default for a fresh profile. Ultra keeps all of that and adds the expensive
  extras: contact-hardening (PCSS) shadows, grass blades that receive shadows
  and glow when back-lit, extra water and terrain detail octaves, 8x MSAA.
  Medium keeps shadows and post-processing but small (two 1024 cascades, no
  godrays), and Low is built to reach 30 fps on weak hardware: no cascades,
  no bloom, godrays, ambient occlusion or FXAA, a third of the grass, wear and
  micro-relief compiled out of the shaders, creature detail reduced past a few
  metres. Each preset is one immutable profile in a table; a change ticks a
  generation counter and the renderer applies the whole profile once at the
  start of the next frame (recompiling every shader program in place for the
  new tier), instead of every subsystem testing the quality level per frame.
  Shader detail is a compile-time tier (`SOI_QUALITY_TIER`), not a uniform
  tested per fragment. Creatures have exactly two rendered levels of detail,
  Full and Reduced, plus a cull distance; the old third "billboard" level was
  only ever a cull and is now named as one.
- **A fresh install no longer opens at full volume.** Every mixer channel used
  to start at full scale, and because master, category and cue gains multiply,
  the first mission was painfully loud on a new profile. A profile with no saved
  audio preference now starts at 60% master and 50% music, with ambience at 80%,
  and those values are written to the settings file on that first load. Anyone
  who has already moved a slider keeps exactly what they saved — including a
  deliberate full-scale setting — and the settings panel reads the effective
  values back into the sliders each time it opens, so what you see is what is
  playing. Settings remains one entry away on the main menu for anyone who wants
  to adjust it before starting.

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
- The "Iron and Ember" interface: activity markers, Roman numerals, and a
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
