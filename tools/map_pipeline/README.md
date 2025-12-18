## Campaign Map Preprocessing (offline)

Single-shot pipeline to prep the West Mediterranean campaign map. It locks coordinates, fetches base coast/land data, and emits UV-space assets the QML/OpenGL widget can consume.

### What it does
- Loads fixed bounds from `map_bounds.json` (equirectangular projection, normalized UV).
- Downloads Natural Earth 10m land + rivers (public domain) into `tools/map_pipeline/build`.
- Clips to bounds, projects/normalizes, writes `assets/campaign_map/land_uv.geojson`.
- Triangulates land polygons → `assets/campaign_map/land_mesh.bin` (float32 u,v pairs) and `land_mesh.obj`.
- Extracts coastline/rivers polylines → `assets/campaign_map/coastlines_uv.json`, `rivers_uv.json`, and OBJ line exports.
- Renders preview PNG → `assets/campaign_map/campaign_preview.png`.
- Renders base textures → `assets/campaign_map/campaign_base_color.png` and `campaign_water.png`.

### Dependencies
- Python 3
- `fiona`, `shapely`, `pillow`, `mapbox_earcut`, `numpy` (install via `pip install -r requirements.txt`)

### Run (does everything)
```bash
./tools/map_pipeline/pipeline.py
```
Outputs land/rivers UV, mesh binaries, OBJs, preview PNG, and base textures to `assets/campaign_map/`.
