# Performance instrumentation

The Ultra / Full-LOD performance plan gates on numbers, so the first thing that had to
exist was a set of counters that can attribute every hotspot to a named stage, plus a
benchmark that refuses to certify a run it could not actually measure.

This document describes what is instrumented, where the counters live, and how to get a
verdict out of a run. It is the reference for Phase 0 of that plan. The optimisation work
itself is described in [MASSED_BATTLE_PERFORMANCE.md](MASSED_BATTLE_PERFORMANCE.md) and
[PATHFINDING_ARCHITECTURE.md](PATHFINDING_ARCHITECTURE.md).

## Running the suite

```bash
scripts/run-perf-suite.sh                 # full suite, 5 interleaved repeats
scripts/run-perf-suite.sh --seconds 10 --repeats 1 --skip-arena --skip-perf
scripts/summarize-perf-suite.py artifacts/perf/<run-id>
```

The runner refuses to measure a machine that cannot produce a comparable number: no
`DISPLAY`, a load average above 4, another `standard_of_iron` or `arena_app` already
running, or a blanked screen each stop the run. `--allow-contended` records the numbers
anyway and marks them diagnostic. That check exists because a locked screen and a
competing GPU client have each silently invalidated a whole measurement session before.

Fixtures are visited round-robin (`A B C A B C ...`) rather than five times each in turn,
so a machine that warms or throttles during the suite biases every fixture equally rather
than only the ones measured late.

Each campaign mission is **recorded once and then replayed** for every measured repeat.
`--replay` drives the match from the recorded command stream and shuts out local input
and the computer opponent, so repeat 5 simulates the same battle as repeat 1 — and so
does a candidate build's repeat 1. Without that, an A/B comparison is comparing two
different matches. `--no-replay` plays each repeat live if you specifically want to see
run-to-run match variance instead. The manifest records which mode produced the numbers.

Camera motion is not part of the replay stream — it is presentation, not a command — so a
run still varies in where the camera sits. What the replay pins is the part that changes
what gets simulated and submitted: units, orders, combat, and the seeded weather.

`perf record` and `perf stat -d` run once per mission, separately from the timed runs and
after an 8-second delay (`PERF_DELAY`): attaching a profiler perturbs the very frame times
the gates read, and without the delay the samples are dominated by loading and warm-up
rather than steady-state play. `--skip-perf` turns that pass off.

Raw JSON lands in `artifacts/perf/<run-id>/` and is not committed. `verdict.json` and
`summary.txt` in that directory are the machine-readable and human-readable verdicts;
`manifest.json` records the commit, dirty-file count, CPU, GPU, driver and load average
the numbers depend on. `summarize-perf-suite.py` exits non-zero when any gate is red, so
CI can call it directly.

### The CPU-only gate

`scripts/check-sim-budgets.py` is the half of the plan that needs no GPU, so it can run on
an ordinary CI runner on every commit rather than only on the hardware runner:

```bash
scripts/check-sim-budgets.py --record          # accept today's numbers
scripts/check-sim-budgets.py --check-digest    # gate a change against them
```

It runs `sim_benchmark --json` and gates the simulation tick budget, the navigation tick
budget, and navigation **query amplification** -- position tests, standability tests and
cells scanned, expressed per unit per tick. The per-unit-per-tick form is deliberate:
Phase 2 is about navigation ceasing to scale with soldiers times ticks, and an absolute
cap would pass simply by shrinking the fixture. `--check-digest` additionally pins the
replay digest, so a change that alters simulation results cannot land quietly.

The baseline lives in `tests/perf/sim_budgets.json`; the default 25% tolerance absorbs
ordinary runner noise while still catching a step change.

Read `navigation_average_ms` in that baseline with care: it is ~3 ms at 1,000 units while
p95 is 0.069 ms, because the first tick builds the whole navigation grid and the mean is
one outlier wide. p50 and p95 are the steady-state numbers. The grid build is loading
work that happens to land inside tick one; it is not a per-tick cost.

The amplification numbers are what make this gate work on a shared runner. Measured on
31 Aug 2026 at `6a6fca63`, position tests per unit per tick were 2120.8 at 1,000 units,
2114.6 at 2,000 and 2112.7 at 5,000 -- flat to within 0.4% across a fivefold change in
army size. It is the number Phase 2 exists to collapse: navigation currently asks the
grid roughly two thousand questions per soldier per tick.

Why timings are advisory by default is also measured rather than assumed. Three
consecutive 1,000-unit runs on the same box, same binary, gave tick p95 of 138.0, 191.1
and 173.4 ms as background load came and went -- a 38% spread -- while `pos/unit/tick`
came back as 2120.8 in all three, and `cells/unit/tick` as 348.4 in all three. Gating on
the wall-clock numbers produced a false failure on the second run. Gating on the counters
did not, because they are a property of the algorithm rather than of the machine.

## Asset and GPU-resource counters

`render/profiling/asset_counters.h` holds one process-global set of relaxed atomic
counters. They are always on: every event they count (a bake, a GL object creation, a
shader compile) is rare enough that an atomic increment is free relative to the work it
is counting, and a counter that only exists in a special build is a counter nobody reads.

The counters cover runtime rigged and snapshot bakes, `RiggedMesh` constructions and the
bytes they copy, rigged/snapshot cache hits, misses, loads and evictions, skin atlas
builds and UBO upload bytes, forbidden-bake and missing-preloaded-asset reports, GL
buffer / VAO / texture / program creations, shader compiles, program links, and GL upload
bytes.

### The load barrier

A total is not interesting on its own; what the plan gates on is work that happens _after
a mission becomes playable_. `AssetCounters` therefore snapshots every total at a **load
barrier** and reports `since_barrier()` alongside `total()`.

The barrier is not a separate concept the caller has to remember to set. It is latched
by `Render::Creature::set_runtime_bake_forbidden()`: the first transition to forbidden
marks the barrier, and a transition back to permitted (a new mission load) clears it.
`RuntimeBakeAllowScope` deliberately bypasses that latch — an editor or portrait scene
that is allowed to bake must not be able to reset the measurement window underneath the
mission that is being measured.

`post_barrier_asset_work()` sums only the counters that must be zero in a playable frame:
bakes, mesh constructions, GL object creations, shader compiles and links, and guard
reports. Cache hits and upload bytes are reported but excluded, because a cache hit after
load is exactly what the architecture wants.

## Navigation counters

`game/core/nav_profile.h` is the navigation instrument. It lives in `engine_core` rather
than `game/systems` because `World::update` brackets each tick with it and `engine_core`
is the bottom layer: a counter that the tick loop cannot reach is a counter that cannot
be reported per tick.

It is off by default and enabled by the runtime benchmark, `arena --profile`, and
`sim_benchmark`. Counting is per tick: `NavTickScope` zeroes the tick accumulators on
entry and publishes them into lifetime totals and a 600-tick sample window on exit, so
both "what did this tick cost" and "what does a tick cost at p95" are answerable.

Counters are attributed by caller, not by function: position tests, segment tests,
standability tests and the cells they scan, nearest-standable searches and the cells they
scan, group routes, individual routes, route cache hits and misses, cells expanded, heap
operations, dirty cells rebuilt, grid rebuilds, queued and dropped path requests, and
lock wait microseconds. `NavScope` times an entry point and attributes both a call count
and its elapsed microseconds, so `navigation p95 per tick` is the sum of real navigation
work rather than a wall-clock tick span that includes everything else.

The cells-scanned counters exist because a call count alone hides amplification. A
thousand-unit `sim_benchmark` run reports roughly 1.9 million position tests and 1.2
million standability cells scanned _per tick_ — the same conclusion the plan reached from
`perf`, but attributable and reproducible without a profiler.

## Per-system percentiles

`Engine::Core::SystemProfiler` keeps a 600-sample window per system and reports p50, p95
and p99 next to the average and the peak, plus the same spread for the whole tick.
Averages and peaks alone hid the Alps tail: a system with a 0.2 ms average and a 9 ms p99
looks identical to one that is genuinely flat.

`Utils::Stats` in `utils/percentile.h` is the one percentile implementation. Before it
there were four, and they disagreed: two used `clamp(p * (n - 1))` and two used
`((n * p) + 99) / 100 - 1`, so the same samples produced different p95s depending on
which report you read.

`SampleWindow<N>::distribution()` describes the **retained window**: average, p50, p95,
p99 and maximum all range over the last N samples, so they are mutually consistent and a
recovered frame rate actually moves the average. `lifetime_average()` and
`lifetime_maximum()` are separate accessors for callers that want the whole run,
including samples the ring has evicted.

## Budget verdicts

`render/profiling/performance_report.h` turns a run into JSON. `PerformanceBudget`
carries the release gate from the plan (frame p50 12 ms, p95 16.67 ms, p99 20 ms, max
33.3 ms; simulation update 2 ms average and 4 ms p95; navigation 0.25 ms average and 1 ms
p95 per tick; render-thread submission 3 ms p95; GPU shadow plus colour 12 ms p95).
`PerformanceBudget::scale_gate(p95)` relaxes only the frame budgets for the massed-battle
fixtures and keeps the simulation and asset gates intact.

`budget_verdict_json` emits `{passed, frames, checks, failures}` where every check records
its measured value, its budget, and whether it passed. A run fails, deliberately, when:

- any budget is exceeded;
- GPU timing was unavailable, so no GPU claim can be made;
- the load barrier was never reached, so post-load asset work is unmeasured;
- any post-load asset work happened, with the counters named;
- the preset was not Ultra, or creature LOD was not forced to Full.

The last four are the reason the verdict is not just a comparison. A run that never
reached the barrier, or that quietly measured the Medium preset, would otherwise report a
green frame time and look like a pass.

Both report writers use it: `ui/gl_view.cpp` for `--benchmark-output`, and
`tools/arena/arena_scenario.cpp` for `report.json`. Arena reports also carry
`simulation_systems`, since the arena owns its `World` and can read the profiler directly.

## First measured baseline

Battle of Ticino, Ultra, 12 measured seconds, `RelWithDebInfo` at `6a6fca63`,
31 Aug 2026. The machine had other work on it, so the **timings below are diagnostic**;
the **counters are exact** regardless of load, which is the whole reason they exist.

| Metric             |   p50 |   p95 |   p99 |   max |
| ------------------ | ----: | ----: | ----: | ----: |
| render ms          |  5.78 |  8.89 | 13.08 | 15.26 |
| update ms          |  2.54 |  3.11 |  5.87 | 61.55 |
| GPU colour ms      |  3.94 |  4.87 |  5.65 |  6.13 |
| navigation ms/tick | 0.000 | 0.005 | 0.025 | 14.41 |

The verdict was **red**, on three counts:

- `update_average_ms` 2.76 against a 2.0 budget;
- `render_submit_p95_ms` 8.64 against a 3.0 budget;
- **869 asset operations after the load barrier.**

`post_load_asset_work_timing` answers the question the totals cannot: whether that work
is a startup tail the barrier was set too early for, or a genuine per-frame leak. It
reports the first and last measured frame in which the post-barrier total moved, and how
many frames moved it at all. A handful of frames clustered at the start is a prewarm
completeness problem; work spread across the run is a cache that never converges.

That last one is the finding the counters were built for. A single playable mission
performs, _after_ the barrier that is supposed to mean loading is done: 638 GL buffer
creations, 216 vertex-array creations, 4 texture creations, 4 shader compiles, 2 program
links, 1 rigged-mesh bake and 2 `RiggedMesh` constructions, plus 28.8 MB of GL uploads.
Phase 1.4 says none of that may happen in a playable frame, and until now nothing counted
it -- the existing guard only logged the two bake operations, which is why the GL object
creation, which is two orders of magnitude larger, went unnoticed.

Navigation is _not_ the problem in this mission: 0.005 ms p95 per tick against a 1.0 ms
budget. Crossing the Alps is where the plan's evidence puts the navigation cost, and it
has not been measured with these counters yet. The 14.41 ms navigation maximum is the
first tick's grid build, which happens before play and is expected.

`renderer_to_first_playable_frame_seconds` was 12.24 s, a large part of it audio
resampling and mastering during startup -- the work Phase 6 wants moved to
`tools/audio_master` output.

### The barrier itself is not reliably reached

Repeating that mission with the same binary and the same flags produced a second report
whose **totals were identical** -- 260 rigged-mesh bakes and 646 GL buffer creations in
both runs -- but in which `load_barrier_marked` was **false**. The barrier is latched by
`set_runtime_bake_forbidden(true)`, which the template prewarm only calls once it
finishes; when the run ends first, the latch never fires and nothing can be attributed to
"after loading" at all.

So the same mission attributes 869 operations to post-load work in one run and reports
the question unanswerable in the next. Neither run logs anything about it: prewarm
completion is silent, and `report_runtime_bake_violation` never fired in either, because
it only speaks when the flag it depends on is already set.

That is the case for Phase 1.4 as measured rather than argued. Until the barrier is a
load transaction that cannot complete with a required asset missing -- rather than the
tail of a best-effort background prewarm -- "no post-load bakes" is not a property any
run can establish. The verdict is built to refuse both halves of that: a run with
post-load work fails on the counters, and a run that never reached the barrier fails with
`the load barrier was never reached; post-load asset work is unmeasured`. What it will
not do is let either shape report green.

## Battle of Zama, ten minutes

600 measured seconds, Ultra, `RelWithDebInfo`, 31 Aug 2026. 69,191 frames over 48,830
simulation ticks on the campaign's largest map (800x800, four AI opponents, a
1,050-troop cap, two undead waves). The machine was busy, including with this session's
own builds, so **the timings are directional only**; the counters are exact.

### A navigation rebuild is a one-second freeze

The worst frame took **933.9 ms**, and `update_ms` peaked at **922.7 ms** in the same
frame -- so the stall is simulation, not rendering. The navigation counters name it:

    grid_rebuilds              5
    dirty_cells_rebuilt        2,560,343   = 4.0 x the 640,000-cell map

Four of the five rebuilds were whole-map rebuilds. On an 800x800 grid each one re-derives
terrain class, forest, resource, building, gate and clearance for every cell inside a
single tick, and the tick takes about nine tenths of a second. Phase 2.2 asks for exactly
this: rebuild dirty tiles plus their clearance halo, not the grid.

### The query amplification, at campaign scale

    position_tests             3,907,505,984   (80,023 per tick)
    cells_expanded             27,141,922
    standability_cells_scanned 5,656 per tick
    route_cache_hits / misses  9,146 / 3,922   (70% hit rate)

Nearly four billion grid questions in ten minutes. Navigation still fits its per-tick
budget comfortably (0.02 ms p95 against 1.0 ms) because each question is cheap -- but the
count is what Phase 2 exists to remove, and it is the reason a topology change costs a
second.

### The load barrier did not latch, again

`load_barrier_marked` was **false** for the third run in a row. A ten-minute mission rules
out "the run ended before prewarm finished". The consequence is that all of this is
unattributable to loading or to play:

    rigged_mesh_bakes          260
    rigged_meshes_constructed  270
    rigged_mesh_vertex_bytes   516,467,616
    shaders_compiled           240
    programs_linked            122
    gl_buffers_created         869

516 MB of mesh vertex data was assembled and 240 shaders compiled somewhere in this
process's life, and the guard that is supposed to say "not after loading" never armed. The
verdict fails the run for that reason rather than reporting a green post-load figure.

## Reading a benchmark report

`--benchmark-output` now emits a distribution rather than a scalar for each timing:
`render_ms`, `update_ms`, `thread_cpu_ms`, `wall_interval_ms`, `gpu_shadow_ms`,
`gpu_color_ms` and `gpu_wait_ms` each carry `average_ms`, `p50_ms`, `p95_ms`, `p99_ms`
and `max_ms`. `asset_counters`, `navigation` and `budget` are the Phase 0 additions.

The frame gates are evaluated against `render_ms` -- the render thread's CPU work per
frame -- not against `wall_interval_ms`. The game presents under vsync, so the wall
interval sits at 16.7 ms almost regardless of how much work a frame did (Ticino: wall p50
16.70 ms while render p50 was 5.78 ms). Gating on the presented interval would pass every
run that manages to hold 60 Hz and say nothing about headroom; `wall_interval_ms` is
still reported, because a run that _stops_ holding 60 Hz shows up there first.

`warmup_seconds` is reported separately from `measured_seconds` so a longer warm-up can
never be used to hide startup work inside the measured window, and
`renderer_to_first_playable_frame_seconds` records how long loading held the renderer
before the first measurable frame. It is measured from renderer construction, not
process start, so it excludes Qt and process startup.

`render_thread_allocations` reports whether allocation tracking was compiled in
(`-DSOI_PROFILE_ALLOCATIONS=ON`, which replaces global `operator new`/`delete`) and, when
it was, the render thread's allocation count and bytes since the first playable frame.
The default build reports `tracked: false` and zeroes rather than paying for the hook.
