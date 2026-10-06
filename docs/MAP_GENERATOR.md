# Map generator

`scripts/soi-mapgen.py` turns a seed, a preset and a handful of parameters into a
complete, playable battlefield. It does not replace the map pipeline; it authors
the _intent_ that pipeline already consumes and then runs it:

```text
seed + preset + parameters
  -> terrain intent        (hills, ridges, mountains)
  -> water intent          -> scripts/generate-map-water.py
  -> settlement intent     (anchors on open ground)
  -> road intent           -> scripts/generate-map-roads.py   (builds the bridges)
                           -> scripts/generate-map-settlements.py
  -> forest intent
  -> landmark intent       -> scripts/generate-map-landmarks.py
  -> dressing intent       -> scripts/generate-map-dressing.py
  -> spawns
  -> placement             -> scripts/fix-map-prop-overlaps.py
  -> validation + metrics  (water/roads/settlements --validate-only, own checks)
  -> candidate map + report
```

The map editor's **Generator** tab is a front end for the same command; anything
the tab can do, the command line can do, so tests and CI drive the generator
without a window.

The design rules in `scripts/RTS_MAP_DESIGN.md` are the generator's contract:
terrain is a tactical graph first and scenery second.

## Quick start

```bash
# a river battlefield from seed 7
python3 scripts/soi-mapgen.py --preset river_crossing --seed 7 --output my_map.json
# the same preset with two rivers and more woods
python3 scripts/soi-mapgen.py --preset river_crossing --seed 7 \
    --set water.rivers=2 --set forests.density=0.8 --output my_map.json
# keep the ground and water of a map you like, reroll everything else
python3 scripts/soi-mapgen.py --preset river_crossing --seed 8 \
    --base my_map.json --lock terrain,water --output my_map_v2.json
```

Each run writes the map and a report next to it (`my_map.report.json`) and prints
the checks. Open the map in the editor, or use the editor's **Generator** tab,
which runs the same command and previews the candidate before it replaces the
document.

A standard map takes 10-30 seconds; the road and water tools' path searches are
most of it. A seed that fails a hard check is retried with fresh stage seeds up
to four times before the run reports failure.

## How a battlefield is laid out

Before anything is placed the generator draws a tactical skeleton from the
layout seed (`scripts/mapgen/layout.py`):

- **Orientation** - which edge the player starts from.
- **Sectors** - 3 to 5 basins across the field. Each boundary between them is a
  river, a ridge line, a wood belt or open ground, chosen from the preset's
  water, ridge and forest weights and the choke intensity.
- **Routes** - one to three strategic routes from the player's edge to the
  enemy's, bending between basins. Flanking sets how far apart they run.
- **Cross links** - roads between routes (or round a single route) at sector
  centres. Every one closes a loop, so every standard map has at least one.
- **Sites** - where settlements may stand: towns on the routes at the links,
  forts just behind a boundary, camps in the basins.

The stages then build on that plan:

| Stage         | Synthesises                                                                                                                                                                                                                                                                                                                           |
| ------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `terrain`     | Mountain walls on the flanks; corridor ridges along ridge boundaries with the routes' gaps left open; defended hills (mounds, boomerangs with their pocket to the road, elbows, rings) beside the routes with 2-3 entrances facing them; pedestal hills for hilltop forts.                                                            |
| `water`       | A river guide from edge to edge (or edge to a flank tarn) on each river boundary; edge tarns that close a flank; a coast of large edge lakes; a ring moat round the enemy seat. Every lake touches the map edge or a river, as the water gate requires.                                                                               |
| `settlements` | The player's start camp, the enemy seat (a town when the preset has one) and the remaining towns, forts and camps at layout sites. Towns straddle a route; camps and forts stand beside one, clear of the planned roads.                                                                                                              |
| `roads`       | Routes edge to edge through the towns on them, cross links, extra diagonal crossings when a river needs more bridges (or a funnel onto one bridge for a hold-the-bridge map), and paired approaches to every camp, fort and spurred hill entrance. Junctions are written into the routes' guides so the road tool joins them exactly. |
| `forests`     | Belts along wood boundaries, ambush woods beside the roads, flank screens at the map edge - all clear of settlements by the clearance parameter.                                                                                                                                                                                      |
| `landmarks`   | Sanctuaries, shrines, hamlets and watch posts on the outer side of a road, away from settlements; a share are held by an enemy picket.                                                                                                                                                                                                |
| `dressing`    | Bridgeheads, a lost ford, riverbanks, hill ramps, scree under ridges, gate approaches, farmland and orchards round towns, the player's tent lines, copses in open ground.                                                                                                                                                             |
| `spawns`      | The player's commander and army formed up on the enemy side of the start camp; a commander and garrisons for the enemy.                                                                                                                                                                                                               |

Places get deterministic names from small pools per nation
(`scripts/mapgen/data/names.json`): settlements, rivers, woods, landmarks and
the map itself. Names are stored on the intent entries and used for ids.

## Presets

Presets are JSON files in `scripts/mapgen/presets/`; add one by dropping a file
there with `id`, `label`, `description`, `width`, `height` and a `parameters`
object of overrides.

| Preset                    | Character                                                                        |
| ------------------------- | -------------------------------------------------------------------------------- |
| `open_plains`             | Few obstacles, wide basins, two long routes and a single cross link.             |
| `river_crossing`          | One river splits the field; each route has its bridge to hold or force.          |
| `mountain_pass`           | Mountain walls on both flanks squeeze the routes into a pass with ridges across. |
| `coastal_battlefield`     | The sea closes one flank; a river comes down to the coast.                       |
| `forest_ambush`           | Woods crowd the roads: cavalry keeps to the routes, infantry owns the trees.     |
| `siege_town`              | A moated town with forts on its approaches and a large starting army.            |
| `fortified_frontier`      | A line of forts on ridges along the border, a town behind them.                  |
| `multi_settlement_valley` | Several towns and camps on a dense road net with farms between.                  |
| `broken_hills`            | Ridges, boomerangs and rings everywhere; every boundary is high ground.          |
| `marsh_wetland`           | Slow rivers, meres along the edges and alder woods.                              |

## Parameters

`--describe` lists every parameter with its range, default and help text; the
editor builds its controls from that list. They group as:

- **General** - biome (ground, light and weather copied from a shipped map, see
  `scripts/mapgen/data/biomes.json`) and the player's nation.
- **Tactical** - sectors, choke intensity, flanking, formation space, mirror
  layout.
- **Terrain** - relief, hills, mountains, ridges, roughness.
- **Water** - rivers, lakes, river width, crossings per river, edge-to-edge
  chance, coast, moat.
- **Roads** - main routes, cross links, side roads, road width.
- **Settlements** - towns, forts, camps, plan variety, spacing, hilltop chance,
  farmland, walled start.
- **Forests** - density, size, ambush share, settlement clearance.
- **Landmarks** - count, spacing, guarded share.
- **Dressing** - density, scatter.
- **Spawns** - starting army, garrison strength.

## Soak testing

```bash
make test-map-generation seeds=100          # seeds 1..100 across every preset
python3 scripts/soi-mapgen.py --preset all --seed 1 --count 50 --report-dir artifacts/mapgen
```

`--preset all` cycles the presets seed by seed. `summary.json` lists failures by
check id, pathological seeds (failed, retried, or slower than twice the median)
and mean metrics; every seed's map and report sit beside it, and any failure
reproduces with `--preset <id> --seed <n>`. `tests/scripts/test_mapgen.py` runs
a small fixed set in the unit tests; `SOI_MAPGEN_SOAK=1` widens it to every
preset at full size.

Generation is deterministic for a given generator version: dressing and
placement run with `--surface off`, so whether `terrain_probe` happens to be
built does not change the map.

## Contract

This section is the interface between the generator and its callers (the
editor, tests, batch runs). Change it in one place and update both sides.

### Stages

Fixed order. Every stage has a stable id.

| id            | owns (map JSON)                                                    | regenerated whenever this stage is unlocked:            |
| ------------- | ------------------------------------------------------------------ | ------------------------------------------------------- |
| `terrain`     | `terrain` (hills, shaped ridges, mountains)                        |                                                         |
| `water`       | `rivers`, `lakes`                                                  |                                                         |
| `settlements` | `settlements` intent; its `structures` output                      |                                                         |
| `roads`       | `roads`, `bridges`                                                 | `settlements`                                           |
| `forests`     | `forests`                                                          | `settlements`, `roads`                                  |
| `landmarks`   | `landmarks` intent; its `structures`/`world_props`/`spawns` output | `settlements`, `roads`                                  |
| `dressing`    | `dressing` intent; its `world_props` output                        | `terrain`, `water`, `settlements`, `roads`, `landmarks` |
| `spawns`      | generated starting armies in `spawns`                              | `settlements`                                           |

The last column is the _forced_ rule: a stage whose intent is derived from
another stage's geometry cannot stay locked while that stage is rerolled. Roads
run between settlement anchors, so unlocking settlements forces roads. A caller
asking for a forced stage to stay locked gets it regenerated anyway, and the
report names the stage that forced it (`forced_by`).

A locked stage keeps the base map's arrays for that stage **verbatim**. Its
materialiser does not re-run, and after every tool that does run the locked
arrays are put back unchanged: `generate-map-settlements.py` grows a hill to
carry a hilltop settlement and moves ramps out of a new wall ring, and neither
may touch locked terrain. When a tool tried to, the stage notes say so, and the
validation gates decide whether the candidate still holds; a candidate that
does not is retried and, failing that, reported. The settlement stage never
asks for a hilltop settlement while terrain is locked, which is the common case.

Materialiser _output_ of a locked intent stage (a locked settlement's
buildings, a locked landmark's props) is kept verbatim too, unless the ground
it stands on was rerolled: then the tool lays the same intent out again, so a
locked town keeps its tier, plan and position and realigns its gates to the new
roads. Placement correction never moves an element the run did not generate.

Every synthesiser treats everything already in the working map - locked stages
and authored elements - as an obstacle, so a rerolled hill never lands on a
locked river and a rerolled forest never grows through a locked town.

### Provenance

Every element the generator authors carries `"generated": "<stage id>"`:
terrain features, rivers, lakes, roads, bridges, forests, spawns and the
`settlements`/`landmarks`/`dressing` intent entries. Materialiser output keeps
the ownership key its tool already writes (`settlement`, `landmark`,
`dressing`) and gains the `generated` key as well.

An element **without** `generated` is authored. Rerolling a stage removes only
that stage's `generated` elements; authored elements are kept and treated as
obstacles. The runtime ignores all of these keys.

Editing a generated element in the map editor (moving it, changing its owner,
editing its JSON) converts it to authored: the edit strips `generated`,
`settlement`, `landmark` and `dressing` from that element, in the same undo
step. A later reroll then leaves it alone instead of silently replacing the
user's work.

### Map metadata

A generated map records how to reproduce itself in a root `generation` object.
Maps without it load exactly as before.

```json
"generation": {
  "version": 1,
  "generator": "soi_mapgen",
  "seed": 73948291,
  "preset": "river_crossing",
  "width": 650,
  "height": 650,
  "parameters": { "terrain.relief": 0.5, "water.rivers": 1 },
  "stage_seeds": { "terrain": 1932, "water": 88123 },
  "locks": ["terrain"]
}
```

`parameters` is the fully resolved set (preset defaults plus overrides), so the
same `version` + `seed` + `parameters` reproduces the same map byte for byte.

Stage seeds are derived, never drawn in sequence:
`stage_seed = int.from_bytes(sha256(f"{seed}:{stage}").digest()[:8], "big") & (2**53 - 1)`.
Changing one stage's parameters therefore cannot perturb another stage's
random stream. The mask keeps every seed exact in a JSON double, so the
metadata survives the editor's QJsonValue round trip. A retry derives
`sha256(f"{seed}:{stage}:attempt{n}")` the same way.

Two more keys describe how the candidate was found: `layout_seed` (the seed of
the tactical skeleton; a reroll with any lock reuses the base map's, so the
skeleton stays put while unlocked stages change) and `attempt` (0 unless the
first candidate failed a hard check).

### Command line

```bash
python3 scripts/soi-mapgen.py --describe
python3 scripts/soi-mapgen.py --preset river_crossing --seed 73948291 \
    [--width 650 --height 650] [--set water.rivers=2 ...] \
    [--request request.json] \
    [--base current.json --lock terrain,water] \
    --output candidate.json [--report report.json] [--progress]
python3 scripts/soi-mapgen.py --preset river_crossing --seed 1 --count 100 \
    --report-dir artifacts/mapgen
```

- `--describe` prints the schema below to stdout and exits 0.
- `--request` reads a request object (`{"version", "seed", "preset", "width",
"height", "parameters", "locks"}`); explicit flags override it. A map's
  `generation` object is a valid request.
- `--base` is the document to reroll; `--lock` lists the stages to keep from it.
  Without `--base` the map is generated from scratch and `--lock` is an error.
- `--progress` writes one JSON object per line to **stdout**, nothing else goes
  to stdout; tool chatter goes to stderr.
    - `{"event": "stage", "stage": "water", "status": "running"}`
    - `{"event": "stage", "stage": "water", "status": "done", "seconds": 1.4}`;
      status is one of `running`, `done`, `locked`, `failed`
    - `{"event": "done", "ok": true, "output": "<path>", "report": "<path>"}`
    - `{"event": "error", "message": "..."}` before an exit status of 2
    - Stage events also name the two closing passes, `placement` and
      `validation`, and carry `"attempt": n`; a retry starts the stages again
      from `terrain` with the next attempt number.
- Without `--report` the report is written next to the output as
  `<output>.report.json`.
- Exit status: `0` candidate written and every check passed or warned; `1`
  candidate written but a hard check failed; `2` bad arguments or no candidate
  could be produced (the report says why when it can).
- `--count N` generates seeds `seed .. seed+N-1` into `--report-dir`
  (`seed_<n>.json`, `seed_<n>.report.json`) and writes `summary.json` with the
  failure count, failures by check id, pathological seeds, generation time and
  mean metrics. Exit `1` if any seed failed.

### `--describe` schema

```json
{
    "version": 1,
    "stages": [
        { "id": "roads", "label": "Roads", "forced_by": ["settlements"] }
    ],
    "parameters": [
        {
            "key": "terrain.relief",
            "stage": "terrain",
            "label": "Relief",
            "type": "float",
            "min": 0.0,
            "max": 1.0,
            "step": 0.05,
            "default": 0.5,
            "help": "How much of the map rises into hills."
        },
        {
            "key": "water.rivers",
            "stage": "water",
            "label": "Rivers",
            "type": "int",
            "min": 0,
            "max": 3,
            "default": 1
        },
        {
            "key": "general.biome",
            "stage": "general",
            "label": "Biome",
            "type": "choice",
            "choices": [{ "id": "temperate", "label": "Temperate" }],
            "default": "temperate"
        },
        {
            "key": "tactical.symmetric",
            "stage": "tactical",
            "label": "Mirror layout",
            "type": "bool",
            "default": false
        }
    ],
    "presets": [
        {
            "id": "river_crossing",
            "label": "River Crossing",
            "description": "One river splits the field; two bridges and a ford.",
            "width": 650,
            "height": 650,
            "parameters": { "water.rivers": 1 }
        }
    ]
}
```

`stage` on a parameter is one of the stage ids, or `general` / `tactical` for
parameters that shape the whole map.

### Report

```json
{
    "version": 1,
    "ok": true,
    "request": { "seed": 1, "preset": "river_crossing", "parameters": {} },
    "stages": [
        {
            "id": "water",
            "status": "done",
            "seed": 88123,
            "seconds": 1.4,
            "forced_by": null,
            "notes": ["river 1: 649.7 -> 650.9, 36 points"]
        }
    ],
    "checks": [
        {
            "id": "roads.connected",
            "status": "pass",
            "message": "connected road graph"
        },
        {
            "id": "tactical.flank_diversity",
            "status": "warn",
            "message": "western route has low flank diversity",
            "objects": [{ "kind": "road", "index": 3, "x": 120.0, "z": 44.0 }]
        }
    ],
    "metrics": {
        "road_components": 1,
        "edge_connections": 4,
        "route_loops": 1,
        "bridges": 2,
        "settlements": 3,
        "sectors": 4,
        "dead_ends": 0,
        "formation_area_ratio": 0.41,
        "terrain_blockage": 0.12,
        "forest_blockage": 0.06,
        "min_settlement_spacing": 180.0,
        "out_of_bounds": 0,
        "seconds": 9.2
    }
}
```

Stage `status` is `done`, `locked` or `failed`. The report also lists
`attempts` - `[{"attempt": 0, "failures": ["settlements.gate"]}, ...]` - and the
metrics include `attempts`, `water_coverage`, `spawn_to_objective` and
`min_landmark_spacing`.

Check `status` is `pass`, `warn` or `fail`. A `fail` makes `ok` false and the
exit status 1. `roads.loop` fails on a standard map (both sides at least 500)
and only warns on a smaller one, following the loop rule in
`scripts/RTS_MAP_DESIGN.md`. `terrain.hill_approaches` applies the engine's own
terrain audit (`game/map/terrain_topology_audit.cpp`): the loader expands an
entrance into every cell within its radius, so two ramps only count as two
approaches when their centres are further apart than both radii plus 12% of
the hill's smaller extent. It fails for a generated hill and warns for an
authored one. `objects` is optional; `kind` is one of `terrain`, `river`,
`lake`, `road`, `bridge`, `structure`, `spawn`, `world_prop`, `forest`,
`settlement`, `landmark`, with the map coordinates of the problem so a caller
can frame it.
