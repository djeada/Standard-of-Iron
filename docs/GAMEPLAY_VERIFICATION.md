# Automated Gameplay Verification

`battlefield_gameplay_verifier` is the non-interactive acceptance gate for battlefield
behavior. Unlike the old capture loop, it creates real units, submits normal commands,
and advances `Engine::Core::World` using the same ordered system registry as the game.
Movement, combat, authored actions, projectiles, cleanup, and `AISystem` are therefore
the production implementations.

## Run the gate

```sh
cmake --build build --target battlefield_gameplay_verifier -j2
build/bin/battlefield_gameplay_verifier --all --seconds 60
```

The command returns nonzero when any scenario fails - but only with `--all` or
`--verify`. A single scenario without either dumps telemetry and returns 0
whatever it found; it says so on stderr.

It is also registered with CTest, and the registered run is the sixty second
one. It used to be fifteen, which is shorter than the `invisible_damage` defect
took to appear - a gate shorter than the bug it is meant to catch:

```sh
ctest --test-dir build -L gameplay --output-on-failure
```

Use a 60-second soak before release candidates:

```sh
build/bin/battlefield_gameplay_verifier --all --seconds 60
```

To inspect one scenario as JSON Lines:

```sh
build/bin/battlefield_gameplay_verifier \
  --scenario mixed_formation --seconds 15 --output mixed.jsonl
```

## What fails automatically

The gate currently checks:

- player command response within 200 ms of simulation time;
- finite transforms and velocities;
- displacement inside each unit's speed envelope (teleport/glitch detection);
- facing changes no greater than 50 degrees per simulation tick;
- excessive idle/walk state flicker;
- damage transactions without an authored visible combat action;
- reserve ranks crossing an occupied friendly front;
- rank/file order inversion and friendly crossing;
- simulation ticks taking longer than a 30 FPS frame budget;
- opposed scenarios failing to reach combat; and
- the production AI completing decisions and applying gameplay commands.

Scenarios cover infantry lines, archers, mixed formations, casualty reflow, cavalry,
a constrained-width approach, commander-adjacent combat, and an autonomous bot
skirmish. Telemetry includes transforms, actual and requested velocity, targets,
authored action/event identifiers, animation state, health, formation slots, timing,
and AI decision/command counts.

## The movement gate

`movement_trace_report` could always read a trace and name what was wrong with
the walking, and nothing ever ran it: one watched skirmish produced 91,777
findings while every suite stayed green. `tests/headless/movement_quality_gate_test.cpp`
now runs the same scenarios with the trace on and holds the findings to a
budget - total findings per scenario, the worst treadmill, abandoned orders,
and bodies left pressing into geometry.

Diagnosing a finding by hand still goes through the tool:

```sh
SOI_MOVEMENT_TRACE_DIR=/tmp/trace SOI_MOVEMENT_TRACE_STRIDE=6 \
  build/bin/battlefield_gameplay_verifier --all --seconds 60
build/bin/movement_trace_report /tmp/trace --quiet
```

`SOI_MOVEMENT_TRACE_STRIDE` keeps one sample every N ticks and
`SOI_MOVEMENT_TRACE_MAX_MB` caps the file. Both matter: an unbounded trace of a
real match reached 12 GB, which the report tool could not read back inside two
minutes.

## What remains visual

This gate proves simulation behavior, responsiveness, and presentation state—not final
pixels, audio, GPU pacing, or whether an animation is aesthetically convincing. Those
need an additional rendered-frame gate that launches the arena offscreen, captures
fixed camera paths at 30 and 60 FPS, and compares perceptual images plus frame-time
traces. Until that layer exists, a short visual release-candidate review is still
required, but routine movement/combat/AI regressions no longer depend on manual arena
inspection.

Thresholds are intentionally framed like a responsive early-2000s RTS: immediate
command acknowledgement, stable readable formations, bounded turns, no invisible
damage, and no single simulation update consuming an entire 30 FPS frame. Tighten a
threshold only with captured evidence from representative gameplay; do not weaken one
merely to make a failing scenario green.
