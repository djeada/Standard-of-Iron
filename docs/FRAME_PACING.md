# Battle frame pacing

The runtime benchmark writes a `frame_pacing` gate alongside the existing Ultra
CPU-headroom `budget`. `scripts/run-perf-suite.sh` collects repeated mission/replay
runs, hardware/build metadata and raw JSON; its summary now fails if any runtime
run fails pacing or lacks a pacing verdict. Never average away a failed repeat.
`scripts/check-frame-pacing.py` is the dedicated lane for all four presets,
independent of the older Ultra-only budget. Run on an idle GPU-equipped machine,
with a Release/RelWithDebInfo build, a fixed 60 Hz display, resolution and driver.
The dedicated runner requests swap interval 1 regardless of saved preferences
and records graphics environment overrides in the manifest.

```sh
python3 scripts/check-frame-pacing.py --binary build/bin/standard_of_iron \
  --seconds 60 --repeats 3 --camera-cycle
```

Retain the entire `artifacts/frame-pacing/<run-id>/` directory, including the
manifest, logs, summary and individual reports. Each process has a watchdog;
the Linux lane samples `/proc` once per second for competing games and build
workers and rejects contaminated runs. It skips a run when a competitor is
already present; `--allow-contended` collects diagnostics but still fails the
gate. Competitor PIDs and executable names are retained per run.
Missing/invalid reports, wrong presets, unmeasured asset barriers and failed
repeats fail the lane. The existing `run-perf-suite.sh` additionally records
simulation replays for repeat comparisons. Pass `--replay path/to/mission.soireplay`
to this runner to measure an identical recorded simulation across presets and
repeats. Replays are copied into the artifact directory and SHA-256 hashed in the
manifest. `--mission-file path/to/mission.json` also accepts custom scenarios;
the runner retains the original mission, copies a local map relative to the
mission file, and records their hashes. Embedded `:/` maps remain tied to the
binary and its bundled assets. Live campaign mode is useful for investigation but does not certify
identical simulation inputs across runs. Software-rendered CI cannot certify GPU
budgets. Existing simulation and replay-determinism checks remain separate;
profiling does not alter simulation stepping or drop simulation work.

## Proposed 60 Hz budgets

These are engineering targets, **not measured reference-hardware results**.
All times are milliseconds; upload limits are bytes submitted per observed frame.

| Preset | CPU p95 | GPU p95 | Upload maximum | Interval p95 / p99 | Maximum spike |
| ------ | ------: | ------: | -------------: | -----------------: | ------------: |
| Low    |      10 |      10 |          2 MiB |            18 / 25 |            50 |
| Medium |      12 |      12 |          4 MiB |            18 / 25 |            50 |
| High   |      12 |      12 |          8 MiB |            18 / 25 |            50 |
| Ultra  |   16.67 |      12 |          8 MiB |            18 / 25 |            50 |

An interval over 33.34 ms is a hitch. Each run allows at most two hitch frames
per minute and at most one consecutive hitch frame. At least 120 frames and
30 seconds are required. More than 10% missing GPU samples, unknown presets and invalid samples
fail the gate. The older `budget` remains an Ultra/Full-LOD certification, so
non-Ultra reports should inspect `frame_pacing` independently of that verdict.

The pacing window starts at the first playable frame and includes the legacy
benchmark's warm-up. Loading itself is excluded. Frame intervals use `QQuickWindow::frameSwapped`, connected directly on the
scene-graph render thread. This measures queuing for presentation, not physical
display scanout ([Qt signal contract](https://doc.qt.io/qt-6/qquickwindow.html#frameSwapped)).
The clock is consumed for each rendered gameplay frame, retaining time across
skipped gameplay renders. Missing swaps fail the gate; render-start intervals
remain separately reported in `wall_interval_ms`. CPU and upload evidence belongs to the preceding frame.
Asset/upload deltas are baselined during loading, then checked from the first
playable frame. `post_playable_asset_work` must be zero. The legacy asset barrier
is armed earlier, before overlay prewarming, so its cumulative totals remain
reported as diagnostic evidence but do not replace this playable-window check. The final rendered frame
has no following interval and is not included.

CPU budgets use render-thread CPU time; `render_elapsed_ms` in hitch evidence
also includes driver waits. Simulation time and the preceding Present interval
are excluded from CPU phase attribution because they belong to other work.

Ten-second windows retain asset-work counts, hitch counts and worst intervals
to distinguish first-use work from recurring work.

Reports contain median, percentile and maximum intervals, normalized hitch
frequency, consecutive clusters and the worst frame's CPU phases/upload bytes in
each cluster. `largest_cpu_phase` identifies where the measured CPU work went;
it is evidence for investigation, not proof of causation. Scheduling, Qt UI work
outside the renderer, and vsync can contribute to the remaining interval. GPU
query results are delayed and explicitly labeled; they must not be interpreted
as exact attribution to that frame. GPU timing currently covers shadow and color
passes, not the entire Qt compositor. Upload deltas use process-wide counters.

## What the counters measure, and where they stop

Every direct GL resource call under `render/`, `ui/`, `scene/` and `app/` now
reports to `Render::Profiling::asset_counters()` through the small helpers in
`render/gl/gl_resource_tracking.h`. `GlCounterCoverage.EveryDirectGlResourceCallReportsToTheAssetCounters`
scans those trees and fails if a `glGenBuffers`, `glGenVertexArrays`,
`glGenTextures`, `glBufferData`, `glBufferSubData`, `glBufferStorage`,
`glMapBufferRange`, `glTexImage2D/3D`, `glTexSubImage2D` or `glTexStorage2D` call
appears without an adjacent note. Before this the counters covered only the
`Buffer`, `VertexArray`, `Texture` and `Shader` wrappers, so every backend
pipeline, the terrain submission path, the fog and visibility textures and the
campaign map view were invisible.

Allocation and transfer are now separate, because conflating them made
per-frame stream orphaning look like upload traffic:

| Counter                     | What it counts                                                                                   |
| --------------------------- | ------------------------------------------------------------------------------------------------ |
| `gl_buffer_storage_bytes`   | bytes of buffer storage allocated by `glBufferData`/`glBufferStorage`, including orphaning       |
| `gl_buffer_orphans`         | `glBufferData` calls with no data: storage reallocation, not a transfer                          |
| `gl_buffer_transfer_bytes`  | bytes actually copied into buffers, including writes into a mapped range                         |
| `gl_texture_storage_bytes`  | bytes of texture storage allocated                                                               |
| `gl_texture_transfer_bytes` | bytes actually copied into textures                                                              |
| `gl_mapped_buffer_bytes`    | bytes mapped for writing; a mapping is not itself a transfer                                     |
| `gl_upload_bytes`           | the aggregate of buffer and texture transfer bytes; what the pacing sample's `upload_bytes` uses |

A mapped range is counted once when it is mapped and once when it is written,
under two different counters, so nothing is double counted in `gl_upload_bytes`.

Measurement boundaries that remain, and must not be read as zero work:

- Qt's own scene graph allocates and uploads on the same context for the QML
  overlay, and Qt does not route through these helpers. Only the game's own
  resources are counted.
- A buffer that stays persistently mapped is counted when it is written through
  `PersistentRingBuffer`, but a driver that services that mapping lazily can
  move the real cost to a later frame.
- GPU-side costs of an allocation — driver-side residency, migration, eviction —
  are not visible to a client-side counter at all.
- Framebuffer objects (`glGenFramebuffers`, `glRenderbufferStorage`) and the
  shadow depth arrays' attachments are counted through their textures, not as
  framebuffers.
- `glGenerateMipmap` does GPU work that no byte counter attributes.

`SOI_TRACE_PLAYABLE_GL=1` records a backtrace for every buffer, vertex array and
texture created after the first playable frame and writes the aggregated sites
to `playable_gl_creation_sites` in the report. The addresses stay raw so tracing
costs nothing at capture time; resolve them with

```sh
python3 scripts/symbolize-gl-sites.py REPORT.json --binary build/bin/standard_of_iron
```

Keep `SOI_KEEP_SYMBOLS=ON` in the build being traced, and rerun without the
variable for acceptance measurements.

## Reproducible battle and UI coverage

A camera-only run does not exercise the game. On Battle of Ticino a 40-second
camera cycle observed one fog reveal and nothing else: no orders, no selection,
no contact, no projectiles, no production. `presentation_coverage` in the report
now counts what actually happened during the playable window, from the places
the events really occur — `WorldFeedbackStore::push` for damage and killing
blows, `VisibilityService` for fog reveals, `ArrowProjectile` for volleys,
`damage_application` for structure destruction, the orders and production view
models for player intent, and the rain manager for weather.

`--action-fixture PATH` drives a versioned, hashed action fixture on the GUI
thread while the benchmark runs. The format is documented by
`app/core/benchmark_action_fixture.cpp` and validated on load: version 1 only,
every action name known, every `required_coverage` name a real event, and every
action inside the loop. `assets/benchmarks/battle_coverage.action.json` drives
selection, attack-moves, guard and hold, the build cursor, the production panel,
recruitment and rally points on a 20-second loop.
`assets/benchmarks/first_contact.mission.json` is the mission it is authored
against, with its own `assets/benchmarks/first_contact.map.json`: two armies
twelve metres apart, builders and a barracks on each side, weather on a
thirty-second cycle so rain is active almost immediately, and two sacrificial
farms. A mission's relative `map_path` now resolves against the mission file
rather than the working directory, so a fixture is self-contained and the runner
copies and hashes its map alongside it.

The runner copies and SHA-256 hashes the fixture into the artifact directory,
records it in the manifest, sets `coverage.ui_actions`, and **fails the run when
a required behaviour was never observed**. `summary.json` also carries an
`outcome_comparison` block naming any behaviour that appeared on one preset and
not another.

Sixty-second runs of that pair observe every behaviour the fixture requires:
formation moves, selection changes, melee contact, projectile volleys, fog
reveals, the build cursor, production orders, killing blows and active weather.
The same fixture on Battle of Ticino observes no contact at all, because that
mission's first wave lands at 180 seconds — which is exactly the rejection the
gate is for.

`structure_destroyed` is deliberately **not** in that contract. It is observed
and reported, but no fixture yet drives it: see the note in `todo.md`.

## Reference qualification still required

`docs/PERFORMANCE_REFERENCE.md` holds the tier matrix, the qualification
commands and the runner provisioning runbook.
`scripts/check-perf-host.py` is the machine-checkable half: it rejects a host
with no graphical session, a software renderer, a non-60 Hz mode, a competing
game or build, or a load average above the threshold, and the workflow runs it
before it configures anything. Record exact CPU, GPU, RAM, OS, driver,
resolution and refresh rate for low, midrange and high-end reference machines
before accepting a tier. No reference hardware has been designated or certified
by this change. Keep the same fixture,
seed, replay and camera/UI actions across comparisons.

Qualify each preset with a 60-second-or-longer scenario containing:

- Repeated pan/zoom across the same loaded battlefield.
- Large formation movement, first contact and projectile volleys.
- First structure destruction and first weather activation.
- Fog-of-war reveals, selection changes, and first production/build panel opens.

Repeat the sequence to distinguish first-use work from recurring stalls. The
`--camera-cycle` option drives a repeating 20-second pan/zoom input path on the
GUI thread, starting after loading; its completed-cycle count is reported and
gated. Runs with no visible soldiers fail qualification; the report also records
the actual render-target dimensions. The camera target follows an absolute, closed world-space path around the
mission's starting camera target, so zoom-dependent pan scaling cannot make repeated traversals drift.
Zoom uses the normal camera controller. The path is a function of elapsed time. This does not
issue simulation commands. The campaign suite exercises real missions, but does
**not** yet automate all the UI/weather/destruction actions. A passing
campaign report alone does not certify this coverage. Save action timing and
coverage evidence with the raw reports until a presentation-action replay is
available. Investigate failing clusters together with `asset_counters`, allocation
tracking and subsystem profiles; remove or schedule the observed work, then rerun
the identical sequence. This instrumentation does not claim to fix unmeasured
shader, asset, upload or UI stalls.

## Dedicated workflow

`.github/workflows/frame-pacing.yml` runs on demand on a self-hosted Linux runner
labeled `soi-performance`. Provision Qt/build dependencies, Ninja, Python, a
hardware OpenGL driver and an active 60 Hz graphical session (`DISPLAY` and
`XAUTHORITY` as appropriate). Keep the host free of other GPU or compilation
workloads. The workflow serializes its own runs, builds before measuring, gates
camera completion as well as pacing and post-playable assets, and uploads artifacts
even on failure. A runner must be configured before this lane can execute; adding
the workflow does not designate or provision reference hardware.

Commander portrait prewarming now receives mission speakers while the loading
overlay is active. Previously synchronization explicitly deferred it until the
overlay closed, creating another renderer and compiling its shaders during
playable frames. The prewarm runs before the gameplay profiler opens (portrait
renderers share the global profiler); its elapsed cost is retained in the
`portrait_prewarm` phase if it ever occurs during gameplay.

The loading-overlay render pass also makes cached rigged meshes, attachments and
skin palettes GPU-resident. CPU-side template construction alone did not create
those buffers, which deferred their uploads until a camera first exposed them.
The pass retries on subsequent loading frames if a GL context is unavailable;
ordinary gameplay retains the existing lazy fallback and the pacing gate reports
any remaining asset work instead of hiding it.

## Local investigation: issue #1398

The 2026-09-07 `anchored-camera` run used the isolated RelWithDebInfo build,
Ultra, Battle of Ticino, a 60-second benchmark (62 seconds including playable
warm-up), and three camera cycles. It rendered at 1280 × 720 on an RTX 5060
with NVIDIA 590.48.01. This is an investigation, not reference qualification.
Raw reports, hardware/build manifest and logs are retained locally under
`artifacts/frame-pacing-1398/anchored-camera/`.

| Measurement                              |                   Result | Gate            |
| ---------------------------------------- | -----------------------: | --------------- |
| Average visible soldiers                 |                   144.96 | Nonempty battle |
| Presentation interval median / p95 / p99 | 16.68 / 27.41 / 34.91 ms | Fail            |
| Maximum presentation interval            |                272.39 ms | Fail            |
| Render-thread CPU / GPU p95              |          10.63 / 9.61 ms | Pass            |
| Maximum upload                           |            102,504 bytes | Pass            |
| Hitch frames                             |        50 (48.39/minute) | Fail            |
| Post-playable asset work                 |            97 operations | Fail            |
| Missing presentation samples             |                        0 | Pass            |

All asset work occurred in the first 20 seconds; all hitches occurred in the
first 30 seconds. Later windows had no hitches or asset work. The worst interval
contained 249.17 ms in presentation update but only 6.68 ms of render-thread CPU
time and no asset work. That is evidence of waiting or scheduling within
presentation update; it does not yet prove which lock or subsystem caused it.
The remaining work is to attribute and remove that wait and the first-use asset
operations, then rerun identical fixtures across presets and complete the
UI/weather/destruction coverage above. The gate remains failing.

Earlier `swap-timed` results had zero visible soldiers because the camera path
was centered on world origin. They cannot qualify battle performance. The
corrected path uses the mission's starting camera target, and both the report
and runner reject empty-battle measurements.

The follow-up `wait-attribution` run did not reproduce the earlier long hitches:
maximum 24.64 ms, p95 23.47 ms, p99 23.90 ms, and zero hitch frames. It still
failed p95 and the asset gate. New `playable_asset_counters` identified exactly
65 buffers and 32 vertex arrays, with no playable shader compilation, texture
creation, or mesh baking. Presentation and effects mutex waits now have separate
phases in cluster evidence; `frame_lock_stats` provides process-lifetime lock
statistics (including startup), distinct from the playable phase measurements.
Terrain chunk buffers are now prewarmed under the loading overlay because their
previous first-draw allocation allowed camera traversal to trigger uploads.

The `terrain-prewarmed` run reduced playable GPU creation from 97 to 52
operations (35 buffers, 17 vertex arrays). It still failed p95 at 23.66 ms and
recorded seven hitches. Its worst interval, 71.19 ms, contained 56.63 ms in
`presentation_lock_wait` and 8 ms in `effects_lock_wait`, with no asset work.
This directly attributes that particular hitch to lock waiting; the waiting
thread's total CPU time was 13.01 ms.

Set `SOI_PROFILE_SIMULATION=1` for a diagnostic run to include per-system
simulation times in `simulation_profile`. The report is read under the frame
lock after timing collection finishes. This opt-in profiler adds simulation
instrumentation overhead; rerun without it for acceptance measurements.

The `shared-prewarmed-profiled` diagnostic reduced playable asset operations
further to 40 (27 buffers and 13 vertex arrays). A separate game process was
active during this run, so its timing is **not** an acceptance result. This
observation prompted the runner's continuous competitor check. Resource
initialization and remaining scenario coverage can still be investigated under
contention, but timing qualification requires an idle machine.

The `features-prewarmed` diagnostic reduced playable resource creation from 40
to 10 operations by uploading authored road, water, shoreline, and bridge meshes
under the loading overlay. `projectiles-prewarmed` reduced this to seven.
Both runs detected competing compiler processes and cannot qualify timing.
The loading pass now also prepares projectile geometry. Fog buffers are retained
when no fog patches are visible, and their GL handles are explicitly initialized
under the overlay. The resource manager prepares its basic meshes during
initialization rather than waiting for their first draw.

The final `scatter-prewarmed` diagnostic recorded three playable creation
operations (two buffers and one vertex array), down from four in
`basic-resources-prewarmed`. Scatter handles are now prepared during loading
and retained through visibility changes. The hidden-chunk regression test
verifies that retention does not submit stale instances. The focused rendering
suite passed 89 tests and the Python runner suite passed 14 tests.

The final diagnostic detected competing games and build workers, including a
concurrent test build, and failed the gate. It cannot qualify presentation timing.
Remaining resource creation and presentation waits are unresolved. Direct GL
uploads in backend pipelines still need a coverage audit: wrapper counters alone
do not account for all GPU transfers or prove absence of runtime allocation.

### 2026-09-09: complete counter coverage and its consequences

Instrumenting the previously uncounted backend paths changed the picture the
earlier entries were built on. The `scatter-prewarmed` run's "three playable
creation operations" was an artifact of incomplete instrumentation. The same
mission and preset, measured with full coverage, reported **136** post-playable
creation operations: 129 buffers, 1 vertex array and 6 textures.

`SOI_TRACE_PLAYABLE_GL=1` attributed all 129 buffers to scatter chunk buffers
allocated on first reveal inside `sync_filtered_state`. `prewarm_gpu_resources`
had walked `spatial_chunks`, but that partition is built by the first submit, so
during loading it was usually empty and the prewarm allocated nothing; anything
it did allocate was then dropped when `refresh_runtime_world_props` reconfigured
the prop renderers on the first playable frame and `reset_instances()` cleared
the chunks.

Three changes followed. `prewarm_filtered_state` builds the spatial partition
itself rather than depending on a prior submit, and reserves each chunk's full
`count * sizeof(Instance)` storage. `sync_filtered_state` now reserves once and
updates in place with `Buffer::update_sub_data`, so a visibility change re-packs
without reallocating. `FilteredRendererState` keeps a retired-buffer pool, so a
rebuild or a `reset_instances()` hands its buffers to the new chunks instead of
deleting them.

Post-playable creation went 136 → 99 → **10** across those changes, measured on
the same mission and preset. The remaining ten are attributed, not guessed:

- 6 textures and 1 buffer: the commander portrait's own `PostProcessPipeline`
  rebuilding its targets from 64 × 64 to 232 × 280 when the HUD lays the panel
  out. The portrait prewarm runs under the loading overlay, but the QML item has
  no real size until gameplay, so the first real-size target is built inside the
  playable window.
- 2 buffers and 1 vertex array: a static mesh whose `Mesh::setup_buffers` runs
  on its first draw in the shadow pass, through `MeshInstancingPipeline::flush`.

`gl_buffer_orphans` also came into view: about 6.4 buffer orphans per frame,
roughly 2.8 GB/s of storage reallocation, almost all of it
`RiggedCullPipeline::begin_frame` re-orphaning its three stream buffers every
frame. That is the standard orphaning idiom rather than transfer traffic, which
is why storage and transfer are now counted separately: real transfer over the
same window is about 11 MB/s.

The presentation lock handoff was made fair: `update_presentation` now publishes
that it wants the frame lock when it defers, and the simulation thread yields to
that flag as well as to blocked waiters after it unlocks. Forced presentation
waits fell from 1–31 per run to 0–2. Deferred presentations did not change
measurably.

None of the timing in this entry is qualification evidence. Every run was made
while other sessions were compiling on the same machine; GPU p95 for the same
binary and mission varied between 19.6 ms and 32.9 ms across five runs, and
`scripts/check-perf-host.py` rejects this host outright. The resource counts are
reported instead because they are deterministic under contention.

### 2026-09-09: what the action fixture found

Running the first real scripted battle through the gate immediately exposed two
defects that a camera-only run cannot reach.

**Enemy troops were never prewarmed, so they did not render at all.**
`prewarm_unit_templates` restricted its troop catalogue to the nations it could
see at that instant, and it runs before the AI player is registered: the owner
registry held one owner, the nation registry held three nations, and there was
exactly one player-nation assignment. Every Carthaginian archetype — archer,
both spearman beard variants, swordsman and builder — therefore had no template.
`set_runtime_bake_forbidden(true)` is armed after the prewarm in every build, not
only in benchmarks, so `submit_rigged` refused those creatures and returned
without drawing; the units' selection rings, health plates and damage numbers
still drew, which is exactly how it looked in play. This was not specific to the
new fixture: `hold_the_sallow_ford` reproduces it as soon as its 75-second
Carthaginian wave lands, with 149,592 refused submissions in a 110-second run.

The prewarm now separates the nations it _observed_ from the nations it will
_prepare_: when it can see fewer than two nations the match roster is not yet
known, so it prepares every registered nation instead of guessing. Nations it did
not observe are still skipped when they declare no troops, which is what the
existing world-supplement regression test pins. Both missions now report zero
missing preloaded assets and zero forbidden bakes. The cost is 0.74 s of extra
loading on Battle of the Sallow Ford (10.20 s to 10.94 s to first playable
frame). The creature-miss log now names the archetype, so the next occurrence is
one line to diagnose.

**The bottom HUD's activity icon flickered during combat.** The selected-units
model is refreshed four times a second and each refresh re-derives the squad's
activity from scratch. Two things made that unstable. The dominant activity was
chosen with a strict `>` over a `std::map`, so an evenly split squad resolved to
whichever `(activity, state)` pair sorted first and flipped as counts wobbled;
it is now resolved by a fixed precedence that puts the fight first. More
importantly a unit's `AttackTargetComponent` comes and goes as it retargets, so
the sampled activity alternates between attack and move or guard. The model now
holds the displayed activity until the same new value has been sampled twice in
a row, so a one-sample blip cannot change the icon while a real change still
appears within half a second.

The battle fixture also reports far more first-use GPU work than a camera run:
942 post-playable operations against Ticino's 10, including 230 rigged meshes
constructed during combat. Those are poses and variants the prewarm's core clip
budget does not cover and only a real engagement reaches. They are recorded in
`todo.md` as remaining work.

### 2026-09-09: closing out the resource work

Two more first-use owners were removed and the last one identified.

`PostProcessPipeline::ensure_targets` released and re-created every render
target whenever the size or the pass set changed. The commander portrait's own
pipeline does that on the first playable frame, when the HUD lays its panel out
and the target goes from 64 × 64 to 232 × 280, so six textures and a buffer were
created inside the measured window; a main-window resize did the same. The
targets now keep their texture and framebuffer ids and re-specify storage
instead, and only targets whose pass has been switched off are released. Ticino's
post-playable operations fell from 10 to **4**.

The remaining four are three buffers and one vertex array: two buffers and the
vertex array are a single `Mesh::setup_buffers` reached from
`MeshInstancingPipeline::flush` in the shadow pass, and one buffer is on the
post-process path inside `Renderer::end_frame`. `SOI_TRACE_PLAYABLE_GL=1` now
also records a fingerprint (vertex count, index count, bounds radius) for every
mesh uploaded in the playable window, reported as `playable_mesh_uploads`, so
the next idle run can name that mesh without a debugger.

A replay of the action fixture was recorded and played back. It is **not** a
usable cross-preset comparison fixture, for two measured reasons: playback
reports `digest diverged at tick 30` on both a campaign replay and a mission-file
replay, so the simulation is not reproduced identically; and playback drives
recorded commands rather than the view models, so the selection, production and
build-panel coverage hooks never fire and a replay cannot satisfy the same
`required_coverage` contract. The runner's `outcome_comparison` over the live
fixture is the mechanism that does work. One playback segfaulted during
shutdown under heavy load and did not reproduce in three further runs; it is
recorded in `todo.md` rather than diagnosed here.

The ordered remaining tasks and acceptance commands are in `todo.md`.
