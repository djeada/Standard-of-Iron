# The Second Punic War — mission roster

What every campaign mission is _for_: what it asks the player to do, what it
teaches, and whether it is won by attacking or by holding.

Deliberately not a spec sheet. Troop counts, wave timings, starting purses and
enemy strengths live in `assets/campaigns/second_punic_war.json`,
`assets/missions/*.json` and the maps they point at, and they move whenever
anyone tunes a mission. A second copy here would only ever be a copy that has
drifted. Read the JSON for numbers; read this for intent.

`docs/MISSION_FRAMEWORK.md` describes the _schema_ — what a field means and what
values it accepts. Each mission's own `teaching_goal` is the authoritative
statement of its lesson; the table below is the one-line version.

`tests/map/campaign_roster_test.cpp` re-reads the campaign and fails if this page
lists the wrong missions, in the wrong order, in the wrong region, with the wrong
kind of goal, or claims a starting base the map does not give. It finds the
at-a-glance columns by header name rather than by position, so rewording the table
or adding a column to it does not break the build — only those four claims are
pinned, and none of them is a count.

## At a glance

| #   | Mission                  | Region           | Player base | Goal      | What it asks of you                                               |
| --- | ------------------------ | ---------------- | ----------- | --------- | ----------------------------------------------------------------- |
| 1   | Crossing the Rhône       | transalpine_gaul | none        | Offensive | Take ground you cannot afford to level, with what you landed with |
| 2   | Crossing the Alps        | alps             | barracks    | Economic  | Win on the length of the haul, not the fight at the far end       |
| 3   | Battle of Ticino         | cisalpine_gaul   | barracks    | Offensive | Spend minutes on high ground instead of on more men               |
| 4   | Battle of Trebia         | cisalpine_gaul   | barracks    | Defensive | Choose where the assault goes by choosing what it can see         |
| 5   | Battle of Lake Trasimene | etruria          | barracks    | Offensive | Behead the army instead of beating it, against a hard clock       |
| 6   | Battle of Cannae         | apulia           | barracks    | Offensive | Run the ridge, the bait and the beheading at once                 |
| 7   | The Campanian Vigil      | campania         | barracks    | Defensive | Defend supply, and notice theirs is raidable too                  |
| 8   | Battle of Zama           | carthage_core    | barracks    | Both      | Fight something the campaign's one reliable rule cannot kill      |

**Mission 1 is the only one without a player barracks**, and that is load
bearing: the player lands with a handful of soldiers, a stretch of palisade and
no production, so the only way to win is to stand in two Roman camps rather than
grind them down. It is also the shortest mission and the only one whose pressure
is a single early wave. Those facts hold each other up — change one and the
mission stops working.

## Goals, and what the column means

- **Offensive** — the win condition takes ground: capture or control structures.
- **Defensive** — the win condition holds ground: survive waves or a duration.
- **Economic** — the win condition is the stockpile. Only _Crossing the Alps_.
- **Both** — takes and holds. Only _Battle of Zama_.

The column is derived, not decorative: `TheGoalColumnAgreesWithTheVictoryConditions`
reads each mission's `victory_conditions` and fails the build if this page
disagrees with them.

Every mission runs `victory_mode: "all"` and every mission carries
`eliminate_commanders`. A nation dies with the man who leads it: on a commander's
death its barracks fall to the neutral owner, its other works come down and its
troops leave the field. That makes decapitation the shortest route to a capture
objective rather than a way around one — a neutral camp is still a camp you have
to walk into. `MissionAssetRulesTest.DecapitationObjectivesHaveCommandersToKill`
fails the build if any AI force ships without a commander spawn in its map, because
the rule only arms once an enemy commander has been seen and an unarmed rule would
make the mission unwinnable. Commanders are authored in the map's `spawns[]`; see
[MISSION_FRAMEWORK.md](MISSION_FRAMEWORK.md).

_Trasimene is the only mission that can be lost to the clock._ Every other timer
is an optional objective or a wave schedule.

## How pressure arrives

Two schedules ship, and the choice is a design decision rather than a number:

- **Timed** — waves land on a wall clock. Six of the eight missions run nothing
  else. The player can learn the schedule and plan against it.
- **Phased** (`after_previous_cleared`) — a wave arrives once the player has
  broken the one before, after a short breather. Only _Trebia_ and _The Campanian
  Vigil_ use it, mixed in among timed waves, and they are also the only two
  `survive_waves` missions. The survive-the-phases objective and the phase-driven
  schedule are the same design: pressure paced by the player's own progress, so
  neither mission can be outrun.

## Economy

Only _Crossing the Alps_ makes the economy the win condition. Only _The Campanian
Vigil_ hands the player a developed settlement to run — a marketplace and homes,
which are also what the Romans are marching at. Everywhere else the purse is
enough to reinforce and not enough to rebuild, and mission 1 has no production at
all.

## Ground worth walking to

Every mission ships at least one dead zone, and every dead zone pays a
`clear_reward` when its garrison breaks — spendable resources, never counted as
harvested, so a hoard funds the next push without shortcutting the Alps gather
objective. See [IRON_SEPULCHER.md](IRON_SEPULCHER.md).

Every standalone sanctuary has one or two wardens on it. Two men do not make a
battle; they make the shrine cost something, which is the point.

Every map authors its wildlife rather than inheriting the derived default: a
sheep pasture somewhere worth the detour, a wolf range denned in a forest, and —
from the Alps onward — wolf packs on a schedule. Wolves are forest-passable and
cavalry is not, so a pack in a forest is a threat a mounted column cannot chase.
See [AMBIENT_WILDLIFE.md](AMBIENT_WILDLIFE.md) and
[RTS_MAP_DESIGN.md](../scripts/RTS_MAP_DESIGN.md).

## Difficulty

The campaign's `difficulty_modifier` rises monotonically from the Rhône to Zama.
The star rating the campaign list draws is derived from it:
`ceil((modifier - 1.0) / 0.15)`, clamped to 1–5. Tune the modifier; the stars
follow.

## Notes for anyone tuning this

- **Region ids must exist in the war table.** `world_region_id` drives the
  objective pin and the camera framing in `MediterraneanMapPanel.qml`. A region
  that is not in `mission_regions` there gets no pin.
- **Every mission ships three or more defeat conditions.** `lose_structure` is
  present on all but the first, which has no key structure to lose.
- **The player is Carthage in every mission**; the campaign runs Carthage against
  Rome throughout.
- If you change what a mission is _for_, change its `teaching_goal` and this
  page's last column together. If you only change how much of something it
  ships, neither needs touching — that is the point of keeping the numbers out.
