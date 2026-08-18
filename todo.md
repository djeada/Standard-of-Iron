# Performance work: what is left

Written 18 Aug 2026, after the optimisation pass that landed the render-snapshot,
combat, capture, terrain-prop, minimap, shadow-pass, instancing-ring and
terrain-octave changes. Numbers below are interleaved A/B against a clean build
of `4709c79b` on a quiet box, using render-thread CPU time
(`thread_cpu_ms_average`), not wall clock.

Full report: <https://claude.ai/code/artifact/493f0438-6c8c-4157-bc36-36dd616e6d03>

## Where the frame goes today

Zama, Ultra, 1600x900, 176 visible soldiers, RTX 5060:

| Cost              | Per frame | Notes                                         |
| ----------------- | --------- | --------------------------------------------- |
| GPU colour pass   | 4.5 ms    | terrain surface 2.1-2.65, rigged soldiers 1.4 |
| GPU shadow pass   | 2.1 ms    | 4 cascades at 4096 squared                    |
| GPU post          | 0.3 ms    |                                               |
| Render-thread CPU | 1.7 ms    | sim 0.5, draw 1.2                             |

The frame is GPU-bound at 1x game speed on this hardware. It is CPU-bound at 4x
game speed, and would be CPU-bound on a weaker CPU. Everything cheaper than the
two items below has already been done.

## 1. Bake the terrain's static per-pixel fields into a texture

The largest single cost anywhere, on **every** preset: 2.1-2.65 ms at Ultra on
Zama, 3.25 ms on Trebia, and 2.0 ms even at Low because no graphics preset
touches it. On an integrated GPU the same shader is 20-30 ms, which is the
difference between shipping to that hardware and not.

Two families of work in `terrain_chunk.frag` are pure functions of world
position and the static height map, so both can be evaluated once at map load
and sampled thereafter. Bake them into the same atlas, in the same pass.

**a. Low-frequency noise.** 27 `gradient_fbm` calls per pixel at five octaves
each, roughly 135 noise evaluations; about fifteen of the calls are functions of
`world_coord * macro_scale + offset` only.

**b. Height-map neighbourhood queries.** `terrain_sky_openness()` (line 218) is
6 directions x 3 rings = **18 dependent texture taps per pixel**, and
`compute_curvature()` (line 141) is 5 more. Both read `u_height_tex`, which
never changes during a match. These may be the better half of the win: 23
dependent taps thrash the texture cache in a way arithmetic does not.

- [ ] Tag every `gradient_fbm` / `gradient_noise` call as bakeable or per-pixel,
      by frequency. Keep grain, granular, speck, rock detail and cracks
      procedural: they need per-pixel resolution.
- [ ] Add a one-time offscreen bake at map load that renders the bakeable fields
      into a world-space texture atlas, four fields per RGBA texture, 2-4
      texels/m (Zama at 800x800 tiles gives 1600-3200 square per texture, so
      three or four textures).
- [ ] Bake sky openness and curvature as two further channels of the same atlas.
      They are cheaper to bake than the noise (the height map is already
      resident) and they invalidate on the same event: a new map.
- [ ] The bake must use the **same** GLSL functions as the runtime shader, so the
      values match; share them through `assets/shaders/include/`.
- [ ] Swap the tagged calls for texture samples in the fragment shader.
- [ ] Validate: `arena_app --batch --scenario roman_marching_camp` and a hillside
      plus a plain screenshot compared before/after. Expect a small diff from
      bilinear filtering; confirm no banding on slopes, no shimmer when the
      camera moves, and that ambient occlusion still reads in gullies and at
      cliff feet, which is what sky openness drives.
- [ ] Re-measure with `SOI_GPU_BREAKDOWN=1` (terrain is draw-command type 8).

Expected: terrain 2.5 ms to under 1 ms, roughly a 20% shorter GPU frame here and
much more on weak GPUs. Bounded, no gameplay risk.

### Not worth baking: fog

Checked and measured, so nobody repeats the investigation. On Trebia, a river
map with mist volumes live, every fog-related cost together is under 0.3 ms of a
~7 ms GPU frame, and that figure is the whole post pass including sky, ambient
occlusion, godrays, blur and FXAA. The fog batch itself (draw-command type 5) is
0.005 ms.

- Fog of war is **already** the baked solution: `visibility_mask.glsl` is a
  single `texture()` fetch into a mask the `VisibilityService` maintains. It also
  has to stay dynamic.
- Ground mist (`ground_mist()` in `post_composite.frag`) is two value-noise taps
  and both scroll with `u_time`, so a static world-space bake does not apply.
  Scrolling UVs through a baked field would work and would save approximately
  nothing: two evaluations, not 135.
- Distance fog is analytic, a range function with no noise at all.

## 2. Move the simulation off the render thread

89% of process cycles are on `QSGRenderThread` with 20 cores idle. Sim rate and
frame rate are the same number, so every hitch in a game system is a dropped
frame, and there is no way to run a fixed simulation step while drawing at
display rate. This is the only change that raises the ceiling rather than the
floor. Budget a week, not a day.

- [ ] The seam already exists: `World::publish_render_snapshot`. The render
      thread should consume the last published snapshot and never touch the live
      world.
- [ ] Stage one: audit and fix everything that reads the live world off the sim
      thread. Known callers are `GameEngine::update`'s frame orchestrator,
      `MinimapManager::update_units`, the HUD view models,
      `SelectedUnitsModel`, picking, and `ActivityViewModel`. Put the boundary in
      and prove it holds before moving any threading.
- [ ] Stage two: run `World::update` on its own thread at a fixed step.
- [ ] Watch out for replay determinism (`scripts/check-replay-determinism.sh`)
      and for the sim already being frame-rate dependent.
- [ ] Validate with `arena_app --batch --all` and the campaign missions, not just
      a benchmark: this can change gameplay timing.

## 3. Cheaper items, worth doing alongside

- [ ] **Per-cascade shadow resolution.** 4 x 4096 squared is 64 M texels cleared
      and drawn every frame (~2.1 ms). The two far cascades gain nothing from 4096. Separate textures at 4096/4096/2048/2048 should cut roughly 40% with
      no visible change. This is the Ultra preset's own choice, so it needs a
      sign-off before changing.
- [ ] **Combat spatial queries.** `is_valid_enemy_of_owner` is still ~2% and
      `rebuild_combat_query_context` rebuilds a hash map every tick. Caching
      per-pair hostility for the tick and reusing the grid should trim the sim
      another 10-15%.
- [ ] **Presets that scale the right axis.** High currently culls 79% of the
      visible army for about 9% of frame time. Now that Low and Medium have
      terrain octave counts, wire the other real costs to the presets too:
      cascade count and resolution. Let High keep the soldiers.

## 4. Fix the instruments

They cost more time than any single optimisation on this pass.

- [ ] `FrameProfile` covers about 12% of the real frame: its phases totalled
      1.3 ms of an 11 ms frame, and cull, submit and present read 0.00 ms
      throughout. Extend the scopes to cover the simulation, the render snapshot
      and the shadow pass.
- [ ] `profiling_hud` is registered as a QML context property in `main.cpp` and
      no QML file references it, so the overlay cannot be switched on in-game.
      Wire it to a QML overlay behind a key binding.
- [ ] The benchmark hardcodes `"graphics_preset": "ultra"` into its report
      regardless of the actual preset, and will happily report a one-frame run
      (`--benchmark-seconds 35` on Zama produced `frames: 1`). Make it refuse to
      report fewer than N frames and read the real preset.
- [ ] Setting `SOI_RUNTIME_BENCHMARK_SECONDS` also enables the continuity probe,
      which does a full-framebuffer `glReadPixels` plus a QImage allocation every
      frame on campaign missions. Separate the two switches.

## Measuring on this machine

The box is shared and the numbers move by more than most real changes do. The
same Zama scene measured anywhere from 2.2 to 60 ms of "CPU work" depending on
what else was running; at load ~40 both binaries drop to one frame every three
seconds because the compositor starves.

- Check `/proc/loadavg` first. Above ~5, do not measure.
- Compare against a clean build of the base commit in a worktree, interleaved,
  at least two runs each, and report the spread.
- Prefer `thread_cpu_ms_average` (render-thread CPU) and the GPU timestamp
  queries over wall clock. Both are in the benchmark JSON.
- `perf record --call-graph lbr -e cpu_core/cycles/P` shares are robust to
  outside load; wall-clock milliseconds are not. Release builds are linked with
  `-s`, so symbol attribution needs `build-perf` (`SOI_KEEP_SYMBOLS=ON`).

Useful switches, all env-gated and free when unset:

```bash
SOI_SWAP_INTERVAL=0    # uncapped frame rate, otherwise vsync hides everything
SOI_GPU_BREAKDOWN=1    # GPU ms per draw-command type, logged every 120 frames
SOI_GL_DEBUG=1         # KHR_debug logger (note: changes driver behaviour)
SOI_SHADOW_INSTANCING=0  # bisect the shadow-pass instancing path
```

## Scenarios not yet exercised

`docs/MASSED_BATTLE_PERFORMANCE.md` documents `massed_battle_250` through
`massed_battle_2000` and `seven_ai_scale`. This pass only measured the campaign
battles, a skirmish and three arena scenarios. The massed-battle set is where
the sim cost should dominate, and it is the right acceptance test for item 2.

```bash
build/bin/arena_app --batch --scenario massed_battle_1000 \
  --fps 240 --duration 1 --capture-interval 0 \
  --artifact-dir artifacts/todo-baseline
```
