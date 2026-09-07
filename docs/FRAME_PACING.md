# Battle frame pacing

The runtime benchmark writes a `frame_pacing` gate alongside the existing Ultra
CPU-headroom `budget`. `scripts/run-perf-suite.sh` collects repeated mission/replay
runs, hardware/build metadata and raw JSON; its summary now fails if any runtime
run fails pacing or lacks a pacing verdict. Never average away a failed repeat.
`scripts/check-frame-pacing.py` is the dedicated lane for all four presets,
independent of the older Ultra-only budget. Run on an idle GPU-equipped machine,
with a Release/RelWithDebInfo build, a fixed 60 Hz display, resolution and driver.

```sh
python3 scripts/check-frame-pacing.py --binary build/bin/standard_of_iron \
  --seconds 60 --repeats 3 --camera-cycle
```

Retain the entire `artifacts/frame-pacing/<run-id>/` directory, including the
manifest, logs, summary and individual reports. Each process has a watchdog;
missing/invalid reports, wrong presets, unmeasured asset barriers and failed
repeats fail the lane. The existing `run-perf-suite.sh` additionally records
simulation replays for repeat comparisons. Pass `--replay path/to/mission.soireplay`
to this runner to measure an identical recorded simulation across presets and
repeats. Replays are copied into the artifact directory and SHA-256 hashed in the
manifest; live campaign mode is useful for investigation but does not certify
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

## Reference qualification still required

Record exact CPU, GPU, RAM, OS, driver, resolution and refresh rate for low,
midrange and high-end reference machines before accepting a tier. No reference
hardware has been designated or certified by this change. Keep the same fixture,
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
