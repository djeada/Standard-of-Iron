# Campaign Map Film

`campaign_map_film` films the campaign's Mediterranean map for "The road"
sequence of each episode (#1531, epic #1547): a camera flight over the map with
Hannibal's march drawing in, regions lit, battle sites pinned and dated, and
army strengths exported for the conform stage.

It renders the game's own `CampaignMapView` (the item `MediterraneanMapPanel.qml`
shows on the campaign screen) through `QQuickRenderControl` into an offscreen
target. There is no QML shell, so there is no UI chrome: no legend, no buttons,
no tooltips, no Hannibal portrait. Everything on the map is the game's renderer;
the only additions are the film overlays described below.

## Quick start

Run from the repository root (the tool reads `assets/`, `tools/map_pipeline/` and
`tools/campaign_film/` relative to it), on the real display:

```sh
cmake --build build --target campaign_map_film

# A quick proxy of the definition-of-done shot: 720p, no supersampling.
DISPLAY=:0 build/bin/campaign_map_film \
  --spec tools/campaign_film/specs/carthago_nova_to_cannae.json \
  --width 1280 --height 720 --supersample 1 \
  --out artifacts/campaign_film

# A few stills to check framing without encoding anything.
DISPLAY=:0 build/bin/campaign_map_film --spec <spec> --stills 0,5,8.6,17.6 \
  --width 1920 --height 1080

# The real take: 3840x2160, 2x supersampled (7680x4320 internally).
DISPLAY=:0 build/bin/campaign_map_film --spec <spec> --codec prores
```

`--validate-only` parses the spec and writes `timeline.json` without opening a
GL context, so it runs anywhere.

Outputs land in `<out>/<spec id>/`:

| File | What it is |
| --- | --- |
| `<id>.mp4` / `<id>.mov` | The clip. H.264 CRF 10 by default; `--codec prores` writes ProRes 422 HQ 10-bit for the edit. |
| `timeline.json` | Resolved timing: route window and every stop's progress and arrival time, marker/label/stamp/region in and out times, army keys with resolved times, the camera keys, warnings. |
| `overlays.json` | Per frame, in output pixels (origin top-left): the route head, every marker, label and stamp with its alpha, and every army counter with its anchor position and values. |
| `frames/frame_NNNNNN.png` | Only with `--frames`. |
| `stills/still_TTT.TT.png` | Only with `--stills`. |

Other switches: `--start`/`--end` (seconds) render a slice, written as
`<id>_fFFFFFF-EEEEEE.mp4` and `overlays_fFFFFFF-EEEEEE.json` (first frame,
end frame exclusive) so slices concatenate without overwriting each other; `--no-clip` skips
encoding; `--no-text` keeps marker glyphs but drops every name, label and stamp
so the graphics package (#1534) can letter the shot from `overlays.json`;
`--draw-armies` paints the exported army figures in magenta as a debug check of
their anchors (never for delivery); `--width`, `--height`, `--fps` and
`--supersample` override the spec. `--march`, `--catalog` and `--bounds` point at
other data files.

The tool refuses software renderers (llvmpipe) unless
`SOI_PROMO_ALLOW_LOW_QUALITY` is set, like promo capture, and logs the GPU it
rendered on. It needs a display: `QT_QPA_PLATFORM=offscreen` cannot create the
GL context.

## Thermal guard and long takes

Every 30 frames the tool reads the CPU package temperature (`coretemp` hwmon)
and the GPU temperature (`nvidia-smi`); above 88 C CPU or 83 C GPU it pauses
until they fall below 78 C and 75 C. A 4K take at 2x supersampling renders a
7680x4320 frame and reads it back every frame, roughly 0.5-1 s a frame; render
long takes in slices with `--start`/`--end` and a rest between them rather than
in one unattended run. `--no-thermal-guard` disables the check.

## The map's edges

The map covers 10° W - 18° E, 30° - 47.5° N. Where land runs into that border
(north of the Alps, the Balkans and the heel of Italy on the east, the Sahara on
the south) the terrain stops in a straight cliff, which must never be in shot.
Every run, including `--validate-only`, projects the land stretches of the four
borders for every frame and reports the time ranges in which any is visible
(`edge_frames` and `warnings` in `timeline.json`, and on stderr), naming the
edge. `forbid_world_edge: true` turns that into a failure before anything is
rendered — the same contract as the arena promo specs' `forbid_world_edge`.
Italy is the hard case: Cannae sits 2° from the east border. The shipped spec
swings round to look west-north-west from over the Adriatic (`yaw` ≈ 235-240) once
the route enters Italy, which puts the east border behind the camera.

## Spec schema

A spec is one JSON object. Field names and easing names follow the arena promo
specs (`docs/PROMO_CAPTURE.md`): `time`, `ease` (`linear`, `smooth`, `in`,
`out`), `interp` (`spline` or `keys`) and `ends` (`ease` or `moving`).

### Output

| Field | Default | Meaning |
| --- | --- | --- |
| `id`, `title` | required, id | Output directory name and a human title. |
| `width`, `height` | 1920, 1080 | Output size. |
| `fps` | 30 | Frame rate. |
| `supersample` | 1 | Internal render scale, 1-4. The frame is rendered at `width*s x height*s` (the map's own 4x MSAA on top) and box-filtered down. |
| `duration` | 10 | Seconds. |
| `reference_height` | 1080 | Every pixel size in the spec (route width, text size, rim width) is authored at this height and scaled to the render, so a 720p proxy and the 4K take frame identically. |
| `terrain_height_scale` | 0.10 | Relief exaggeration. The campaign screen uses 0.085 seen almost straight down; the heightmap is noisy, so oblique film angles read better lower (the shipped spec uses 0.055). |
| `drape_radius` | 0.008 | The route, its head, markers and the camera target sit on the terrain heights averaged over this radius (UV), so the line follows the relief without zigzagging over every ridge. |
| `borders` | false | The game's province border lines. Off: they are gameplay borders, and the regions are lit explicitly. |
| `forbid_world_edge` | false | Fail (exit 5) if the edge of the map's land is ever in frame; see below. |
| `province_fills` / `province_fill_alpha` | false / 0.6 | The campaign screen's owner tint per province. Off by default: the film lights regions explicitly. |
| `symbols` | true | The map's city and mountain glyphs. |
| `game_route` | false | The campaign screen's eight mission lines. Off: they are the game's route, not the historical march. |
| `burn_text` | true | Draw names, labels and stamps into the frame. |

### Camera keys (orbit, pan, zoom, tilt)

```json
"interp": "spline", "ends": "ease",
"camera": [
  { "time": 0, "look": { "uv": [0.335, 0.5] }, "distance": 0.62, "yaw": 186, "pitch": 62, "fov": 40 },
  { "time": 8.4, "look": { "stop": "alps" }, "distance": 0.44, "yaw": 197, "pitch": 46 },
  { "time": 11.2, "look": { "head": true, "smooth": 1.2 }, "distance": 0.6 }
]
```

- **pan** is `look`, the point the camera orbits: `{"uv": [u, v]}`,
  `{"lonlat": [lon, lat]}`, `{"stop": "<march stop id>"}` or
  `{"head": true, "smooth": seconds}`, which follows the route's drawing head
  (averaged over +-`smooth` seconds so the camera does not jerk at every
  corner of the route). Any target takes an `offset: [du, dv]`. The camera
  looks at the terrain surface under the target, so a close orbit around the
  Alps centres on the peaks rather than on sea level beneath them.
- **orbit** is `yaw` in degrees: where the camera stands around the target.
  180 stands south looking north (north up, as on the campaign screen); 270
  stands east looking west. Yaw takes the shorter way round between keys.
- **tilt** is `pitch` in degrees above the map (90 = straight down, 1-90).
- **zoom** is `distance` (map units; the map is 1x1) and/or `fov` (degrees).
  Distance interpolates in log space, so a zoom reads at an even speed.
- A key may omit any field; it inherits the previous key's value.
- `interp: "spline"` (default) runs a time-parametrised cubic Hermite spline
  through the keys, as promo orbit keys with `"interp": "spline"` do; the camera
  moves through a middle key instead of stopping on it. `ends: "ease"` (default)
  starts and finishes at rest; `"moving"` keeps travelling at the cut.
  `interp: "keys"` blends key to key with each key's `ease` instead.

### Route

```json
"route": {
  "from": "carthago_nova", "to": "cannae",
  "width": 6.0, "head": true, "head_radius": 8.5, "ghost": 0.0,
  "keys": [
    { "time": 1.0, "progress": 0.0 },
    { "time": 4.6, "at": "pyrenees" },
    { "time": 8.6, "at": "alps", "ease": "out" },
    { "time": 17.6, "at": "cannae", "ease": "out" }
  ]
}
```

The route is the historical march in `assets/campaign_map/hannibal_path.json`
(`"march"`, written by `tools/map_pipeline/hannibal_path.py`). `from`/`to` pick
a window of it; `progress` is 0..1 along that window by arc length, and `at`
names a stop, so each segment between two stops reveals at its own pace — that
is the per-segment reveal. Between keys the head moves by the incoming key's
`ease` (default `linear`); repeating a stop at two times holds the line there.

The line is drawn by the map renderer itself, draped on the terrain relief and
built in screen space, so its width is constant in pixels at any zoom: a dark
casing, a gold body and a red core (`casing_color`, `gold_color`, `core_color`
override them as `[r, g, b, a]` 0..1), with a soft drop shadow (`shadow`). The
head is a disc in the same colours with a slow pulse. `ghost` > 0 draws the
unrevealed remainder of the window faintly ahead of the head.

### The march

`hannibal_path.json` keeps the eight mission `lines` the campaign screen draws
unchanged and adds `march`: one polyline (`points`, UV) plus named `stops`,
each with `id`, `name`, `date`, `kind` (`city`, `battle`, `crossing`, `pass`,
`waypoint`), `lonlat`, `uv` and the `index` of its point. It follows Polybius
(III.33-118) and Livy (XXI-XXII, XXX): Carthago Nova, Saguntum, the Ebro, the
Pyrenees, the Rhône crossing, up the Isère to the Alps, Taurasia, Ticinus,
Trebia, Bononia and the Arno marshes, Trasimene, Spoletium, the Adriatic coast,
Gerunium, Cannae, then Capua, Tarentum, Croton, the crossing to Hadrumetum, and
Zama. Edit `MARCH_STOPS` in `hannibal_path.py` and rerun it;
`tests/scripts/test_hannibal_march.py` fails if the committed asset drifts from
the generator or the mission lines change.

### Regions

```json
"regions": [
  { "id": "iberia", "in": 0.0, "out": 4.4, "fade": 0.9, "dim_outside": 0.22 },
  { "id": "apulia", "in": 16.4, "fill": [0.93, 0.74, 0.38, 0.3], "rim": [0.98, 0.83, 0.48, 0.95] }
]
```

`id` names a region in `tools/campaign_film/regions.json` (Iberia, Gallia
Transalpina/Cisalpina, Etruria, Italia, Sicilia, Africa, Numidia, the Alps,
Apulia, Campania, Samnium); a spec may instead give `provinces` (ids from
`provinces.json`) or a `lonlat` polygon of its own. Polygons are clipped to land
by the map's own land mask, so their sea sides can be loose — only the land
borders need care. A lit region gets a warm wash (`fill`), a screen-space rim
(`rim`, `rim_width` px) and optionally dims the rest of the map
(`dim_outside`, 0..1). `in`/`out`/`fade` set its window; `keys` (`time`,
`amount`) can shape it further.

### Markers, labels and stamps

```json
"markers": [{ "site": "trebia", "appear": "arrival", "fade": 0.5, "label_hold": 2.6 }],
"labels": [{ "text": "Gallia Cisalpina", "style": "region", "size": 24, "at": { "lonlat": [10.3, 45.6] }, "in": 9.2, "out": 12.6 }],
"stamps": [{ "title": "Cannae", "subtitle": "2 August 216 BC", "in": 17.9 }]
```

- **markers** pin a march stop (`site`) or a `lonlat`. Battle stops draw a
  ringed cross, other places a dot, with the name in the game's display face
  (Standard Iron Display) and the date (`date`, from the march unless given) in
  EB Garamond beneath. `appear: "arrival"` brings the marker in exactly when the
  drawing head reaches it; `appear: <seconds>` or `in` sets it by time. `hold`
  removes the marker after that long; `label_hold` fades only its name and date,
  leaving the glyph on the map. `label: false` / `show_date: false` drop either
  line.
- **labels** are free text at a target: `region` (spaced display capitals,
  ink at 60%), `place` (display capitals with a parchment halo) or `sea`
  (spaced Garamond in sea ink). `size` is in reference pixels; `offset` moves it.
- **stamps** are the lower-left date/place stamp: a display-face title, a short
  gold rule and a Garamond subtitle, inside the 6% title-safe margin.

All text is restrained on purpose — the map carries the shot. The display face
has no lowercase, so display text is set in capitals.

### Army counters

```json
"armies": [{
  "id": "hannibal", "label": "Hannibal", "side": "carthage", "anchor": "head",
  "source": "Polybius III.35, III.56, III.60, III.114",
  "keys": [
    { "time": 0, "foot": 90000, "horse": 12000 },
    { "at": "pyrenees", "foot": 50000, "horse": 9000 },
    { "at": "rhone", "foot": 38000, "horse": 8000 }
  ]
}]
```

Armies are never burned in. Each key holds any numeric fields; `at` resolves to
the stop's arrival time (plus `time` as an offset), so a figure changes as the
line arrives. Values step at each key (`"interp": "linear"` counts between
them). `timeline.json` lists the keys with resolved times and a formatted line
("38,000 foot, 8,000 horse"); `overlays.json` gives, for every frame, the
anchor's screen position, the alpha of the army's window and the current values,
so the conform stage can set the counters in the series graphics and keep them
pinned to the head.

## Definition-of-done shot

`tools/campaign_film/specs/carthago_nova_to_cannae.json` is the #1531
definition of done: 20 seconds, 3840x2160 at 30 fps, 2x supersampled. The
camera opens oblique over Iberia with the province lit and the
"Carthago Nova — Spring 218 BC" stamp, the route draws in from 1 s, eases into
the Alps at 8.6 s for a closer pass over the relief, then swings round to look
west across Italy from over the Adriatic as the line runs past Ticinus, Trebia
and Trasimene, and settles on Cannae at 17.6 s with Apulia lit and the
"Cannae — 2 August 216 BC" stamp. It sets `forbid_world_edge`. Hannibal's and Rome's strengths are
exported, not drawn.

## Source map

| Concern | Source |
| --- | --- |
| Film CLI, render loop, encoder, outputs | `tools/campaign_film/main.cpp` |
| Spec schema, timeline, camera spline, route timing | `tools/campaign_film/campaign_film_spec.{h,cpp}` |
| Labels, markers, stamps | `tools/campaign_film/campaign_film_overlay.{h,cpp}` |
| Route geometry, camera projection, frame state | `ui/campaign_route_path.h` |
| Terrain heights for draping and projection | `ui/campaign_map_film.{h,cpp}` |
| Film rendering in the map renderer | `ui/campaign_map_view.cpp` (`render_film`) |
| The march | `tools/map_pipeline/hannibal_path.py` → `assets/campaign_map/hannibal_path.json` |
| Region catalogue | `tools/campaign_film/regions.json` |
| Tests | `tests/tools/campaign_film_spec_test.cpp`, `tests/scripts/test_hannibal_march.py` |

The campaign screen never sets a film state, so the in-game map renders exactly
as before; the film path is a separate branch of the renderer.
