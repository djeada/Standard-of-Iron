# Battlefield Capture Runner

`battlefield_capture` is the deterministic, render-free acceptance runner for the
battlefield migration. It advances a fixed 30 Hz clock from an explicit seed and emits
newline-delimited JSON. The default invocation is the required 60-second soak.

Available scenes are `infantry_20v20`, `archers_vs_infantry`, `mixed_formation`,
`casualty_reflow`, `cavalry_charge`, `narrow_passage`, and `commander_in_line`.

Each tick records transforms, group and stable slot IDs, target and action IDs, action
events, damage/death state, preferred and actual velocity, and animation state. Overlay
records cover group anchors, slots, paths, velocity vectors, combat fronts, engagement
pairs, weapon range, and line of fire. The final summary contains update/event/death
counters, simulated and wall duration, and a deterministic replay digest.

Acceptance captures use one runner output as the authority. Render-frame sampling at
30 and 60 FPS interpolates that same fixed-tick replay; it never reruns gameplay at the
render rate. A capture manifest therefore names `render_fps=30,60` and records the
replay digest shared by both outputs.

Typical invocations after building:

```sh
battlefield_capture --list
battlefield_capture --scenario infantry_20v20 --seed 418 --seconds 60 --output infantry.jsonl
```

The JSONL output is deliberately stable and suitable for replay comparison, metric
extraction, visual overlay tooling, and CI artifact retention.
