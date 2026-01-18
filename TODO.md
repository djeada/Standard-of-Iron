# Campaign Map TODO

- Replace broken path lines with a proper stroke mesh (polyline -> triangulated ribbon with joins/caps and multi-pass outline/highlight) so routes look like inked cartography, not GL line strips.
- Add spline smoothing for routes (Catmull-Rom/Bezier) in the pipeline or at load time to remove jagged segments and create curved, coastline-hugging trajectories.
- Introduce true 3D terrain mesh: height-displaced land mesh (DEM -> mesh) with normal map, directional light, and subtle AO so relief reads immediately.
- Upgrade base cartography textures: add hillshade overlay, bathymetry tint, and landform shading for an illustrated map feel.
- Improve coastline and borders: regenerate with higher resolution, consistent smoothing, and double-stroke coastline (dark outer + light inner) for printed-map legibility.
- Province fills with texture instead of flat alpha: faint parchment/ink texture mask per province with owner tint as multiply/overlay.
- Typography polish: use a dedicated serif/engraved style for province labels and city names, consistent scaling with zoom.
- Add cartographic symbols: small mountain icons, city markers, coastal anchors for high-importance regions.
- Cinematic camera defaults: update yaw/pitch/distance and region focus positions to showcase relief and depth by default.
- Mission marker styling: replace the generic circle/emoji with an emblematic badge (seal, standard, or banner) that matches the map style.
