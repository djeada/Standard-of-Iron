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
  ├─ composite         + bloom → grounding AO → depth fog → tonemap → grade → vignette → RGBA8
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
`pine_instanced`, `olive_instanced`, `magic_shrine_instanced`, `iron_ore_instanced`,
and now `dead_tree_instanced`, `bridge`, `road`, `ground_plane`, `riverbank` and
`banner`. `ShaderSource.WorldSurfacesTakeTheirKeyLightFromTheSharedModel` pins the list.

The ground surfaces (`road`, `ground_plane`, `riverbank`) mattered more than they look:
they were shading off a hard `max(dot(n, l), 0.0)` while every object standing on them
used the wrapped terminator, so a road crossing a shadow edge broke against the
buildings beside it. They take `soi_key_light(n) * K` now, keeping each surface's own
key attenuation.

`dead_tree_instanced` was not merely inconsistent, it was wrong: it built
`lighting` from `environment_primary_intensity()` and then multiplied by a `light_tint`
that already carried the same intensity, so dead wood was lit by the square of the sun
— too hot in the afternoon and too dark at night, on a curve nothing else followed.

Still deliberately on their own lighting: `terrain_chunk`, `character_skinned(_instanced)`,
`river`, `plant_instanced` and `sky`. The first three are separately art-directed and the
foliage one carries authored subsurface and sun-catch terms that the shared curve would
flatten.

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

The term is sampled as three rings (`k_ao_ring_spread`) rather than one, because a
single 3px ring is an edge-detect filter, not a contact shadow — it produced a hairline
and nothing else. Each ring outward also accepts a larger depth step
(`k_ao_ring_reach`), since a wide contact shadow comes from a taller occluder.

Debugging note, because this is easy to get wrong twice: replace the composite's
`soi_finalize(combined)` with `vec3(grounding_occlusion())` and render a frame. The
term should show as a clean halo tracing every silhouette. When it did, its peak was
only 0.32, and `k_ground_ao_strength` was then 0.62 — a 5% darkening at the very
darkest pixel, i.e. invisible. The ring count and the acceptance window were both
red herrings; the strength was the bug. It is 2.40 now, and the composite clamps the
mix factor so raising it further saturates rather than inverts.

**Known limitation.** Cast shadows disappear at wide cameras. `render_directional_shadows`
culls a static caster by its distance from the camera, so at High (80) nothing beyond
80 units casts, while `scene/camera.h` lets the player pull back to
`k_max_rts_distance` = 85. Raising High to 140 was measured on `riverside_mill_town`:
frame cost went 1.96 → 12.60 ms p50 (2.35 → 18.58 ms p95) and the frame gained only
some blurry terrain self-shadow — the caster loop re-binds and re-draws every mesh per
cascade with no batching, so coverage is priced per caster per cascade. It was reverted.
Closing this properly is a shadow-pass batching job, not a lighting tweak. What keeps
objects planted at that range today is the contact-shadow fallback, whose reach
(`shadow_max_distance`) does cover the full zoom —
`GraphicsLightingSettingsTest.GroundingReachesTheFullyZoomedOutCamera` pins that.

## Atmospheric fog

Distance fog is a single screen-space term in `post_composite.frag`, not a per-shader
feature. The composite reconstructs the world position of every pixel from the scene
depth texture (`u_inverse_view_proj`, set by `Backend::execute_scene` via
`PostProcessPipeline::set_atmosphere`) and applies `atmospheric_fog_amount` from
`environment_lighting.glsl` — the composite binds the environment UBO for exactly this,
so fog colour and weather density follow the same time-of-day keyframes as the world.
The fog range still comes from `fog_range_for_camera` (`render/gl/backend/fog_range.h`),
which scales with orbit distance so zooming out does not drown the map.

Before this, `terrain_chunk`, `ground_plane`, `riverbank` and `river` each applied
their own fog while every building, tree, unit and prop applied none — a distant
building sat hard-edged and full-contrast on terrain that was hazing out behind it,
the exact opposite of the reference games' painterly falloff. Fogging in the composite
covers everything in the depth buffer with one consistent curve, and the per-surface
fog uniforms (`u_fog_start`/`u_fog_end`) were retired with it.

Two deliberate details:

- **Sky pixels are skipped** (same far-plane test the grounding AO uses). The sky
  already blends `fog_color` at the horizon; fogging it again would wash the zenith.
- **`k_fog_desaturation`** pulls hazed pixels toward their own luma before the mix
  toward fog colour, so mid-distance objects lose chroma slightly before they lose
  value — that is what makes the falloff read as painted haze rather than grey soup.

Fog runs after the grounding AO (so contact shadows fade with distance like everything
else) and before the tone curve, in linear radiance like the per-surface fog it
replaced. Translucent surfaces fog by the opaque depth behind them, which at RTS
camera angles is indistinguishable from their own distance.

## Ground mist

Local mist — the low white haze on river and lake banks, and the violet miasma over
undead zones — is the same screen-space mechanism, evaluated per pixel in the
composite against a small list of analytic **mist volumes** (`render/mist_volume.h`):
capsules in the XZ plane for rivers, discs for lakes and undead zones, at most
`k_max_mist_volumes` of them. A pixel's mist is lateral falloff past the volume edge
(`k_mist_bank_reach`), times a vertical envelope above the volume's base height
(`k_mist_water_ceiling` / `k_mist_miasma_ceiling`), times two octaves of drifting
value noise so the blanket breathes. Because it is applied by world position and
depth, a unit wading a ford is misted at the knees and clear at the helmet with no
billboards involved.

Both mist colours derive from the environment fog colour so they follow time of day:
water mist is the fog colour lifted (`k_mist_water_lift`), miasma is the fog colour
hue-shifted violet (`k_mist_miasma_tint`) — at night it stays a dark cold violet
rather than glowing.

The volumes are built once per level in `app/core/skirmish_loader.cpp`
(`build_mist_volumes`): undead zones first (so the cap never drops them), then river
polylines — chained back together from their authored waypoint segments and
Douglas-Peucker-simplified so a 40-waypoint river costs a handful of capsules — then
lakes. Base heights come from the terrain service. They reach the composite through
`Renderer::set_mist_volumes → Backend → PostProcessPipeline::set_mist`, and the
uniform upload happens only when the set changes.

This is distinct from the fog-of-war shroud (`fog_instanced.frag` + FogZone patches
in `AmbientFogRenderer`), which is a visibility-masked gameplay surface, not
atmosphere; the two coexist.

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
able from noon; at y=0.24 it gets the long raking shadows that make golden hour read.
A low sun costs direct light on every up-facing surface, so intensity and ambient have
to rise to pay it back or the scene just goes dim. Pushing the sun even lower (y=0.19)
was tried and reverted: `ndl` on flat ground collapses with it, the sun term starves,
and the field goes _duller_, not more golden.

The golden cast itself cannot come from the sun colour alone, because up-facing
surfaces take their ambient from `sky_color` (the hemisphere mix) — with a blue sky
the field stays midday-green no matter how amber the key is. Golden hour therefore
warms `sky_color` itself ({0.66, 0.59, 0.52} — the ambient integral of a low sun is
horizon scatter, not zenith blue), which also warms the rim light and the sky dome,
and moves the coolness into `shadow_tint` ({0.28, 0.29, 0.44}) so the warm/cool split
lives between light and shade instead of between ground and sky. Warmer, denser fog
({0.94, 0.74, 0.48} at 0.005) gives the composite's distance fog and waterside mist
a golden haze for free.

Note when judging this in captures: the arena renders without directional shadow
maps, so the long-shadow half of golden hour only shows in the real game.

Snow in `apply_weather` does more than tint the ground bounce. A snowfield throws light
back everywhere, so shadows weaken and soften and go blue, and the horizon hazes to a
pale cold white. Without those the ground reads as green terrain with white stains.

Known gap: the Iron Sepulcher profile now carries a cold violet shadow tint, but every
scenario that selects the profile is a night scene, where shadow strength is already
low — so the tint currently has no measurable effect. It is there for daylight sepulcher
maps that do not exist yet.

Resolved: several scenarios set `exposure_override` (2.3, 2.1, 1.45) tuned against the
old pipeline, and these stack on `k_soi_grade_exposure` before the tone curve. Measured
on the four highest — `trailer_barrow_night` (2.3), `trailer_night_snow` and
`trailer_last_breath` (2.1), `trailer_clash` (1.7) — every one clips 0.00% of its frame
above luma 250, and p99 lands between 124 and 187. The extended-Reinhard shoulder at
`k_soi_grade_white_point` = 2.80 absorbs the whole range, so the values were left alone.
Re-measure before changing the white point, not before changing an override.
