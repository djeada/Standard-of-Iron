Campaign map plan and current status for the QML/OpenGL widget with a Mediterranean base map (UV space, dynamic provinces/highlights/flags/Hannibal path via JSON).

---

## Status checkpoint
* Bounds locked (West Med: lon -10..18, lat 30..47.5), equirectangular → normalized UV (0..1).
* Offline pipeline in `tools/map_pipeline` generates:
  * Geometry: `land_mesh.bin` (triangulated), `coastlines_uv.json`, `rivers_uv.json`, `land_uv.geojson`
  * Textures: `campaign_base_color.png`, `campaign_water.png`
  * Visualization helpers: `campaign_preview.png`, OBJ exports for mesh/rivers/coastlines
* Scripts: `preprocess_map.py`, `render_textures.py`, `render_preview.py`, `export_all_obj.py`
* Manual validation: OBJ overlay aligns in Blender; preview looks correct.

## Next immediate tasks
1. Integrate assets into runtime:
   * Add to resources/qrc and load `campaign_base_color.png` / `campaign_water.png`
   * Implement QQuickFramebufferObject renderer that draws base quad + line layers (coast/rivers) from UV data
2. Province data path:
   * Author/import provinces GeoJSON → convert to `assets/campaign_map/provinces.json` (UV polygons + anchors)
   * CPU hit-test and VBO packing for per-province tinting
3. Interaction:
   * Pan/zoom within UV bounds
   * Hover/click highlight + tooltip; mission list ↔ map selection sync
4. Overlays:
   * Flags/ownership tint updates from JSON state
   * Hannibal path polyline + animated marker
5. Polish/stylization:
   * Optional relief/paper layers; line weights/palette tuning; pulse/hover outline animation

---

## Original data inputs (kept for reference)
* Natural Earth 10m land + rivers (public domain) clipped to bounds.
* Optional: relief/height sources for shading/hatching if we add `campaign_relief.png`.

---

## Runtime renderer plan (QML + OpenGL)
1. Load base textures (color + water) and UV geometry (land mesh, coast/rivers polylines).
2. Draw order: water gradient → land fill quad/mesh → coastlines/rivers ink → provinces (tint VBO) → highlights/flags/path → labels/tooltips in QML.
3. Interaction: map UV camera with clamp; CPU hit-test provinces; hover/selection feedback.
4. Data feeds: `provinces.json`, `missions.json`, `campaign_state.json`, `hannibal_path.json` drive tints, flags, and path highlight.
5. Performance: single VBO for provinces with per-province ranges; batched lines; minimal draw calls.

---

## Milestones (updated)
1. Offline pipeline outputs verified (done)
2. Base textures + lines integrated in QML renderer
3. Province data import/render + hit-testing
4. Interaction sync with mission list + tooltips
5. Ownership/flags animations; Hannibal path overlay
6. Stylization polish (relief/paper, hover pulse, labels)

---

## Notes
* We’re staying on equirectangular UV; all authored data must use the same bounds/transform.
* Keep heavy lifting offline (triangulation, rasterization) to keep runtime simple.
* Stylization (relief/paper) can be layered later without changing UV data.

[1]: https://www.naturalearthdata.com/?utm_source=chatgpt.com "Natural Earth - Free vector and raster map data at 1:10m, 1 ..."
[2]: https://www.soest.hawaii.edu/pwessel/gshhg/?utm_source=chatgpt.com "GSHHG - A Global Self-consistent, Hierarchical, High- ..."
[3]: https://www.naturalearthdata.com/downloads/10m-physical-vectors/?utm_source=chatgpt.com "1:10m Physical Vectors"
[4]: https://gdal.org/en/stable/programs/gdal_rasterize.html?utm_source=chatgpt.com "gdal_rasterize — GDAL documentation"
[5]: https://doc.qt.io/qt-6/qquickframebufferobject-renderer.html?utm_source=chatgpt.com "QQuickFramebufferObject::Renderer Class | Qt Quick"
[6]: https://doc.qt.io/qt-6/qsgrendernode.html?utm_source=chatgpt.com "QSGRenderNode Class | Qt Quick | Qt 6.10.1"
