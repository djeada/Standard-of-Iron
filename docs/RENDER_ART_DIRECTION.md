# Render art direction

The target look is the clean, cozy stylisation of _Röki_ and _The Last Campfire_:
large flat colour fields, readable silhouettes, soft contact grounding, no crushed
blacks and no clipped whites. This document records how the renderer gets there and
which knob to turn when a scene reads wrong.

## Frame structure

The world no longer renders straight into the host framebuffer. `Backend::begin_frame`
asks `PostProcessPipeline::begin_scene` to bind an offscreen **RGBA16F** scene target
(falling back to RGBA8 if float attachments are unavailable), and `Backend::execute`
resolves it back to whatever framebuffer the host had bound.

```
begin_frame            bind HDR scene target, clear
execute_scene
  ├─ shadow cascades   (own FBO, restores the scene target)
  ├─ sky               fullscreen pass, depth write off, before all geometry
  └─ world geometry    writes linear radiance, no clamp, no tone curve
resolve_scene
  ├─ bright pass       threshold + knee, third resolution (k_bloom_divisor)
  ├─ blur              separable gaussian, two ping-pong iterations
  ├─ composite         grounding AO → + bloom → tonemap → grade → vignette → RGBA8
  └─ FXAA              to the host framebuffer
```

`Backend::execute` is a thin wrapper around `execute_scene` precisely so that the
resolve runs on every early-return path inside the world render.

## Where colour is decided

**Material shaders write linear radiance.** They must not clamp, saturate or grade
their output — the scene target is HDR and the tone curve belongs to one place.
`ShaderSource.WorldShadersLeaveGradingToThePostProcessPass` pins this.

**`assets/shaders/include/tonemap.glsl`** owns the look. It has no UBO dependency so
the post pass and any future tool can include it:

| Constant                                     | Effect                                                    |
| -------------------------------------------- | --------------------------------------------------------- |
| `k_soi_grade_exposure`                       | linear gain before the curve; raise to open the midtones  |
| `k_soi_grade_white_point`                    | where the extended-Reinhard shoulder reaches 1.0          |
| `k_soi_grade_contrast` / `k_soi_grade_pivot` | S-curve strength and its anchor                           |
| `k_soi_grade_saturation`                     | global chroma                                             |
| `k_soi_grade_shadow_lift`                    | cool floor under the blacks — this is what stops crushing |
| `k_soi_grade_highlight_tint`                 | warm/cool split against the shadow lift                   |

**`assets/shaders/include/environment_lighting.glsl`** owns the _shading_ model on top
of the existing `EnvironmentLighting` UBO. `soi_wrapped_diffuse` wraps the terminator
and softly quantises it into plates; `soi_surface_lighting` is the shared
ambient + key term; `soi_rim_light` is the sky-coloured silhouette separator. Banding
is deliberately a _mix_ (`k_soi_shade_band_mix`), not a hard cel step — the reference
games are soft-shaded, not cel-shaded.

Props and foliage used to hand-roll `environment_ambient_light(N) + sun * ndotl * K`
with a different `K` per material, so nothing responded to light quite the same way.
They now call `soi_surface_lighting_scaled(N, K)`, which keeps each material's key
attenuation but routes every one of them through the same wrapped-and-banded curve.
Note that the scaled helper already applies `environment_exposure()` — a shader that
calls it must not multiply by exposure again.

`environment_lighting(normal, wrap)` itself now returns `soi_surface_lighting(normal)`,
so everything that already used the shared entry point — `basic`, `basic_instanced`
(all buildings), `catapult`, `catapult_instanced`, `primitive_instanced`,
`cylinder_instanced` — picks up the wrapped-and-banded key for free. That is what gives
adjacent building planes their tonal separation; before it, two walls meeting at a
corner were nearly the same value. The `wrap` argument is now ignored: the shared
model owns the terminator via `k_soi_shade_wrap`.

Because GLSL has no forward declarations, the `soi_` shading helpers must be defined
_above_ `environment_direct_light` in the include. Do not move them back below it.

Directly routed hand-rollers: `stone_instanced`, `tent_instanced`, `ruins_instanced`,
`weapon_rack_instanced`, `supply_cart_instanced`, `statue_instanced`,
`pine_instanced`, `olive_instanced`, `magic_shrine_instanced`, `iron_ore_instanced`.
Still on their own lighting: `dead_tree_instanced` and `bridge`, whose blocks are
shaped differently enough to need individual attention.

Foliage note: pine and olive canopies used to scale ambient by 1.30, which washed the
form out of them. At 1.06 the key light carries the shape instead.

## Palette

Cohesion comes from split-toning in one place rather than from editing colours at
fifty call sites: `k_soi_grade_shadow_lift` pushes the shadow floor cool-teal while
`k_soi_grade_highlight_tint` pushes highlights warm. Widening that split is the first
knob to reach for when the frame reads hue-flat. `scene/environment_lighting.h` carries
the matching defaults — `shadow_tint` and `shadow_strength` were lifted from a near-black
`{0.16, 0.20, 0.27}` at 0.72 to `{0.30, 0.34, 0.44}` at 0.60 so cast shadows read as
coloured shade rather than as grey slabs.

## Anti-aliasing

Two independent mechanisms, because they cover different hosts:

- **MSAA** on the scene FBO, driven by `GraphicsSettings::presentation().msaa_samples`
  (Low 0, Medium 2, High/Ultra 4). Both `ui/gl_view.cpp` and `ui/campaign_map_view.cpp`
  fall back to 0 samples if the multisample FBO fails to validate.
- **FXAA** as the final post pass, which is what actually cleans up the arena captures.

Before this work `ui/gl_view.cpp` hard-coded `setSamples(0)` and `campaign_map_view.cpp`
set 4 and then immediately overwrote it with 0, so nothing anywhere was anti-aliased.

## Grounding

`post_composite.frag` derives a cheap depth-delta occlusion from the scene depth
texture (`grounding_occlusion`). It darkens and cools where geometry meets the ground,
which is what stops buildings reading as pasted-on decals. `u_ground_ao_radius` is in
world units; `u_ground_ao_strength` scales the tint.

This needs the scene depth as a _texture_, not a renderbuffer — that is why
`ensure_targets` attaches `GL_DEPTH_COMPONENT24` via `glFramebufferTexture2D`.

## Sky

`sky.frag` reconstructs a world-space view ray from the inverse view-projection and
shades a horizon→zenith gradient with soft cloud bands and a sun disc plus halo.
Colours come from the environment UBO (`fog_color` at the horizon, `sky_color` at the
zenith, `cloud_cover` for band density), so the sky follows the same time-of-day
keyframes as everything else in `game/map/environment_lighting.cpp`.

Before this the sky was a flat `glClearColor`, which is what produced the hard horizon
line where the terrain simply stopped.

## Terrain

`terrain_chunk.frag` is a very detailed procedural shader, and that detail actively
fights the target look — clean stylisation puts detail in silhouettes, not surfaces.
`k_soi_terrain_detail_damping` and `k_soi_terrain_relief_damping` scale the
high-frequency grain/granular/speckle terms and the relief normal amplitude. Raise them
towards 1.0 to get the old naturalistic surface back.

`k_soi_terrain_hue_scale` / `k_soi_terrain_hue_amount` add a very low-frequency warm↔cool
break across the ground so a map does not read as one flat green. Keep the amount small
(under ~0.12); past that it stops looking like light and starts looking like paint.

## Known geometry fix

The stone mesh in `render/gl/backend/vegetation_pipeline_natural.cpp` computed
`cross(b - a, d - a)` for its ring quads, which is **-outward**: every stone and
boulder in the game shaded to a flat ~5% grey because `ndotl` collapsed. Both the
normal and the triangle winding are now outward-facing. The scatter pass disables
backface culling, which is why the bug showed up as black rocks rather than
inside-out ones.

## Time of day and weather

`game/map/environment_lighting.cpp` holds the per-hour keyframes, and they **override**
`shadow_tint` and `shadow_strength` from `scene/environment_lighting.h` for any scene
using a time-of-day profile. Change shadows there, not in the header, unless the scene
has no profile.

Night (0/22/24) needs three things to read as cozy rather than merely dark: moonlight
that is actually blue (so firelight has something cool to contrast against), ambient
high enough that the ground stays legible (0.30 — at 0.12 four fifths of the frame sat
below luma 20), and a shadow tint well off black.

Afternoon (17) is defined by sun _height_, not colour. At y=0.55 it was indistinguish-
able from noon; at y=0.26 it gets the long raking shadows that make golden hour read.
A low sun costs direct light on every up-facing surface, so intensity and ambient have
to rise to pay it back or the scene just goes dim.

Snow in `apply_weather` does more than tint the ground bounce. A snowfield throws light
back everywhere, so shadows weaken and soften and go blue, and the horizon hazes to a
pale cold white. Without those the ground reads as green terrain with white stains.

Known gap: the Iron Sepulcher profile now carries a cold violet shadow tint, but every
scenario that selects the profile is a night scene, where shadow strength is already
low — so the tint currently has no measurable effect. It is there for daylight sepulcher
maps that do not exist yet.

Known gap: several scenarios set `exposure_override` (2.3, 2.1, 1.45) tuned against the
old pipeline. These now stack on `k_soi_grade_exposure` before the tone curve. The
shoulder absorbs it rather than clipping, but those scenes sit high on the curve where
contrast compresses, and the overrides deserve a sweep.
