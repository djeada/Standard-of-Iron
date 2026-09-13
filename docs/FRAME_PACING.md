# Battle Frame Pacing

Frame pacing in Standard of Iron is measured from real gameplay runs and reported as a `frame_pacing` verdict. The gate is designed to answer a broader question than “what was the average FPS?” It measures whether presented frames arrive consistently, whether CPU/GPU frame work fits the active graphics preset, whether uploads stay bounded, whether first-use asset work leaks into the playable window, and whether the measurement itself captured enough trustworthy evidence to qualify the run.

The current implementation is split across:

- `render/profiling/frame_pacing.h` — sample schema, preset budgets, statistics, verdict generation;
- `ui/gl_view.*` — gameplay-frame sampling and report integration;
- `scripts/check-frame-pacing.py` — repeat orchestration, fixture/replay handling, validation, artifact output;
- `.github/workflows/frame-pacing.yml` — dedicated qualified-hardware workflow; and
- `tests/render/profiling/frame_profile_test.cpp` — gate-logic tests.

The numbers in this article describe the current code-defined gate, not aspirational targets.

## What the gate is measuring

A frame-pacing pass combines several independent measurements:

1. **presentation intervals** — how much time passed between observed presented frames;
2. **render-thread CPU time** — how much CPU work the renderer/presentation path performed;
3. **GPU time** — delayed GPU timing samples where available;
4. **upload traffic** — game-owned GL transfer volume per frame;
5. **post-playable asset work** — resource construction after the measurement window is considered playable;
6. **hitch behavior** — rate and clustering of long presentation intervals; and
7. **measurement validity** — enough frames, enough time, correct preset, correct interval source, expected scene coverage.

A run can therefore fail even if one headline number looks good. For example, low CPU p95 does not compensate for a repeated presentation hitch or a stream of texture creation after the battle becomes playable.

## Current preset budgets

`render/profiling/frame_pacing.h` is the source of truth.

| Preset |  CPU p95 | GPU p95 | Max upload/frame | Interval p95 | Interval p99 | Max interval |
| ------ | -------: | ------: | ---------------: | -----------: | -----------: | -----------: |
| Low    |    10 ms |   10 ms |            2 MiB |        18 ms |        25 ms |        50 ms |
| Medium |    12 ms |   12 ms |            4 MiB |        18 ms |        25 ms |        50 ms |
| High   |    12 ms |   12 ms |            8 MiB |        18 ms |        25 ms |        50 ms |
| Ultra  | 16.67 ms |   12 ms |            8 MiB |        18 ms |        25 ms |        50 ms |

All presets also enforce:

- hitch threshold: `33.34 ms`;
- maximum hitch rate: `2` frames per minute;
- maximum consecutive hitch cluster: `1` frame;
- maximum missing GPU-sample fraction: `10%`;
- zero untimed presentation frames;
- zero post-playable asset work;
- at least `120` valid frames; and
- at least `30` seconds of measured presentation time.

Unknown graphics presets fail the verdict rather than silently inheriting a default budget.

## Why the gate uses percentiles

Frame time distributions are not well described by averages.

A renderer can average 8 ms while still producing a visible 70 ms stall every few seconds. The pacing gate therefore pays particular attention to:

- p95 CPU time;
- p95 GPU time;
- p95 and p99 presentation interval;
- absolute worst interval;
- hitch count/rate; and
- consecutive hitch clustering.

The goal is to catch both sustained overload and short recurring stalls.

## One frame sample

`Render::Profiling::PacingSample` records the evidence for one measured frame.

Current fields include:

- presentation interval (`interval_ms`);
- render-thread CPU time (`cpu_ms`);
- delayed GPU time (`gpu_ms`);
- game-owned upload bytes;
- per-phase CPU timings;
- post-playable asset-work delta;
- total render elapsed time; and
- whether a presentation timestamp was available.

The report then derives distributions and verdicts from the sequence of samples.

## Presentation interval source

The accepted interval source is:

```text
QQuickWindow.frameSwapped
```

This is a Qt presentation/queueing signal. It is not a claim to measure physical panel scanout or end-to-end input latency.

The distinction matters when interpreting the report: the gate measures application-level frame presentation continuity as observed through Qt, not display hardware telemetry.

`scripts/check-frame-pacing.py` rejects reports that use the wrong interval source.

## Untimed frames

A presentation frame without a valid presentation timestamp is not silently removed from the evidence.

The current gate allows zero untimed presentation frames. If timing disappears for part of the run, the measurement is considered invalid for qualification rather than producing optimistic percentiles from the subset that happened to be timestamped.

## CPU timing

`cpu_ms` measures render-thread CPU work for the frame.

Per-phase CPU timing is also retained so a failed frame can be decomposed. Depending on the report, the worst sample can show which rendering phase dominated instead of forcing diagnosis from one aggregate number.

The preset CPU p95 budget is intended to capture recurring CPU pressure, while the presentation-interval/hitch checks catch visible stalls that may come from CPU, GPU, synchronization, resource creation, or other causes.

## GPU timing

GPU timing is delayed because GPU queries do not necessarily resolve in the same frame that submitted the work.

The report associates available delayed GPU results with the pacing evidence while tracking the fraction of missing GPU samples.

A run fails if more than 10% of the expected GPU samples are missing. That prevents an apparently excellent GPU p95 from being accepted when most expensive frames simply lack GPU timing data.

## Upload traffic

The pacing gate records game-owned GL transfer traffic through the repository's GL resource tracking.

Tracked categories include:

- buffer storage allocation;
- buffer orphaning;
- bytes uploaded into buffers;
- texture storage allocation;
- bytes uploaded into textures;
- mapped-buffer bytes; and
- aggregate transfer bytes (`gl_upload_bytes`).

The per-frame upload budget varies by graphics preset because the expected content/detail envelope differs.

These counters describe traffic issued through the game's wrappers. They are not a complete measurement of driver residency, internal Qt Quick allocations, or every byte moved inside the graphics stack.

## Post-playable asset work

`post_playable_asset_work` is deliberately separate from byte uploads.

The gate expects zero asset/resource construction after the battle enters the playable measurement window.

This catches a common class of hitch source: the steady-state frame is cheap, but the first arrow, creature, effect, building, shader variant, or other asset triggers expensive creation after gameplay has already been revealed.

A resource creation can therefore fail the pacing gate even when its transfer size is small.

## Playable GL creation tracing

For diagnosis, enable:

```sh
SOI_TRACE_PLAYABLE_GL=1
```

This records call sites for buffer, vertex-array, and texture creation after the first playable frame.

The resulting report stores raw addresses under `playable_gl_creation_sites`.

Resolve them against a symbol-preserving build with:

```sh
python3 scripts/symbolize-gl-sites.py REPORT.json \
  --binary build/bin/standard_of_iron
```

This tracing mode is diagnostic. The acceptance verdict remains based on the normal pacing/report contract.

## Hitch definition

A presentation interval longer than `33.34 ms` is considered a hitch for the current gate.

The gate then evaluates both frequency and clustering.

### Hitch rate

At most two hitch frames per measured minute are accepted.

### Hitch cluster

At most one consecutive hitch is accepted.

A pair of back-to-back long frames can therefore fail even if the total count over a long run remains small. Consecutive stalls are visually more disruptive and can indicate a multi-frame resource or synchronization event.

## Worst-frame evidence

The report keeps evidence for the worst pacing sample, including information such as:

- frame interval;
- CPU phase timings;
- upload traffic;
- asset-work delta; and
- delayed GPU timing when available.

This is important because the gate is meant to be actionable. A red verdict without evidence would only say that the run was bad, not which subsystem dominated the bad frame.

## Ten-second windows

Reports also summarize ten-second windows.

Window-level evidence includes:

- asset-work count;
- hitch count; and
- worst interval.

This makes it easier to distinguish a single startup-adjacent event from a recurring pacing problem spread throughout the scenario.

## Minimum sample requirements

A report is not valid simply because the executable exited successfully.

The current gate requires:

- at least 120 valid frames; and
- at least 30 seconds of measured presentation time.

A too-short or mostly-unmeasured run is rejected instead of treated as a pass with insufficient evidence.

## Scene validity checks

`scripts/check-frame-pacing.py` also verifies that the run measured the intended gameplay state.

It rejects a report when, among other things:

- `frame_pacing` is missing;
- the report is invalid;
- the measured preset differs from the requested preset;
- the frame-pacing verdict failed;
- no visible soldiers were measured;
- the load barrier was not measured;
- the interval source is not `QQuickWindow.frameSwapped`;
- untimed presentation frames failed their check; or
- post-playable asset work failed its check.

This prevents a trivially empty scene from “qualifying” a renderer that was never asked to draw the representative battle.

## Action coverage

A performance run is more useful when it exercises known expensive gameplay actions instead of only orbiting an idle camera.

The repository includes:

- `assets/benchmarks/first_contact.mission.json`;
- `assets/benchmarks/first_contact.map.json`; and
- `assets/benchmarks/battle_coverage.action.json`.

The action fixture is versioned and declares `required_coverage`.

The runner validates that required behaviors were actually observed. Current coverage can include gameplay events from systems such as:

- selection and orders;
- combat feedback;
- projectiles;
- fog reveals;
- production actions; and
- weather state.

A fixture therefore proves more than “the process stayed alive for 60 seconds.”

## Camera-cycle coverage

The runner can drive repeated camera movement with `--camera-cycle`.

Camera motion exercises presentation paths that a static view may not stress in the same way, including visibility changes, terrain/scatter submission, scene transformations, and frame-to-frame QSG behavior.

Camera-cycle runs are still subject to the same sample and scene validity checks.

## Replay-driven coverage

The runner can use an explicit replay:

```sh
--replay path/to/mission.soireplay
```

The replay is copied into the artifact directory and hashed.

This is useful when comparing presets or code revisions because the command stream can remain identical across runs.

The pacing gate is still measuring presentation performance, but the gameplay sequence is controlled by the deterministic replay system.

## Local run

A representative camera-cycle run is:

```sh
python3 scripts/check-frame-pacing.py \
  --binary build/bin/standard_of_iron \
  --seconds 60 \
  --repeats 3 \
  --camera-cycle
```

A run using the authored mission/action coverage is:

```sh
python3 scripts/check-frame-pacing.py \
  --binary build/bin/standard_of_iron \
  --mission-file assets/benchmarks/first_contact.mission.json \
  --action-fixture assets/benchmarks/battle_coverage.action.json \
  --seconds 60 \
  --repeats 3
```

Repeated runs are useful because a single performance sample can be influenced by transient host conditions even when the application is unchanged.

## Artifact set

The output directory contains the evidence needed to reproduce and inspect the qualification run.

Depending on the invocation, artifacts include:

- a manifest;
- raw performance reports;
- stdout/stderr logs;
- summary/verdict output;
- copied action fixtures;
- copied replays;
- content hashes; and
- host qualification information.

Keeping the exact fixture/replay and hashes with the report makes performance results reviewable instead of relying on a command copied into a chat or issue description.

## Host qualification

The dedicated workflow uses:

```text
.github/workflows/frame-pacing.yml
```

It runs on a self-hosted Linux x64 machine carrying the `soi-performance` label.

Before the build/measurement, it records host information with:

```sh
python3 scripts/check-perf-host.py \
  --output artifacts/pacing-ci/reference-host.json
```

The qualification result is part of the artifact set.

The workflow is not intended to treat arbitrary shared hosted CI hardware as a stable performance reference.

## GitHub Actions workflow

The frame-pacing workflow is `workflow_dispatch` only.

Its current responsibilities include:

1. record/verify performance-host information;
2. build a Release configuration;
3. run frame-pacing and GL-counter tests;
4. run real campaign pacing measurements with camera movement;
5. run the scripted battle/action fixture; and
6. upload artifacts even when qualification fails.

It does not provision a performance machine itself. The correctly labeled self-hosted runner is part of the test environment.

## Contention detection

On Linux, `check-frame-pacing.py` looks for competing processes that can contaminate a run.

Examples include:

- another `standard_of_iron` process;
- `arena_app`;
- compiler workers;
- Ninja;
- Make; and
- linkers.

A contaminated run is not silently treated as a clean qualification result. Competing PID/executable information is retained for diagnosis.

Performance measurement is only meaningful if host contention is visible in the evidence.

## How to interpret a failure

The gate reports several dimensions because different failures imply different work.

### CPU p95 failure

Likely direction: recurring render-thread work is too expensive.

Inspect CPU phase timings and determine whether the pressure comes from scene walk, preparation, sorting/batching, UI/QSG interaction, or another measured phase.

### GPU p95 failure

Likely direction: the selected preset submits more GPU work than the current budget allows.

Inspect pass cost, draw/triangle load, shadow/post effects, overdraw, and shader/resource choices.

### Upload budget failure

Likely direction: too much buffer/texture data is being transferred per frame.

Inspect dynamic buffers, orphaning patterns, repeated texture updates, or resources that should have been prepared earlier.

### Post-playable asset-work failure

Likely direction: resource construction is occurring after the playable barrier.

Use `SOI_TRACE_PLAYABLE_GL=1` and symbolization to find the creation sites.

### Interval p95/p99 failure with acceptable CPU/GPU p95

Likely direction: intermittent stalls, synchronization, host contention, QSG/presentation delay, asset creation, or a smaller set of bad frames rather than sustained workload.

Inspect worst-frame and ten-second-window evidence.

### Hitch-cluster failure

Likely direction: a multi-frame event is blocking presentation, even if the total number of hitches is low.

### Missing GPU-sample failure

Likely direction: the GPU measurement itself is incomplete. Fix the instrumentation/availability problem before drawing conclusions about GPU headroom.

### Scene-validity failure

Likely direction: the benchmark did not measure the intended battle. Correct the launch/fixture/load path before treating the numbers as performance evidence.

## Graphics presets and pacing budgets

The pacing gate and graphics profile system describe different layers of the same performance contract.

`render/graphics_settings.h` defines what Low/Medium/High/Ultra render. `frame_pacing.h` defines the timing/upload limits those presets must meet during the measured workload.

A profile change that adds more expensive rendering work can therefore require either:

- an implementation optimization; or
- an intentional budget change backed by updated performance expectations.

The documentation should not update one table without checking the other source.

## Relationship to load/startup performance

Frame pacing begins after a playable measurement barrier.

Startup/map-load work has its own instrumentation and readiness contracts. Moving expensive work earlier can be a valid way to remove first-use hitches, but that work still needs to fit the startup/loading experience.

See [MISSION_STARTUP.md](MISSION_STARTUP.md) for startup readiness and [PERFORMANCE_INSTRUMENTATION.md](PERFORMANCE_INSTRUMENTATION.md) for broader profiling.

## Relationship to simulation performance

A `frame_pacing` pass does not replace simulation-budget or deterministic replay checks.

Presentation can be smooth while the simulation itself is too slow, and simulation can be fast while rendering hitches.

The repository therefore keeps separate measurements for:

- simulation/system work;
- frame presentation;
- replay determinism;
- asset/resource behavior; and
- startup/load behavior.

A performance change should be judged against the subsystem it actually affects.

## Measurement invariants

The current gate depends on these invariants:

- budgets come from `frame_pacing.h`;
- the requested and measured preset must agree;
- presentation intervals come from `QQuickWindow.frameSwapped`;
- missing timing is a measurement failure rather than silently discarded data;
- representative scene/action coverage must be observed;
- post-playable resource creation must be zero;
- contaminated host state is recorded; and
- reports retain enough evidence to diagnose failed frames.

## Source map

| Concern                | Source                                          |
| ---------------------- | ----------------------------------------------- |
| Budgets/sample/verdict | `render/profiling/frame_pacing.h`               |
| Runtime sampling       | `ui/gl_view.*`                                  |
| Runner/validation      | `scripts/check-frame-pacing.py`                 |
| GL-site symbolization  | `scripts/symbolize-gl-sites.py`                 |
| Host qualification     | `scripts/check-perf-host.py`                    |
| CI workflow            | `.github/workflows/frame-pacing.yml`            |
| Gate tests             | `tests/render/profiling/frame_profile_test.cpp` |
| Action fixture         | `assets/benchmarks/battle_coverage.action.json` |

The gate documented here is the current executable performance contract. Historical investigation notes and proposed targets are not substitutes for the values enforced by the code and workflow.
