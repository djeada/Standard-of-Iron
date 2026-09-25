# Choosing a Starting Base in Skirmish

Every skirmish map authors its barracks in the map JSON `structures` array. Originally, the `player_id` on each barracks determined the starting position completely: player 1 began at `p1_barracks`, player 2 at `p2_barracks`, and so on. The setup screen could change which player occupied a seat, but not which authored base that seat used.

Skirmish setup now separates those two ideas. Each player can choose any available barracks on the map as a starting base, while the game still guarantees **exactly one starting base per player**.

## What the player sees

`ui/qml/MapSelect.qml` gives every seat in the Order of Battle a **Base** chip next to Colour, Nation, Commander, and Team. Clicking the chip cycles through bases that no other seat currently claims.

One seat is always _armed_ for direct selection. The UI marks it with a lighter card, brighter border, and `▸` caret; clicking anywhere on a player card arms that seat.

The map preview in `ui/qml/MapPreview.qml` provides the spatial half of the same control:

- claimed bases are drawn in the owning player's colour;
- unclaimed bases appear as ringed dark discs;
- hovering a marker shows the base name and current owner; and
- clicking a marker assigns it to the armed seat.

If the selected base is already occupied, the two seats **swap bases**. Swapping instead of stealing preserves the one-base-per-player invariant and prevents a roster from ending up with a player who has no start.

Base selection is disabled when a map offers only one base, as on Iron Sepulcher Watch. The **Play** button also rejects any roster in which a seat has no base assignment.

## How bases are named

`MapPreviewGenerator::base_markers()` derives a display name for every starting base.

A barracks with an authored `id` that already reads like a place keeps that identity in human-readable form. For example:

```text
east_lodge_barracks → East Lodge
```

IDs such as `p<N>_barracks`, together with unnamed entries, are treated as bookkeeping rather than authored place names. Those bases receive a name derived from their **world-space bearing**, such as `South-West`, `North-East`, or `Centre`.

Bearings use world orientation, where north is `-z`, rather than the rotation of the preview image. This is deliberate. Authored place names and the battlefield itself use world north, so a location called the north toll should not become “South” simply because the minimap happens to be rotated by 225°.

When two bases resolve to the same display name, a numeric suffix disambiguates them. `BaseMarkersTest` verifies that shipped skirmish maps do not expose duplicate base names.

## The data contract

`StructureEntry::id`, parsed by `MapLoader::read_structures`, is the stable key for a base.

Maps that omit an ID fall back to `structure_<index>`. That fallback is stable for a particular version of a map file, but it changes when the `structures` array is reordered. A map whose bases are intended to be selected by stable name should therefore author explicit IDs.

Two map helpers expose the available seating information:

- `Game::Map::collect_base_options()` lists every point barracks on the map; and
- `default_base_assignments()` returns the seating authored by the map itself.

## How a base choice reaches the match

The setup screen stores the selected base as `baseKey` on each entry in the existing `player_configs` list passed to `start_skirmish`.

`SkirmishLoader::start` collects those keys and forwards them to `MapTransformer::set_base_assignments()`, alongside the team and nation overrides it already handles.

When no base assignments are present—for example, in an observed match, campaign mission, or restored save—the transformation path remains inactive and the map behaves exactly as authored.

## What reseating changes

When base assignments are present, `resolve_base_seating()` in `map_transformer.cpp` resolves them before any entity spawns. Reseating changes three things.

### 1. Ownership follows the selected base

The chosen barracks spawns under its new owner.

### 2. Each player still owns only one starting base

Any other barracks originally authored for that player revert to neutral ownership. A reseated player can therefore never begin with two bases.

The abandoned camp remains on the map as a capturable neutral prize, just like any neutral outpost placed by the map author.

### 3. The player's camp moves with the base

The player's authored units, together with any other structures they own, are translated by the vector from the original base to the selected one. A builder authored beside the starting barracks therefore remains beside the barracks after reseating.

Translated spawns still pass through the transformer's existing “nudge off forbidden ground” logic. If moving a camp places a unit in a river or other invalid location, the unit is moved to the nearest valid tile before play begins.

## Population capacity follows the original start

Neutral outposts are often authored with a much smaller `max_population` than a true starting base. Choosing one as the new start does **not** reduce the player's intended army capacity: the player carries the troop cap of the base they gave up.

The choice therefore changes strategic position without accidentally changing the starting population budget.

## Invalid selections fail safely

A `baseKey` that does not match any base on the map is a no-op for that player. The player keeps the barracks authored for them rather than being left without a valid start.

This fallback protects saved or externally supplied configurations from turning an unknown base identifier into an unplayable roster.

## Every base has room for fields

A base is only a real start if its player can feed a household from it. Food
comes from farms (see [FOOD_AND_FARMS.md](FOOD_AND_FARMS.md)), and a farm is a
13.6 m square that needs level, open ground. Every authored starting seat
therefore has room for at least **three** farm plots within 42 m of its
barracks, and every neutral base at least **two**. A neutral outpost may trade
farmland for something else - the Copper Canyons mine camps sit between the
rivers and the mesas, close to ore and short of fields - but none is left
unable to feed anyone.

`tests/map/skirmish_farmland_test.cpp` measures this with the engine's own
`assess_ground`, after loading each shipped skirmish map through
`SkirmishLoader`, so the count includes procedural scatter and every building
the match spawns.

Two map features make that room:

- **Camp floors.** A `flat` terrain feature listed after the camp's shoulder
  ridge levels the ground around the barracks (a flat painted after a hill
  erases it where they overlap). The camp may cut back its own shoulder; the
  map's other landforms are left alone.
- **`"fields": true`.** Procedural trees, boulders and ore are scattered at load
  without knowing where buildings will go, so a clearing fills up with the same
  pines as the forest around it. A flat authored with `"fields": true` keeps
  generated scatter off its level core (`build_runtime_world_props` in
  `game/map/terrain_service.cpp`), and out of forest ground and forest
  navigation cells as well, so a camp floor cut into a wood is a real clearing.
  Every camp floor carries it.

Food has a second source: sheep. Each skirmish map authors one pasture per
starting seat, and wildlife groups are dealt to authored pastures in order, so
every seat has a flock to raise near home.

## Test coverage

The behavior is covered at three levels:

- `tests/map/base_options_test.cpp` validates the option list, authored defaults, marker naming and placement, and verifies that every skirmish map contains at least as many bases as the player slots it advertises.
- `tests/map/map_transformer_test.cpp` checks seating rules in isolation, including ownership, the one-base-per-player invariant, troop-cap preservation, translated retinues, unknown keys, and assignment isolation between matches.
- `tests/map/skirmish_farmland_test.cpp` checks every base has room for fields (above).
- `tests/map/skirmish_base_choice_test.cpp` exercises the complete path through `SkirmishLoader`, including the opening camera framing the selected base.

The result is a flexible setup choice that changes where a player begins without weakening the map's ownership, population, spawn-placement, or fallback guarantees.
