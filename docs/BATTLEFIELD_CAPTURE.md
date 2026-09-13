# Battlefield Capture Runner

`battlefield_capture` is a deterministic, render-free acceptance runner used to validate battlefield behavior during migration work. It advances the simulation on a fixed 30 Hz clock from an explicit seed and writes newline-delimited JSON (JSONL), making its output suitable for repeatable comparisons and automated analysis.

The default invocation runs the required 60-second soak test.

## Deterministic simulation

The runner advances gameplay independently of rendering. Given the same scenario, seed, and duration, it produces the same simulation timeline and replay digest.

This separation is important for acceptance captures: render output can be sampled at different frame rates without changing gameplay behavior. A 30 FPS capture and a 60 FPS capture both interpolate the same fixed-tick replay rather than rerunning the simulation at the render rate.

Capture manifests therefore record `render_fps=30,60` together with the replay digest shared by both outputs.

## Available scenarios

The runner currently provides these scenarios:

- `infantry_20v20`
- `archers_vs_infantry`
- `mixed_formation`
- `casualty_reflow`
- `cavalry_charge`
- `narrow_passage`
- `commander_in_line`

Use `--list` to inspect the scenarios available in the current build.

## Recorded data

Each simulation tick records the state needed to understand and compare battlefield behavior, including:

- unit transforms;
- group IDs and stable slot IDs;
- target and action IDs;
- action events;
- damage and death state;
- preferred and actual velocity; and
- animation state.

Overlay records add higher-level battlefield context such as group anchors, formation slots, paths, velocity vectors, combat fronts, engagement pairs, weapon range, and line of fire.

At the end of the run, the summary records update, event, and death counters; simulated and wall-clock duration; and the deterministic replay digest.

## Usage

After building the project, list scenarios or run a capture directly:

```sh
battlefield_capture --list
battlefield_capture --scenario infantry_20v20 --seed 418 --seconds 60 --output infantry.jsonl
```

## How the output is used

The JSONL format is deliberately stable. It can serve as the authoritative input for:

- deterministic replay comparison;
- metric extraction;
- visual overlay tooling;
- 30 FPS and 60 FPS render captures; and
- CI artifact retention.

Keeping simulation output independent from rendering gives battlefield acceptance tests a single source of truth while still allowing the same replay to be inspected visually at multiple presentation frame rates.
