# Standard of Iron — test suite

Around 2,700 GoogleTest cases across five binaries, plus a QtQuickTest suite for
the QML design system. The split is not organisational tidiness: each binary
links a different slice of the project, so the boundary it sits behind is
enforced by the link step rather than by review.

## The binaries

| Binary                    | Links                                                 | Covers                                                                                                                                                                           |
| ------------------------- | ----------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `simulation_tests`        | `engine_core`, `game_sim`                             | The simulation kernel: session, command pipeline, ECS, systems (combat, movement, AI, pathfinding, production, formations), map and mission loading, and the architecture guards |
| `persistence_tests`       | `engine_core`, `game_sim`                             | The snapshot contract, the save format, the save database and mission progress                                                                                                   |
| `render_tests`            | `+ render_gl`                                         | The renderer, the creature/equipment pipeline, the software rasteriser                                                                                                           |
| `app_tests`               | `+ app_core`, `ui_shell`                              | View models, controllers, the widget shell, and the gameplay services that need a camera or the application around them                                                          |
| `tools_tests`             | `+ map_editor_core`, `arena_*`, `balance_sim_harness` | The map editor document model, the arena harness and the balance runner                                                                                                          |
| `design_system_qml_tests` | `ui_shell`                                            | The QML design system, driven by QtQuickTest. Skipped when Qt QuickTest is not installed                                                                                         |

Two rules keep the split meaningful:

- **A test binary links production targets.** It never re-lists a production
  `.cpp` in its own source list — a separately compiled copy can pass while the
  object the game ships is broken.
- **No test is quarantined.** Nothing is excluded from the default run. A test
  that cannot build or cannot pass is fixed or deleted with an issue.

`simulation_tests` is the one to watch. It links the kernel and nothing else, so
a gameplay file that starts needing a camera, the renderer or `app/` breaks its
link step. `tests/architecture/layering_test.cpp` says the same thing about
includes, and `documentation_accuracy_test.cpp` checks that the README and
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

Two focused binaries exist alongside the five: `horse_model_tests` and
`elephant_model_tests` build one contract file each, so they stay runnable when
an unrelated translation unit in the render suite has interface drift.

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

## CI

`.github/workflows/pr.yml` builds `soi_test_binaries` and runs
`scripts/run-tests.sh`; the three platform build workflows run the same script
after their build. A suite missing from the build directory is an error there,
not a skip.
