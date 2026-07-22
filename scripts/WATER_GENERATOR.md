# Campaign water generator

`generate-map-water.py` turns authored river sketches into deterministic,
terrain-safe water networks. It preserves confluences, extends loose ends to a
map edge or lake, routes around hills and mountains, and adds broad bends at
map scale. Rivers receive a strong soft cost near roads so the two networks do
not unrealistically share the same corridor; intentional crossings remain
available at bridge decks. Existing lakes are retained; `--strategic-lakes`
may add one only on campaign maps with an explicit geographic policy.

Run water generation before road generation because rivers determine bridge
locations:

```bash
# Audit the currently authored water
python3 scripts/generate-map-water.py --campaign --validate-only

# Preview generated water without changing JSON
python3 scripts/generate-map-water.py --campaign --strategic-lakes

# Write water, then rebuild roads and perpendicular bridge crossings
python3 scripts/generate-map-water.py --campaign --strategic-lakes --write
python3 scripts/generate-map-roads.py --campaign --write
```

Validation rejects river centerlines that touch protected hills or mountains,
river ends that terminate in the middle of nowhere or below a lake surface,
self-crossing channels, and lakes that overlap protected terrain or have no
tactical/network purpose. A river-to-lake connection passes only when its
centerline terminates in the same irregular shoreline band used by the game;
ending anywhere in the lake interior is an error.
A tributary similarly terminates at the receiving river's visible bank rather
than continuing to its centerline. Validation rejects embedded confluences,
and generation truncates accidental through-river loops at the first wider
channel they encounter.
