# Campaign Map TODO

## Completed

- [x] Replace broken path lines with a proper stroke mesh (polyline -> triangulated ribbon with joins/caps and multi-pass outline/highlight) so routes look like inked cartography, not GL line strips.
  - *Implementation: `campaign_map_render_utils.h::build_stroke_mesh()` with round caps, miter joins*
  - *Multi-pass rendering: dark border → golden highlight → red core*

- [x] Add spline smoothing for routes (Catmull-Rom/Bezier) in the pipeline or at load time to remove jagged segments and create curved, coastline-hugging trajectories.
  - *Implementation: `campaign_map_render_utils.h::smooth_catmull_rom()` with 8 samples per segment*

- [x] Improve coastline and borders: regenerate with higher resolution, consistent smoothing, and double-stroke coastline (dark outer + light inner) for printed-map legibility.
  - *Implementation: `LineLayer::double_stroke` flag with outer/inner color passes*
  - *Coastline shader: `campaign_coastline.frag` with ink variation*

- [x] Province fills with texture instead of flat alpha: faint parchment/ink texture mask per province with owner tint as multiply/overlay.
  - *Implementation: Parchment tinting applied in `draw_province_layer()`*
  - *Province shader: `campaign_province.frag` with procedural parchment pattern*

- [x] Add cartographic symbols: small mountain icons, city markers, coastal anchors for high-importance regions.
  - *Implementation: `CartographicSymbolLayer` with city/port/mountain extraction from provinces*
  - *Symbol generation: `generate_mountain_icon()`, `generate_city_marker()`, `generate_anchor_icon()`*
  - *Symbol shader: `campaign_symbol.frag` with SDF-based rendering*

- [x] Cinematic camera defaults: update yaw/pitch/distance and region focus positions to showcase relief and depth by default.
  - *Implementation: yaw=185°, pitch=52°, distance=1.35 for northwest tilt and oblique depth*
  - *Region focus positions defined in `CinematicCameraDefaults` namespace*

- [x] Mission marker styling: replace the generic circle/emoji with an emblematic badge (seal, standard, or banner) that matches the map style.
  - *Implementation: `MissionBadge` struct with shield badge at path endpoint*
  - *Badge shader: `campaign_badge.frag` with SDF shapes (shield, seal, banner, medallion)*
  - *Multi-pass: shadow → border → primary fill with gradient*

- [x] Introduce true 3D terrain mesh: height-displaced land mesh (DEM -> mesh) with normal map, directional light, and subtle AO so relief reads immediately.
  - *Implementation: `TerrainMesh` struct with procedural heightmap generation*
  - *Heightmap: `generate_terrain_height()` with Mediterranean mountain ranges (Alps, Pyrenees, Apennines, Atlas)*
  - *Normals: `compute_terrain_normal()` using central differences*
  - *Terrain shader: `campaign_terrain.vert/.frag` with height displacement, hillshade, AO*

- [x] Upgrade base cartography textures: add hillshade overlay, bathymetry tint, and landform shading for an illustrated map feel.
  - *Implementation: `HillshadeLayer` with procedural hillshade texture generation*
  - *Hillshade: `generate_hillshade_texture()` with configurable light direction and vertical exaggeration*
  - *Bathymetry/landform: `getElevationTint()` in terrain fragment shader*

- [x] Typography polish: use a dedicated serif/engraved style for province labels and city names, consistent scaling with zoom.
  - *Implementation: `LabelLayer` with province/city labels extracted from provinces.json*
  - *Label styles: `LabelStyles::province_label()`, `city_label()`, `region_label()`, `sea_label()`*
  - *SDF shader: `campaign_label.vert/.frag` with stroke/fill rendering*
  - *Zoom scaling: `compute_label_scale()` for consistent screen-space sizing*

## Files Added

### Shaders (`assets/shaders/`)
- `campaign_terrain.vert/.frag` - Height-displaced terrain with hillshade, AO, bathymetry
- `campaign_stroke.vert/.frag` - Improved stroke rendering with ink texture
- `campaign_province.vert/.frag` - Province fills with parchment texture
- `campaign_badge.vert/.frag` - Mission marker badges (SDF shapes)
- `campaign_symbol.vert/.frag` - Cartographic symbols (SDF shapes)
- `campaign_coastline.frag` - Double-stroke coastline rendering
- `campaign_label.vert/.frag` - SDF-based typography for labels

### Utility Header (`ui/`)
- `campaign_map_render_utils.h` - Comprehensive rendering utilities:
  - Catmull-Rom spline smoothing
  - Triangulated stroke mesh generation with joins/caps
  - Multi-pass cartographic style definitions
  - Hillshade and terrain coloring utilities
  - Parchment texture pattern generation
  - Cartographic symbol geometry generation
  - Mission badge geometry generation
  - Cinematic camera defaults
  - Mediterranean terrain heightmap generation
  - Hillshade texture generation
  - Typography label styling and geometry


