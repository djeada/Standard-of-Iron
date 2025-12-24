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

### Provinces (rough draft)
Generate a gameplay-focused `provinces.json` with hand-authored boundaries (requires `shapely` + `mapbox_earcut`):
```bash
./tools/map_pipeline/provinces.py
```
This writes `assets/campaign_map/provinces.json` (triangulated UVs + colors).

### Hannibal's Campaign Path
Generate progressive path lines for Hannibal's 8-mission campaign:
```bash
./tools/map_pipeline/hannibal_path.py
```
This script uses hardcoded city lon/lat coordinates from `provinces.py` and converts them to UV space using `map_bounds.json`. It generates `assets/campaign_map/hannibal_path.json` containing 8 progressive path lines, one for each campaign mission:

- **Mission 0**: Crossing the Rhône (New Carthage → Massalia)
- **Mission 1**: Battle of Ticino (+ Mediolanum)
- **Mission 2**: Battle of Trebia (consolidates in Cisalpine Gaul)
- **Mission 3**: Battle of Trasimene (+ Veii through Etruria)
- **Mission 4**: Battle of Cannae (+ Capua in Southern Italy)
- **Mission 5**: Campania Campaign (maintains Capua position)
- **Mission 6**: Crossing the Alps (flashback - path through mountains)
- **Mission 7**: Battle of Zama (final - + Syracuse → Carthage)

Each successive path builds upon the previous one, with the last path being the longest and covering all previous waypoints. 

**Coastline-Aware Features:**
- Routes follow the North African coast westward before crossing at Gibraltar
- Coastal segments along Spanish, French, and Italian coasts use smooth curves
- Each segment is classified as 'coastal', 'land', or 'open_sea'
- Open-sea crossings only occur at Gibraltar (Africa→Spain) and Sicily→Carthage
- Intermediate waypoints create natural, organic appearance along shores
- The C++ renderer uses a three-pass system (dark border, gold highlight, red core)
- Line widths: 10px border, 6.5px highlight, 3.5px core for high visibility
- Historical map styling with gold/amber tones for authenticity

**Note**: This script does NOT require `provinces.json` - it calculates UV coordinates directly from lon/lat. If you need to generate `provinces.json` for the full campaign map visualization, run `provinces.py` after running the full pipeline.
