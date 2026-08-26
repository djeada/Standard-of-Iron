# Minimap

The HUD minimap is composed from four independent layers plus a QML overlay. Each
layer has its own invalidation rule, and the split exists to keep the per-frame
CPU cost near zero when nothing the player can see has changed.

## Layers

| Layer                           | Owner                  | Rebuilt when                                       |
| ------------------------------- | ---------------------- | -------------------------------------------------- |
| Parchment base                  | `MinimapGenerator`     | once, on map load                                  |
| Fog                             | `MinimapFogCompositor` | the visibility snapshot's cells change             |
| Units                           | `UnitLayer`            | the quantised marker hash changes                  |
| Camera viewport                 | `CameraViewportLayer`  | the camera target or frustum footprint moves       |
| Events, destinations, landmarks | `MinimapOverlay.qml`   | driven by signals and properties, never rasterised |

The first four are `QImage`s composited by `MinimapManager`. The fifth is
vector QML: blips animate on the scene graph and cost the simulation thread
nothing.

Anything that animates continuously belongs in the QML overlay. Anything that
is a stable picture of world state belongs in a raster layer.

## Symbol vocabulary

`MarkerClass` (`game/render_bridge/minimap/unit_layer.h`) sets the visual
hierarchy. Draw order runs from least to most important so a village is never
buried under the troops standing on it.

| Class            | Spawn types                         | Drawn as                                 |
| ---------------- | ----------------------------------- | ---------------------------------------- |
| `MinorStructure` | home, farm, wall segment, wall gate | small dot, blended toward ink, no border |
| `Troop`          | everything that fights              | pigment disc under an ink rim            |
| `Tower`          | defense tower                       | slim tower with a pitched roof           |
| `Landmark`       | temple, marketplace                 | pediment                                 |
| `Stronghold`     | barracks (a.k.a. village)           | twin-turret keep with an ink cast shadow |

An unclaimed village is drawn in bone under a heavy ink border, so it reads
differently from one that already belongs to somebody without ever becoming
faint.

### Villages are always charted

Villages are the thing the match is fought over, so they are the one marker
class exempt from the fog visibility cull in `UnitLayer::update`. Every village
is drawn wherever it stands, in the colour of whichever faction holds it right
now, even under unseen fog. Enemy troops are still hidden by fog — that
exemption is for strongholds only.

Because the live marker is always drawn, it has to cover the stone keep baked
into the parchment beneath it; `k_stronghold_scale` is sized against
`structure_icon_size` for exactly that reason. Change one and check the other,
or every village grows a stone halo.

### Art direction

The base is an aged parchment in warm browns, so the markers are drawn as if
inked onto it rather than composited over it:

- **Pigments, not primaries.** `TeamColors` is a set of historical pigments —
  woad, iron oxide, verdigris, orpiment, tyrian purple, celadon — not saturated
  screen primaries. Every marker is outlined in a near-black ink tinted toward
  its own hue, which is what makes it sit on the parchment instead of floating
  above it.
- **Silhouettes, not primitives.** Buildings are cartographic glyphs, not
  rectangles. The keep silhouette lives in `keep_polygon`
  (`minimap_utils.h`) and is shared by the live layer and the baked landmark
  icon, so the two can never drift apart.
- **Detail must survive the pen.** At roughly 13 px a glyph carries about two
  bold features and no more. An earlier keep had three merlons with 1 px gaps;
  the ink stroke swallowed them and it read as a dark trident. Bold steps of a
  third of the glyph width or wider are the floor.
- **Baked landmarks are stone.** Villages baked into the parchment carry no
  player colour, because a baked image cannot follow a capture. They say "a
  settlement stands here"; the live layer, drawn over them, says who holds it.
  The map-select preview is unaffected — `MapPreviewGenerator` paints its own
  lobby-coloured bases on top.
- **A compass rose** sits in the lower-left of the baked image, its long ray
  pointing at true map north. It is drawn once at map load, so it costs nothing
  per frame — and it answers the question the 225° rotation always raises.

Wildlife is filtered out before it reaches the layer. Sheep and wolves wander
constantly; drawing them added noise to the picture and churned the marker hash
every single update.

### Capture rings

A stronghold with an active `CaptureComponent` gets a progress arc outside its
footprint, sweeping clockwise from twelve o'clock in the capturing player's
colour. A blocked capture (`capture_blocked`) draws the ring in the contested
amber instead.

Progress is quantised to `k_capture_steps` (12) before it enters the marker
hash. Without that, a capture in progress would repaint the whole unit overlay
on every update for as long as it lasted.

## The event channel

`MinimapViewModel::note_alert` is the single entry point for everything that
blinks. It resolves the world position to normalized minimap coordinates,
decides the relation, throttles, and emits one `event_blip` signal that
`MinimapOverlay.qml` styles by kind.

Add new blinking events here. Do not add another `Repeater` to `HUDTop.qml`.

Kinds: `troops_attacked`, `structure_attacked`, `capture_started`,
`capture_contested`, `capture_finished`, `shrine`.

Relations answer "who does this hurt or help", not "who owns it":

| Relation   | Meaning                              | Colour         |
| ---------- | ------------------------------------ | -------------- |
| `self`     | our units or holdings are the target | danger         |
| `ally`     | an ally's are                        | warning        |
| `friendly` | we or an ally are the ones gaining   | success        |
| `enemy`    | two other parties, neither ours      | secondary text |

An `enemy` blip is suppressed unless the position passes the same visibility
test the unit markers use, so a fight between two AI players in unscouted fog
stays invisible.

### Throttling

Two independent gates keep a large battle from turning the minimap into a
strobe:

- `consume_alert_budget()` — a 60 ms gate callers check _before_ resolving an
  event. The combat hit path uses it so that a busy melee costs one boolean
  comparison per hit instead of a component lookup.
- `accept_alert()` — a direct-mapped 32-slot cache keyed by (kind, 12×12 map
  cell) with a per-kind cooldown. A hash collision costs at most one extra blip.

Capture completions and shrine events have a zero cooldown; they are rare and
always worth showing.

## Where troops are going

Selected troops with a movement goal publish their destinations as
`MinimapViewModel.destinations`. Goals within two world units of each other
collapse into one marker, capped at eight. Each entry carries the selection
centroid so the overlay can draw a leash from the squad to where it is headed.

The selection centroid is quantised before it enters the destination hash, so
a marching squad republishes the property a few times a second rather than at
the full update rate.

## Iron Sepulcher shrines

Shrines are world props, not units, so they never appear in the unit layer.
`UndeadAwakeningSystem::shrine_markers()` reports them and `GameEngine` polls it
twice a second into `MinimapViewModel.landmarks`. Dormant, awakened and cleared
each get their own tint; an awakened shrine pulses.

## Coordinates

`Game::Map::Minimap::world_to_pixel` takes **grid** coordinates — world
coordinates divided by `tile_size` — and rotates them by the map's camera yaw
(225° by default) before normalising. Passing raw world coordinates is wrong on
any map whose `tile_size` is not 1.

Use `MinimapManager::world_to_normalized`, which applies the tile division and
the clamp for you. Every pin publisher goes through it.

## Rules for changes

- Anything you add to a marker must also go into the hash in
  `MinimapManager::update_units`, or the layer will silently stop refreshing.
- Anything continuous you add to the hash must be quantised first, or the layer
  will repaint on every update.
- Prefer the QML overlay for anything that moves on its own.
- Every animation must be gated on `Design.A11y.reducedMotion`, and kinds must
  differ in shape, not only colour.
