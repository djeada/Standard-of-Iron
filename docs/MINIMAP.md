# Minimap Architecture

The HUD minimap combines four cached raster layers with one live QML overlay. The split is designed around invalidation: stable pictures of world state are rebuilt only when their inputs change, while continuously animated signals stay on the scene graph.

That keeps the normal per-frame CPU cost close to zero when nothing visible on the minimap has changed.

## Rendering layers

| Layer                           | Owner                  | Rebuilt when                                       |
| ------------------------------- | ---------------------- | -------------------------------------------------- |
| Parchment base                  | `MinimapGenerator`     | once, when the map loads                           |
| Fog                             | `MinimapFogCompositor` | visibility cells change                            |
| Units                           | `UnitLayer`            | the quantized marker hash changes                  |
| Camera viewport                 | `CameraViewportLayer`  | the camera target or frustum footprint moves       |
| Events, destinations, landmarks | `MinimapOverlay.qml`   | driven by properties and signals; never rasterized |

The first four layers are `QImage` surfaces composited by `MinimapManager`. The fifth is vector QML, allowing blips and other transient elements to animate on the scene graph without asking the simulation thread to repaint an image.

The division rule is simple:

> Stable world state belongs in a raster layer. Continuously animated or transient information belongs in the QML overlay.

## Marker hierarchy

`MarkerClass` in `game/render_bridge/minimap/unit_layer.h` defines the visual and draw-order hierarchy. Less important markers are drawn first so strategic landmarks remain readable even when troops stand directly on top of them.

| Class            | Spawn types                         | Presentation                             |
| ---------------- | ----------------------------------- | ---------------------------------------- |
| `MinorStructure` | home, farm, wall segment, wall gate | small ink-blended dot without a border   |
| `Troop`          | fighting units                      | pigment disc with an ink rim             |
| `Tower`          | defense tower                       | narrow tower with a pitched roof         |
| `Landmark`       | temple, marketplace                 | pediment glyph                           |
| `Stronghold`     | barracks / village                  | twin-turret keep with an ink cast shadow |

An unclaimed village uses a bone-colored fill under a heavy ink border. This keeps neutral strongholds distinct from owned ones without making them visually weak.

## Strongholds stay visible through fog

Villages are the primary territorial objective, so `Stronghold` is the only marker class exempt from the fog visibility cull in `UnitLayer::update`.

Every village is shown at its known location and in the color of its current owner even when the surrounding ground is unexplored. Enemy troops remain hidden by fog; the exception applies only to strongholds.

Because the live stronghold marker must cover the stone keep baked into the parchment base, `k_stronghold_scale` is sized relative to `structure_icon_size`. Changes to either value should be reviewed together or the underlying baked icon can appear as an unintended halo.

## Cartographic art direction

The minimap is styled as an inked military chart rather than a modern radar panel.

### Pigments instead of screen primaries

`TeamColors` uses historical-pigment-inspired colors such as woad, iron oxide, verdigris, orpiment, Tyrian purple, and celadon. Each marker receives a near-black outline tinted toward its own hue so it reads as ink applied to parchment rather than a floating UI element.

### Silhouettes instead of generic primitives

Buildings use recognizable cartographic glyphs rather than rectangles. The keep silhouette is defined once in `keep_polygon` in `minimap_utils.h` and shared between the baked landmark and live ownership layer so the two representations cannot drift apart.

### Detail has to survive at minimap scale

A glyph around 13 px across can carry only a few bold features. Tiny notches and one-pixel gaps disappear beneath the outline stroke, so readable shapes should use broad steps and strong negative space rather than miniature architectural detail.

### Baked landmarks show place; live markers show ownership

The parchment base can permanently show that a settlement exists, but it cannot encode an owner because ownership changes during the match. Live unit-layer markers are drawn over the baked landmark and provide the current faction color.

The map-selection preview is separate: `MapPreviewGenerator` draws its own lobby-colored base markers.

### Compass orientation is explicit

A compass rose is baked into the lower-left of the base image, with its long ray aligned to true map north. It is generated once at load time and clarifies orientation on maps whose minimap is rotated by the default 225° camera yaw.

Wildlife is excluded from the unit layer. Sheep and wolves move frequently, add little strategic value to the chart, and would otherwise invalidate the marker hash continuously.

## Capture progress

A stronghold with an active `CaptureComponent` receives an arc outside its footprint. The arc sweeps clockwise from twelve o'clock in the capturing player's color.

If `capture_blocked` is true, the ring switches to the contested amber treatment.

Progress is quantized to `k_capture_steps`, currently 12, before entering the unit-layer hash. Without quantization, every small capture-progress change would force the entire raster layer to repaint on every update.

## The minimap event channel

`MinimapViewModel::note_alert` is the single entry point for transient events that blink or pulse on the map.

It:

1. converts the world position to normalized minimap coordinates;
2. determines the event's relation to the local player;
3. applies throttling; and
4. emits one `event_blip` signal consumed by `MinimapOverlay.qml`.

New blinking event types should be added through this path rather than by adding another independent `Repeater` to `HUDTop.qml`.

Current event kinds are:

- `troops_attacked`;
- `structure_attacked`;
- `capture_started`;
- `capture_contested`;
- `capture_finished`; and
- `shrine`.

### Event relation describes impact, not ownership

| Relation   | Meaning                                    | Presentation   |
| ---------- | ------------------------------------------ | -------------- |
| `self`     | our units or holdings are the target       | danger         |
| `ally`     | an ally's units or holdings are the target | warning        |
| `friendly` | we or an ally are gaining                  | success        |
| `enemy`    | two other parties are involved             | secondary text |

An `enemy` event is hidden unless its position passes the same visibility test as unit markers. Battles between AI players remain invisible when they occur inside unscouted fog.

## Alert throttling

Two independent mechanisms keep large fights from turning the minimap into a strobe.

### Early budget gate

`consume_alert_budget()` applies a 60 ms gate that callers can check before resolving an event. The combat-hit path uses it so a dense melee pays only a boolean check per hit instead of repeated component lookups.

### Spatial and per-kind cooldown

`accept_alert()` uses a direct-mapped 32-slot cache keyed by event kind and a 12 × 12 minimap cell. Each event type has its own cooldown. A cache collision can produce at most one extra blip.

Capture-completion and shrine events use a zero cooldown because they are rare and important enough to always show.

## Showing movement destinations

Selected troops with a movement goal publish their destinations through `MinimapViewModel.destinations`.

Goals within two world units of one another collapse into one marker, and the list is capped at eight destinations. Each item also carries the selection centroid, allowing the overlay to draw a visual leash from the squad toward its goal.

The centroid is quantized before it contributes to the destination hash. A marching formation therefore republishes the QML property only a few times per second instead of at full simulation update frequency.

## Landmark feed

Some strategic objects are world props rather than entities in the unit layer.

### Iron Sepulcher shrines

`UndeadAwakeningSystem::shrine_markers()` reports shrine landmarks. `GameEngine` polls the feed twice per second and publishes the result through `MinimapViewModel.landmarks`.

Dormant, awakened, and cleared shrines receive different treatments, and awakened shrines pulse in the overlay.

### Cursed gold veins

`CursedGoldVeinSystem::vein_markers()` uses the same landmark channel with kind `gold_vein` and state `neutral`, `owned`, `enemy`, or `destroyed`. Those states map to gold, success, danger, and disabled treatments respectively. See [CURSED_GOLD_VEIN.md](CURSED_GOLD_VEIN.md).

## Coordinate conversion

`Game::Map::Minimap::world_to_pixel` expects **grid coordinates**, not raw world coordinates. Before normalization, the coordinates are rotated by the map's camera yaw, which is 225° by default.

Passing raw world coordinates is incorrect on any map where `tile_size != 1`.

Call `MinimapManager::world_to_normalized` instead. It performs the world-to-grid division, rotation, normalization, and clamp in one shared path. Every pin publisher should use that helper.

## Rules for extending the minimap

When adding new minimap state, preserve the invalidation model:

- If a marker gains a visual property, include that property in the marker hash in `MinimapManager::update_units`; otherwise changes can fail to repaint.
- Quantize continuously changing values before hashing them, or the whole raster layer will refresh every update.
- Put continuously animated elements in the QML overlay rather than a raster image.
- Gate every animation on `Design.A11y.reducedMotion`.
- Differentiate event kinds and strategic states by shape or structure as well as color.

The minimap stays inexpensive because every layer has a clear ownership and invalidation rule. Preserving those boundaries matters as much as the appearance of any individual marker.
