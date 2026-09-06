# Standard of Iron — test suite

Around 2,700 GoogleTest cases across nine binaries, plus a QtQuickTest suite for
the QML design system. The split is not organisational tidiness: each binary
links a different slice of the project, so the boundary it sits behind is
enforced by the link step rather than by review.

## The binaries

| Binary                    | Links                                                               | Covers                                                                                                                  |
| ------------------------- | ------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------- |
| `simulation_tests`        | `engine_core`, `game_sim`, `soi_persistence`                        | The kernel: session, command pipeline, ECS, terrain and map loading, production, wildlife, and the architecture guards  |
| `combat_balance_tests`    | `+ soi_runtime`, `soi_persistence`                                  | Combat, formations, and the headless battles that drive the production system registry                                  |
| `ai_tests`                | `+ soi_ai`, `soi_runtime`                                           | The computer opponent, and the two scenarios that need one to be meaningful                                             |
| `campaign_tests`          | `+ soi_missions` (which brings `soi_campaign`)                      | Mission and campaign content, victory rules, wave archetypes, the map/mission catalogue                                 |
| `persistence_tests`       | `+ soi_persistence`, `soi_missions`                                 | The snapshot contract, the save format, the save database and mission progress                                          |
| `render_tests`            | `+ render_gl`                                                       | The renderer, the creature/equipment pipeline, the software rasteriser                                                  |
| `app_tests`               | `+ app_core`, `ui_shell`                                            | View models, controllers, the widget shell, and the gameplay services that need a camera or the application around them |
| `arena_tests`             | `+ render_gl`, `arena_scenario_harness`, `arena_panels`             | The arena scenarios, runner, frame continuity, promo capture schedule and panel population                              |
| `tools_tests`             | `engine_core`, `game_sim`, `map_editor_core`, `balance_sim_harness` | The map editor document model and the balance runner. Links no renderer at all                                          |
| `design_system_qml_tests` | `ui_shell`                                                          | The QML design system, driven by QtQuickTest. Skipped when Qt QuickTest is not installed                                |

Three rules keep the split meaningful:

- **A test binary links production targets.** It never re-lists a production
  `.cpp` in its own source list — a separately compiled copy can pass while the
  object the game ships is broken.
- **A test binary links the domains it uses and no more.** Reaching for
  `game_systems` because it is convenient puts the AI and the save database into
  a binary that was supposed to prove it needed neither. (`simulation_tests` and
  `combat_balance_tests` do link `soi_persistence`, for three "survives a save"
  assertions. It sits _above_ `game_sim`, and a kernel file reaching down into it
  fails `scripts/check-modules.py` — so the boundary is still enforced, just by
  the module check rather than by the link step.)
- **No test is quarantined.** Nothing is excluded from the default run, and no
  test source is listed behind an `if(EXISTS)`. A test that cannot build or
  cannot pass is fixed or deleted with an issue. The fast pull-request profile
  below skips some tests, but the default run is still everything, and
  `scripts/check-test-speed.py` fails if a skipped test's name has stopped
  matching anything.

All three are checked, not just asserted:
`tests/architecture/module_boundary_test.cpp` fails if a production `.cpp`
appears in this directory's source lists, and if `scripts/run-tests.sh` and
`soi_test_binaries` disagree about which suites exist.

`simulation_tests` is the one to watch. It links the kernel and nothing else —
no AI, no save stack, no runtime, no camera — so a gameplay file that starts
needing any of them breaks its link step.
`tests/architecture/layering_test.cpp` says the same thing about includes,
`module_boundary_test.cpp` covers the module map across the kernel targets, and
`documentation_accuracy_test.cpp` checks that the README and
`docs/ARCHITECTURE.md` still describe the code that exists.

## Running them

```bash
make test              # configure, build the test binaries, run all of them
make test-only         # run them without rebuilding
```

`make test` shells out to `scripts/run-tests.sh`, which the CI workflows also
call — so there is one answer to "what does running the tests mean" rather than
one per workflow.

Directly:

```bash
./build/bin/simulation_tests
./build/bin/render_tests --gtest_filter=CarthageArmorBoundsTest.*
bash scripts/run-tests.sh build --gtest_brief=1
```

Anything that opens a window needs `QT_QPA_PLATFORM=offscreen`;
`scripts/run-tests.sh` sets it. `TEST_ARGS` forwards GoogleTest flags through
the Makefile:

```bash
make test-only TEST_ARGS="--gtest_filter=SaveLoadServiceTest.*"
```

Two focused binaries exist alongside the nine: `horse_model_tests` and
`elephant_model_tests` build one contract file each, so they stay runnable when
an unrelated translation unit in the render suite has interface drift. Both of
those sources are also compiled into `render_tests`, which is where they run on
the default path — the focused binaries are a debugging convenience, not a
second verification route.

## Adding a test

1. Put the file under the directory matching what it exercises (`systems/`,
   `render/`, `map/`, `db/`, …).
2. Add it to the source list of the binary whose link surface it needs, in
   `tests/CMakeLists.txt`. If a kernel test needs the renderer in order to
   compile, that dependency is the finding — fix it rather than moving the test
   up a tier.
3. `make test`.

Fixtures should leave nothing behind. A `SessionContext` plus `ScopedSession`
gives a test its own world, terrain, ownership, economy and clock; anything
reached through a registry `instance()` resolves through that session, so tests
no longer leak into each other through globals. A signal connection made to a
process-wide singleton must be bound to a context object that dies with the
test, or it will outlive the stack it captured.

## Conventions

- `EXPECT_*` continues on failure, `ASSERT_*` stops. Use `ASSERT_*` when the
  rest of the test would crash or assert nothing.
- Name the case after the behaviour, not the method:
  `HeavyArmorHangsBelowTheLightOneButStaysOffTheGround`.
- Prefer relationships and frame-relative bounds over tuned constants, so a test
  keeps meaning when the thing it measures is re-authored.
- One behaviour per case; keep cases independent of ordering.

## Profiles

`scripts/run-tests.sh` runs the same nine binaries either way. `SOI_TEST_PROFILE`
picks how much of them:

|                                        | `full` (default) | `pr`                             |
| -------------------------------------- | ---------------- | -------------------------------- |
| binaries built and run                 | all nine         | all nine                         |
| tests                                  | every one        | minus `tests/extended_tests.txt` |
| verifier, QML suite, replay round trip | yes              | no                               |

The `pr` profile exists because a few dozen tests are acceptance checks against
content -- they simulate a battle tick by tick, or walk every shipped map and
asset on disk. They cost seconds each, which in a Debug build is minutes, and
they are answering questions about content that a pull request has usually not
touched. Before the split the pull-request lane spent over ninety minutes here
and was killed by its own timeout.

`tests/extended_tests.txt` names them, one GoogleTest filter pattern per line
with the reason. Add to it only for the reasons already listed there, and only
after measuring. `scripts/check-test-speed.py` fails a fast run in which any
test exceeded the per-test budget, and fails if a pattern in the manifest has
stopped matching a real test:

```bash
SOI_TEST_PROFILE=pr SOI_TEST_REPORT_DIR=artifacts/test-reports \
  bash scripts/run-tests.sh build --gtest_brief=1
python3 scripts/check-test-speed.py --build-dir build \
  --report-dir artifacts/test-reports
```

Fixtures leaving nothing behind is what makes the split safe. A test that only
passes because a neighbour ran first passes in the full profile and fails in the
fast one, which is how `FormationCombatGeometry` was found leaning on
`MovementMotorTest` to seat the nation registry and the navigation grid for it.

## CI

`.github/workflows/pr.yml` builds `soi_test_binaries` and runs
`scripts/run-tests.sh` with `SOI_TEST_PROFILE=pr`;
`.github/workflows/extended-validation.yml` and the weekly sanitizer and
coverage lanes run the full profile, and the three platform build workflows run
the same script after their build. A suite missing from the build directory is
an error there, not a skip.
