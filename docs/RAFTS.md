# River Rafts

A raft is a fixed crossing on a river: a log raft on a guide rope stretched between the two banks. It carries one unit at a time from one bank to the other. The Rhône crossing (218 BC) is the first battle of _The Barcid Road_ campaign, and the rafts are what let Hannibal's army cross a river with no bridge in reach.

## Using a raft

- **Calling it.** Select troops within `k_raft_call_radius` (16 m) of either landing of a raft, and **Cross by Raft** appears in the action menu. Press it, and each selected unit walks to the nearer landing and joins that raft's line.
- **One unit per trip.** The raft takes the unit at the head of the line. It boards (`k_raft_board_seconds`, 1.4 s), is poled across at the raft's `speed`, and steps off at the far landing, then walks a few metres inland to clear the landing. The next unit waits on the bank until the raft is back.
- **Fetching.** If the head of the line is on the other bank, the raft crosses empty to collect it. One raft serves both banks and both sides of the war. Whoever is first in line goes first.
- **Leaving the line.** A unit given any other order before it boards leaves the line, and so does one that drifts far from the landing. A unit that does not reach the landing within `k_raft_approach_timeout` (40 s) of the raft waiting for it loses its turn.
- **Aboard.** Once aboard, a unit ignores orders until it lands. Its soldiers stand packed on the deck. While the raft is in mid-river, every hit on the unit is multiplied by `k_afloat_exposure_multiplier` (×1.35), because men on a raft cannot run or close ranks. Archers aboard can still shoot back.
- **Who can ride.** Infantry, cavalry, healers, builders and commanders can ride. Siege engines and elephants cannot. Elephants need the larger earth-covered rafts that issue #1524 describes, which are not built yet.

## Authoring

Rafts sit beside `rockfall_traps` in the map JSON:

```json
"rafts": [
    {"id": "rhone_raft_south", "position": [240.96, 150], "speed": 1.6}
]
```

| Field      | Default    | Meaning                                                                                          |
| ---------- | ---------- | ------------------------------------------------------------------------------------------------ |
| `id`       | `raft_<n>` | Name used in saves. Ids must be unique on a map.                                                 |
| `position` | required   | A point on the river. Takes `[x, z]`, `[x, y, z]` or `{"x", "z"}`; plain `x`/`z` keys also work. |
| `speed`    | 1.6        | Metres per second across the water (0.3–6).                                                      |

Coordinates follow the map's `coord_system`, exactly like rockfall traps.

When the map loads, the raft system checks every raft and skips the ones that break a rule, with a warning in the log:

1. **On a river.** The point has to lie inside the drawn water of a river segment. A raft on dry land, a lake or a pond is dropped.
2. **Wide enough.** The river's authored `width` has to be at least `k_raft_min_river_width` (6 m). A raft is a slow, exposed crossing for rivers too wide to ford. A narrower stream does not need one and has too little water to float the deck.
3. **Dry landings.** Each bank needs walkable ground within 14 m of the water's edge, measured square to the river, with room for a unit to form up. A raft between a cliff and a wall is dropped.

The crossing always runs square to the river at the raft's position. The raft docks with its near edge over the bank. `EveryRaftOnAShippedMapFindsItsRiver` in `simulation_tests` loads every shipped map and fails if any authored raft is dropped.

## The Rhône crossing

`map_crossing_rhone.json` is built around the rafts. The Rhône is 24 m wide, and its eastern branch, which runs east from the confluence at z≈240, is 18 m wide. Together they cut the far bank into two halves:

- **North of the branch: the hill fort.** Three Rhône bridges (z≈107, 190, 224) still lead there, so the army can march to the first camp.
- **South of the branch: the river town.** No bridge reaches it. The Rhône bridges south of the confluence and both branch bridges are gone. The town's own moat keeps its two bridges, but they only lead from the town's bank into the town.

Three rafts are the only ways to the river town. One is on the Rhône below the Gaulish landing (the `raft_camp` dressing), one is further south on the Rhône, and one crosses the branch from the hill-fort side. The victory condition needs both barracks, so the player has to ferry an army across. The AI does not use rafts, so the river town's garrison stays on its island too.

`RhoneRiverTownCanOnlyBeReachedByRaft` in `simulation_tests` floods the engine's own nav grid from Hannibal's camp and checks three things: the hill fort can be reached on foot, the river town cannot, and every raft links ground the army can stand on to the river town's bank.

### Regenerating the roads

`scripts/generate-map-roads.py` keeps a map's road network in one piece, and it used to add bridges to do that. Now it reads `rafts`: banks that a raft joins count as connected, so it does not build a bridge across a ferried river. If you remove a bridge, first split any road that crossed it at the water, then regenerate. After that, run `scripts/fix-map-prop-overlaps.py --surface require` to move anything the wider water now covers.

## Implementation notes

- `RaftSystem` (`game/systems/raft_system.*`) runs at the end of the Movement phase, after body contact and avoidance. It writes the passenger's transform every tick, so nothing that ran earlier in the tick can push the passenger off the deck.
- A passenger carries `RaftRiderComponent`. Three places read it. The movement gate returns `OnRaft`, so route following and the motor leave the unit alone, and so does `unstick_body`, which would otherwise shove a unit standing over water back to the bank. Terrain alignment lifts the transform to the deck instead of the river bed. The formation presentation pins each soldier to the deck and gives them the deck's elevation above the bed, the same way wall walkers stand on a wall.
- While aboard, the unit's `formation_files_override` is set so that its block fits the deck's width. The previous value is restored when the unit lands.
- The rafts, their lines and their passengers are saved through the `rafts` session-snapshot contributor. `RaftRiderComponent` itself is rebuilt from that state on the next tick.
- The AI does not use rafts yet.
