# Frame pacing remaining work — issue #1398

Issue: https://github.com/djeada/Standard-of-Iron/issues/1398

## Status

The issue is not complete. Loading-time GPU preparation and benchmark diagnostics
have been improved, but presentation pacing has not passed qualification.
This file replaces the previous queue at the user's request.

Implemented in the current changes:

- Prepare terrain, shared geometry, roads, water, shorelines, bridges, projectile
  geometry and basic meshes before gameplay, while loading is active.
- Initialize fog and scatter buffer handles during loading. Retain hidden scatter
  buffers without submitting stale instances when visibility changes.
- Attribute presentation and effects mutex waits separately from render CPU work;
  report playable resource counters and optional simulation profiling.
- Detect competing games/builds throughout benchmark runs, retain fixture hashes,
  support copied custom missions/maps, and record graphics environment settings.

## Verification at handoff

The game and rendering tests built successfully. All 89 focused rendering tests
and 14 Python tests passed; Ruff, workflow YAML lint and `git diff --check` passed.
The final `scatter-prewarmed` native diagnostic recorded three playable resource
creations (two buffers and one VAO), down from four before the scatter fix.
It failed the gate and detected competing games/builds, including the concurrent
test build. Its timings are not qualification evidence. Three resource creations
and the known presentation waits remain unresolved.

## Instructions

Read `docs/FRAME_PACING.md` and applicable repository instructions first. Preserve
existing workspace changes. Use an isolated build directory and do not terminate
other sessions' processes. Complete tasks below in order, attach evidence, and
only mark a task done when its acceptance condition is met.

Do not relax budgets or skip battlefield presentation updates to make a report
pass. Debugger runs, profiled simulation runs and contended runs are diagnostics;
repeat without those conditions for performance acceptance. Process scanning is
an additional check, not proof that the whole machine is idle.

## Remaining tasks

### 1. Complete resource measurement and remove remaining first-use work

- [ ] Audit direct GL buffer creation/upload calls in `render/gl/backend` and
      instrument paths that bypass the wrapper counters. Include texture/shader paths
      where relevant. Distinguish storage allocation/orphaning from transferred bytes
      and avoid double counting. Document measurement boundaries, including Qt.
- [ ] Trace remaining playable buffer/VAO creation, including the first playable
      frame before benchmark observation. Prewarm the owning resources while loading;
      do not suppress counters or hide the first frame.
- [ ] Verify scatter reveal/hide/reveal and camera traversal across chunk boundaries
      with an actual GL context; prove hidden instances stay hidden and known resources
      do not allocate again. Measure retained memory and uploads.

Start in `render/gl/buffer.cpp`, `render/gl/mesh.cpp`,
`render/ground/scatter_renderer_state.h`, `render/ground/scatter_renderer_base.h`,
`app/core/game_engine.cpp` and `ui/gl_view.cpp`.

Evidence: before the scatter fix, `basic-resources-prewarmed` recorded four
playable creation operations (three buffers, one VAO). A debugger traced a later
buffer allocation to Cypress scatter visibility. The remaining mesh owner was
not identified. Existing counters do not cover every raw backend upload, so a
zero count alone cannot establish completion.

Done when: coverage is documented and representative repeated traversals have
zero prohibited post-playable asset work with complete relevant instrumentation.

### 2. Resolve presentation stalls and pacing failures

- [ ] Reproduce on an idle native display with a freshly built executable.
- [ ] Use wait attribution and optional `SOI_PROFILE_SIMULATION=1` diagnostics to
      identify long world-lock holders; separate scheduling delays from actual CPU
      work. Fix the demonstrated cause while preserving simulation/presentation
      correctness, then rerun without diagnostic profiling.
- [ ] Verify p95/p99, maximum intervals, hitch frequency and clusters across the
      full measurement window, including the first playable seconds.

Evidence: `terrain-prewarmed` captured a 71.19 ms interval containing 56.63 ms
of presentation lock wait and 8 ms of effects lock wait. That wait is attributed,
but not fixed. Later contended runs cannot establish improvement or regression.

Done when: repeated native runs satisfy the existing pacing and resource gates;
rendered battlefield state continues to advance correctly under simulation load.

### 3. Complete reproducible battle and UI coverage

- [ ] Add versioned mission/replay/action fixtures for formation movement, first
      contact, dense projectiles, destruction, weather, fog reveal, selection,
      production and build panels. Camera-only runs do not cover these interactions.
- [ ] Record observed events and reject fixtures that never exercise their required
      behavior. Keep fixture/map hashes and executable/settings metadata in artifacts.
- [ ] Compare simulation/replay outcomes under different rendering loads to verify
      that pacing changes preserve orders, combat, destruction and production.

Done when: every required behavior has a reproducible fixture and observable
coverage evidence, with relevant correctness checks passing.

### 4. Qualify presets and provision the performance lane

- [ ] Designate reference CPU/GPU tiers and resolution/display settings. The local
      RTX 5060 measurements at 1280 × 720 are investigations, not a reference matrix.
- [ ] Run all four presets, representative missions and action fixtures for at least
      three repeats of 60 seconds. Investigate failures without discarding bad runs.
- [ ] Validate the proposed per-preset CPU/GPU/upload/spike budgets against that
      matrix; document any justified changes with evidence.
- [ ] Provision the `soi-performance` self-hosted runner required by
      `.github/workflows/frame-pacing.yml`, execute the workflow and verify retained
      manifests, logs, reports and summaries. The workflow alone does not provision it.

Done when: the agreed hardware/scenario matrix passes, artifacts are reproducible,
and the dedicated lane demonstrably runs the same acceptance checks.

## Commands

Use the existing isolated build if configured; otherwise configure it using the
repository build instructions. Local diagnosis used RelWithDebInfo and
`SOI_KEEP_SYMBOLS=ON`. Run builds and native performance captures sequentially.

```sh
cmake --build build-frame-pacing --target standard_of_iron render_tests -j 4
build-frame-pacing/bin/render_tests --gtest_filter='FramePacingTest.*:FrameProfileTest.*:RiggedMeshCache.*:ProjectileRelationTest.*:TerrainSceneProxyTest.*:TerrainSceneProxyServiceTest.*:RoadNetworkGeometryTest.*:FogRenderer.*:ScatterRuntimeTest.*' --gtest_brief=1
python3 -m unittest discover -s tests/scripts
ruff check scripts/check-frame-pacing.py scripts/summarize-perf-suite.py tests/scripts
yamllint .github/workflows/frame-pacing.yml
git diff --check
python3 scripts/check-frame-pacing.py --binary build-frame-pacing/bin/standard_of_iron --mission second_punic_war/battle_of_ticino --preset ultra --seconds 60 --repeats 3 --camera-cycle --output artifacts/frame-pacing-1398/qualification-next
```

Use a fresh output directory each time. Omit `--mission` and `--preset` for the
runner's default campaign/preset matrix. Use `--mission-file PATH` or
`--replay PATH` for explicit fixtures. `--allow-contended` collects diagnostics but still
fails qualification when competitors are detected.

Local artifacts live under `artifacts/frame-pacing-1398/` and are ignored by git;
they may not exist in another checkout. Preserve or export needed evidence with
the final handoff. Do not infer that an absent artifact passed.
