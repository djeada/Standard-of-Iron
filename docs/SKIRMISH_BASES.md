# Choosing a starting base

Every skirmish map authors its barracks in the map JSON's `structures` array.
Historically the `player_id` on each entry was the whole story: player 1 always
began at `p1_barracks`, and the only choice the setup screen offered was which
authored slot you sat in. The skirmish screen now lets each player pick which
of the map's barracks they start from — always exactly one apiece.

## What the player sees

`ui/qml/MapSelect.qml` gives every seat in the Order of Battle a **Base** chip
next to Colour, Nation, Commander and Team. Clicking the chip cycles to the next
base nobody has claimed. The seat that is currently _armed_ is shown with a
lighter card, a brighter border and a `▸` caret; clicking anywhere on a card
arms it.

The map preview (`ui/qml/MapPreview.qml`) is the other half of the control.
Claimed bases are drawn in their owner's colour; unclaimed ones as a ringed
dark disc. Hovering a marker names it and says who holds it; clicking one gives
it to the armed seat. If that base was already held, the two seats **swap**, so
the roster never ends up with a player who has no start.

Both paths are off when a map offers only one base (Iron Sepulcher Watch), and
the Play button refuses a roster where a seat has no base.

## Naming

`MapPreviewGenerator::base_markers()` names each base:

- A barracks with an authored `id` that reads as a place keeps it, humanised —
  `east_lodge_barracks` becomes "East Lodge".
- `p<N>_barracks` and unnamed entries are bookkeeping, not names, so they get a
  **world-space bearing** instead: "South-West", "North-East", "Centre".

Bearings are computed against the world (north is `-z`), not against the
rotated preview image. That is deliberate: the map's own authored place names
use world north, and so does the battlefield the player ends up looking at, so
a base the author called the _north_ toll is never labelled "South" because the
minimap happens to be rotated 225°.

Duplicate names are suffixed with a number. `BaseMarkersTest` asserts no
shipped skirmish map produces two bases under one name.

## The data contract

`StructureEntry::id` (parsed by `MapLoader::read_structures`) is the stable key
for a base. Maps that omit it fall back to `structure_<index>`, which is stable
for a given map file but moves if the `structures` array is edited — so a map
whose bases should be pickable by name ought to author ids.

`Game::Map::collect_base_options()` lists every point barracks on a map;
`default_base_assignments()` reports the seating the map itself authored.

## What a reseat actually does

The choice travels as `baseKey` on each entry of the `player_configs` list that
the setup screen already passes to `start_skirmish`. `SkirmishLoader::start`
collects them into `MapTransformer::set_base_assignments()`, alongside the team
and nation overrides it already gathers. When that map is empty — an observed
match, a campaign mission, a save being restored — nothing changes and the map
plays exactly as it always did.

When it is not empty, `resolve_base_seating()` in `map_transformer.cpp` works
out three things before a single entity spawns:

1. **Ownership.** The chosen structure is spawned under its new owner.
2. **One base each.** Every _other_ barracks that player was authored to hold
   reverts to neutral, so a reseated player never fields two starts. The camp
   they walked away from stays on the map as a capturable prize, exactly like
   the neutral outposts the author placed.
3. **The camp follows.** The player's authored units — and any other structure
   they own — are translated by the vector from their authored base to the one
   they chose, so a builder authored beside the barracks still stands beside it.
   Spawns keep the transformer's existing "nudge off forbidden ground" pass, so
   a translated unit that lands in a river is walked to the nearest free tile.

A neutral outpost is usually authored with a far smaller `max_population` than
a start base. Taking one as your start therefore also takes the troop cap of
the base you gave up, so the choice costs you position, not army size.

A `baseKey` naming a base the map does not have is a no-op for that player:
they keep the barracks the author gave them rather than being left with none.

## Where the tests are

- `tests/map/base_options_test.cpp` — the option list, the authored default
  seating, marker naming and placement, and a sweep asserting every skirmish
  map has at least as many bases as the player slots it advertises.
- `tests/map/map_transformer_test.cpp` — the seating rules in isolation:
  ownership, the one-base-each rule, the troop cap, the moved retinue, an
  unknown key, and that assignments do not leak into the next match.
- `tests/map/skirmish_base_choice_test.cpp` — the whole load path through
  `SkirmishLoader`, including the opening camera framing the chosen base.
