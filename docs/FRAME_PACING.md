# Battle Frame Pacing

Frame pacing is measured by the runtime benchmark and reported as a `frame_pacing` verdict. The gate measures presentation intervals, render-thread CPU time, delayed GPU timing, upload traffic, post-playable asset work, hitch frequency/clusters, and whether presentation timing itself was captured correctly.

The implementation is split between:

- `render/profiling/frame_pacing.h` — sample format, budgets, verdict generation;
- `ui/gl_view.*` — gameplay-frame sampling and report integration;
- `scripts/check-frame-pacing.py` — repeated real-run orchestration and artifact validation;
- `.github/workflows/frame-pacing.yml` — dedicated hardware workflow; and
- `tests/render/profiling/frame_profile_test.cpp` — gate-logic tests.

## Current budgets

`render/profiling/frame_pacing.h` is the source of truth.

| Preset | CPU p95 | GPU p95 | Max upload/frame | Interval p95 | Interval p99 | Max interval |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Low | 10 ms | 10 ms | 2 MiB | 18 ms | 25 ms | 50 ms |
| Medium | 12 ms | 12 ms | 4 MiB | 18 ms | 25 ms | 50 ms |
| High | 12 ms | 12 ms | 8 MiB | 18 ms | 25 ms | 50 ms |
| Ultra | 16.67 ms | 12 ms | 8 MiB | 18 ms | 25 ms | 50 ms |

All presets also use:

- hitch threshold: `33.34 ms`;
- maximum hitch rate: `2` frames per minute;
- maximum consecutive hitch cluster: `1` frame;
- maximum missing GPU-sample fraction: `10%`;
- zero untimed presentation frames; and
- zero post-playable asset work.

A report also requires at least 120 valid frames and at least 30 seconds of measured presentation time.

Unknown graphics presets fail the verdict.

## What one frame sample contains

`Render::Profiling::PacingSample` records:

- presentation interval (`interval_ms`);
- render-thread CPU time (`cpu_ms`);
- delayed GPU time (`gpu_ms`);
- game-owned upload bytes;
- per-phase CPU timings;
- post-playable asset-work delta;
- total render elapsed time; and
- whether a presentation timestamp was available.

The frame-pacing report computes percentile distributions and keeps evidence for hitch clusters, including the worst sample's CPU phases, upload bytes, asset-work count, and delayed GPU timing.

Ten-second windows report asset work, hitch count, and worst interval so first-use spikes can be distinguished from recurring pacing problems.

## Presentation timing source

The gate identifies the interval source as:

```text
QQuickWindow.frameSwapped
```

This is a Qt presentation/queueing signal. It is not a measurement of physical display scanout.

`check-frame-pacing.py` rejects a report when:

- `frame_pacing` is missing;
- the report is invalid;
- the measured preset differs from the requested preset;
- the frame-pacing verdict failed;
- no visible soldiers were measured;
- the load barrier was not measured;
- the interval source is not `QQuickWindow.frameSwapped`;
- untimed presentation frames failed their check; or
- post-playable asset work failed its check.

Optional camera/action-fixture runs add their own coverage requirements.

## Upload and asset counters

The pacing gate's upload value is game-owned transfer traffic recorded by the GL resource-tracking helpers.

The counters distinguish:

- buffer storage allocation;
- buffer orphaning;
- bytes transferred into buffers;
- texture storage allocation;
- bytes transferred into textures;
- mapped-buffer bytes; and
- aggregate transfer bytes (`gl_upload_bytes`).

`post_playable_asset_work` is separate from byte transfer. It catches asset/resource construction after the game has entered the playable measurement window.

These counters do not claim to measure all driver or Qt scene-graph work. Qt's internal QSG allocations are outside the game's GL resource wrappers, and driver-side residency/migration costs are not equivalent to client-side byte counters.

## Playable GL creation tracing

Set:

```sh
SOI_TRACE_PLAYABLE_GL=1
```

to record call sites for buffer, vertex-array, and texture creation after the first playable frame.

The report stores raw addresses in `playable_gl_creation_sites`. Resolve them against a symbol-preserving build with:

```sh
python3 scripts/symbolize-gl-sites.py REPORT.json \
  --binary build/bin/standard_of_iron
```

This tracing mode is diagnostic and is not the acceptance measurement itself.

## Reproducible action coverage

`scripts/check-frame-pacing.py` can drive both camera motion and an authored action fixture.

The repository provides:

- `assets/benchmarks/first_contact.mission.json`;
- `assets/benchmarks/first_contact.map.json`; and
- `assets/benchmarks/battle_coverage.action.json`.

The action fixture is versioned and declares `required_coverage`. The runner copies and hashes the fixture into the artifact directory and rejects a run when a required behavior was never observed.

The current coverage path records gameplay events from the systems where they occur, including selection/orders, combat feedback, projectiles, fog reveals, production actions, and weather state.

The runner can also use an explicit replay with:

```sh
--replay path/to/mission.soireplay
```

Replays are copied into the artifact directory and hashed so repeated/preset comparisons can use the same command stream.

## Running the gate locally

A representative run is:

```sh
python3 scripts/check-frame-pacing.py \
  --binary build/bin/standard_of_iron \
  --seconds 60 \
  --repeats 3 \
  --camera-cycle
```

Custom mission/action coverage can be measured with:

```sh
python3 scripts/check-frame-pacing.py \
  --binary build/bin/standard_of_iron \
  --mission-file assets/benchmarks/first_contact.mission.json \
  --action-fixture assets/benchmarks/battle_coverage.action.json \
  --seconds 60 \
  --repeats 3
```

The artifact directory contains the manifest, raw reports, logs, copied fixtures/replays when used, hashes, and summary output.

## Host qualification

The dedicated GitHub Actions workflow is `.github/workflows/frame-pacing.yml`.

It runs only on a self-hosted Linux x64 runner carrying the `soi-performance` label. Before building, it calls:

```sh
python3 scripts/check-perf-host.py \
  --output artifacts/pacing-ci/reference-host.json
```

The workflow then builds a Release configuration, runs the frame-pacing/GL-counter tests, measures real campaign missions with repeated camera movement, measures the scripted battle/action fixture, and uploads all artifacts even when the gate fails.

The workflow is `workflow_dispatch` only. It does not run on generic hosted CI and does not provision performance hardware by itself.

## Contention checks

On Linux, `check-frame-pacing.py` looks for competing game/build processes such as `standard_of_iron`, `arena_app`, compiler workers, Ninja, Make, and linkers while a measurement process is running.

A contaminated run is not silently treated as a clean qualification run. The competing PID/executable information is retained for diagnosis.

## Relationship to other performance gates

`frame_pacing` is the presentation-continuity gate. The repository also has separate simulation/replay/performance budgets and instrumentation. A frame-pacing pass does not replace deterministic replay checks or simulation-budget checks.

See [PERFORMANCE_INSTRUMENTATION.md](PERFORMANCE_INSTRUMENTATION.md) for system/asset/navigation counters and the broader performance-report format.

## Source of truth

Frame-pacing capabilities and budgets are defined by `render/profiling/frame_pacing.h`, `scripts/check-frame-pacing.py`, and `.github/workflows/frame-pacing.yml`. Historical investigation logs and proposed follow-up work are not part of the current gate contract.
