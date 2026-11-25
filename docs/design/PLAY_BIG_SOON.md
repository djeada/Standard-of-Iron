# Play Big Soon

Status: decided 2026-11-24, implementation in progress (todo item D1).
Written 2026-11-24 from the playtest of the same day, the shipped data, the code, and
two headless AI duels run for this document.

The pillar: the player grows a city and an army as fast as the computer does, and a
match ends with thousands of small soldiers on the field.

## Decisions (made 2026-11-24)

1. **Target army size: yes.** About 500 men per side at 7 minutes, 1,000-1,200 at
   15 minutes. Skirmish caps rise to 2,000 men per side only if the section 7
   benchmark holds 30 fps at High on the reference RTX 5060 machine; otherwise 1,500.
2. **Camp start: yes**, as in section 6.2, on every skirmish map.
3. **Civilians walk to the barracks on their own: yes**, default on, with a per-home
   toggle.
4. **Homes:** lifetime cap raised from 3 to 6 civilians. No endless regrowth.
5. **Iron lever:** the ore trip brings 45 iron and maps start with 200 iron. Soldier
   iron prices stay as they are.
6. **Starting gold:** 500 for every seat. The maps' authored value becomes the source
   of truth (all skirmish maps set to 500, the `GameConfig::starting_gold` override
   goes), and the opening teaches the marketplace (iron for gold).
7. **Hardware floor:** 30 fps at the cap on the reference RTX 5060 at High; Low and
   Medium keep their own budgets and must not get slower than today.
8. **The AI gets every new convenience: yes.**

## 1. What the playtest showed

| Match | Player at the end | Computer at the end |
| --- | --- | --- |
| Amber Delta, Rome vs Normal CPU, lost at 7 min | 36 men trained (2 squads) | 527 men trained |
| The Two Fords, Carthage vs Rome Normal, 10.5 min | 104 men trained, 1 barracks | 561 men trained, 7 barracks |

On Amber Delta the player bought two legionary squads with the starting 60 iron and
then every recruit card said "Need 10 more Iron" for the rest of the match. The 500
starting gold was never spent.

A caution on these numbers: "trained" in the battle report
(`GlobalStatsRegistry::on_unit_spawned`) counts every non-initial troop spawn, which
includes builder crews and civilians, and a squad split creates a non-initial spawn
too (`squad_service.cpp`, `params.is_initial_spawn = false`). The AI splits builder
crews for concurrency, so its "trained" figure is inflated by an unknown amount. The
gap is still real: see the fielded-men curve in section 3.

## 2. The growth chain, measured from data and code

### 2.1 Starting position (skirmish)

| What | Value | Source |
| --- | --- | --- |
| Gold | 500 for every seat (map value ignored) | `game_config.h` `starting_gold`, `skirmish_runtime_coordinator.cpp` |
| Food / wood / stone / iron | per map; Amber Delta 260 / 240 / 100 / 60 | `assets/maps/*.json` `starting_resources` |
| Iron across skirmish maps | 60 (Amber Delta) to 150 (Copper Canyons) | same |
| Buildings | 1 barracks | map `structures` |
| Units | 1 builder crew (12 men), 1 healer | map `spawns` |
| Recruitment reserve | 140 men in the starting barracks | `max_population: 140`, `barracks.cpp` |
| Army cap | 360-900 men per map (Amber Delta 500) | `max_troops_per_player` |

### 2.2 What things cost

Recruit prices are men from the barracks reserve plus resources. Men come from the
nation file (`production.cost`, always equal to the squad size); resources come from
`assets/data/troops/base.json` unless the nation overrides them.

| Recruit (Rome / Carthage) | Men | Wood | Iron | Food | Iron per man |
| --- | --- | --- | --- | --- | --- |
| Legionary / Citizen Infantry | 18 / 16 | 10 | 30 | - | 1.67 / 1.88 |
| Triarius / Liby-Phoenician Spear | 26 / 28 | 30 | 15 | - | 0.58 / 0.54 |
| Auxiliary / Libyan Archer | 24 / 22 | 30 | 10 | - | 0.42 / 0.45 |
| Velites (Rome) | 16 | 20 | 10 | - | 0.63 |
| Balearic Slinger (Carthage) | 16 | 20 | 0 | - | 0 |
| Equites / Numidian Cavalry | 9 | 15 | 30 | 30 | 3.33 |
| Builder crew | 12 | 20 | - | - | - |
| Civilian (from a home) | home's own | - | - | 20 | - |

Buildings (`assets/data/construction/catalog.json`): home 50 wood 15 stone (10 s),
farm 40 wood 10 stone (8 s), barracks 100 wood 60 stone (10 s), marketplace 60 wood
40 stone 50 gold (10 s).

What 60 iron buys: 2 legionary squads (36 men) or 6 archer squads (144 men, but the
reserve of 140 runs out first). A new player picks the legionary, the iconic unit, and
is stuck.

### 2.3 Income

One builder trip = walk to the node, work, walk back to a **barracks** (the only
drop-off, `resource_delivery_system.cpp` `is_live_depot`). Work times are in
`catalog.json` (12 s for wood, stone and ore, 10 s for grain); yields are in
`game/systems/harvest_yields.h`: wood 80, stone 35, iron 30, grain 60, sheep 35.
(`docs/FOOD_AND_FARMS.md` still says 40 wood and 6 s; it is stale.) Builders walk at
2.0-2.3 m/s. A crew below full strength gathers at `squad_fraction^1.5`.

Per full crew, per minute, by one-way distance from node to barracks (speed 2.1 m/s):

| Distance | Wood | Stone | Iron |
| --- | --- | --- | --- |
| 30 m | 118 | 52 | 44 |
| 45 m | 87 | 38 | 33 |
| 60 m | 70 | 30 | 26 |

The nearest authored ore on Amber Delta is 42 m from the player's barracks, so one
iron crew earns about one legionary squad a minute.

Food: a farm ripens every 60 s for 60 food, so one farm feeds three civilians a
minute. Food is not the brake.

### 2.4 Manpower

Reserve is the real currency of growth, and it only grows like this
(`docs/FOOD_AND_FARMS.md`, `home.cpp`, `civilian_delivery_system.cpp`):

1. A builder raises a home (50 wood, 15 stone).
2. The player selects the home and recruits a civilian (20 food). A home gives **3
   civilians in its lifetime** (`k_max_home_civilians = 3`).
3. The civilian appears beside the home and stands there. Nothing sends it anywhere.
4. The player selects it and right-clicks a barracks. It walks over and adds **18 men**
   (`k_civilian_delivery_reserve_grant`).
5. The delivery is refused if it would overfill the barracks: 140 for the starting
   barracks, **60 for a built one** (`SpawnParams::max_population` default, lifted to
   the dearest recruit + 18 when that is higher).

So one home is worth 54 men, ever. To go from the starting 140 men of reserve to a
500-man army the player needs 20 civilians: 7 homes, 400 food, 20 recruit clicks and
20 delivery orders, spaced out so no barracks overflows. Losses have to be replaced
the same way, so a long war needs a home for every 54 men who die.

### 2.5 Builders

- One builder crew at the start, and nothing tells the player that the first move
  should be three more (60 wood, 36 men of reserve).
- Starting any construction clears the crew's Auto Gather (`begin_site_work` calls
  `clear_auto_gather`). When the building is done the crew stands idle until the
  player notices.
- A crew holds one site at a time. A site queue exists only for wall plans
  (`queued_construction_site_ids`, `wall_plan_service.cpp`).
- Recruit queues hold 5 per barracks (`production_service.cpp`), one click each. There
  is no batch or repeat order.

### 2.6 Inputs needed for a 500-man army today

A rough count for Rome on Amber Delta, from the start to 500 men in the field: 3 builder
recruits, 7 home placements, 2 farm placements, 1 marketplace, about 10 gather orders
(and re-orders after each build), 20 civilian recruits, 20 civilian deliveries, about 22
squad recruits, plus every builder that went idle. Well over 100 separate actions, while
the opponent attacks from minute 3. The AI performs all of them every decision tick.

## 3. What the AI does differently

Same starting stock, same prices, same refusals: the AI's orders go through
`ProductionService::can_start_production` and the same command validator
(`ai_command_applier.cpp`). Its only resource credits are the same harvest, market and
refund paths the player has. It wins on throughput and attention, not on cheats.

Headless duels for this document (`build/bin/ai_tests`, binary built 2026-11-24 09:52,
`SOI_DUEL_NARRATE=1`, logs in `artifacts/playtest-2026-11/D/`). Note the fixture gives
each barracks 630 men of reserve, not the 140 a skirmish barracks gets, so the first two
minutes are faster than a real skirmish. Population is men in the field, workers
included, against a cap of 560.

| Minute | Scipio (Rome) | Fabius (Rome) | Marcellus (Rome) | Hanno (Carthage) |
| --- | --- | --- | --- | --- |
| 1 | 165 (9 crews) | 165 (12 crews) | 165 (9 crews) | 158 (11 crews) |
| 2 | 282, market up | 351, market up | 276, 2nd barracks | 370, market up |
| 3 | 406 | 514 | 368 | 520 |
| 5 | 528 (cap) | 157 (losing) | 199 (losing) | 507 (cap) |
| 7 | 527 | 0 (eliminated) | 0 | 517 |
| 15 | 528, gold 4,283 | - | - | 517, gold 3,554 |
| 30 | 527, gold 10,663 | - | - | 517, gold 6,622 |

What the AI does that the player does not:

1. **Workers first.** About ten builder crews inside the first minute, many split into
   half crews for concurrency. The AI budgets 28% of its cap for workers
   (`k_worker_pop_share`, `ai_macro_targets.cpp`).
2. **Never idle.** Builder behaviour re-tasks every crew each decision; gathering is
   driven by stock thresholds (wood under 80, iron under 50).
3. **Gold into iron.** A marketplace by minute 2, then `EconomyBehavior` buys iron,
   wood and stone in lots of 10 (iron at 20 gold per 10). The starting 500 gold becomes
   about 225 iron. The player's 500 gold sat unused.
4. **Civilians recruited and delivered automatically**, to whichever barracks has room
   (`DeliverCivilians` from the AI, the same command the player has).
5. **More barracks.** Up to 6 desired plus outposts (`ai_strategy.cpp`), so more queues
   run in parallel and more reserve capacity exists. 7 on The Two Fords.
6. **It hits the cap and stops.** From minute 3-5 the surviving AI sits at the cap for
   the rest of the match and its gold piles up with nothing to buy. The cap, not the
   economy, is the AI's limit. The game is "big" at about 550 men and then stops
   growing.

## 4. Diagnosis, ranked

1. **Attention, not economy.** At today's income rates, three to four builder crews
   that never idle, civilians that deliver themselves and one marketplace are enough
   for 500-600 men by about minute 7 (arithmetic in section 6.9, which adds the camp
   start). The AI proves it every match. The player loses because every step of the
   chain is a separate manual order, and the most frequent ones (civilian recruit and
   delivery, re-tasking idle crews) are the least visible.
2. **The opening is a trap.** One builder, 60 iron, 500 gold that only a marketplace
   can spend, and an iconic unit that costs half the iron.
3. **Manpower is lumpy and capped per building.** 54 men per home, ever; built
   barracks refuse deliveries above 60 reserve.
4. **The ceiling is low.** Map caps of 360-900 men mean even a perfect player tops out
   at a few dozen squads, and the AI reaches its cap by minute 5.
5. **Nothing grows visibly.** The town is a handful of buildings; the army is the only
   thing that grows, so the player cannot see their economy working.

## 5. Target growth curve

Men in the field per side (workers included), 1v1 on a 500-900 cap map at Normal, for a
competent player who is not an expert. The Normal AI should land within 15% of the same
line; it already does until the cap.

| Time | Target | Today, player | Today, AI (skirmish) |
| --- | --- | --- | --- |
| 0:00 | 70 (camp start) | 13 (1 crew, 1 healer) | 13 |
| 3:00 | 250-300 | ~50 (estimate) | 400-520 in the duel fixture (630 reserve) |
| 7:00 | 500-600 | 36 trained | 527 trained |
| 15:00 | 1,000-1,200 | - | at cap (500-900) |
| 25:00 | 1,800-2,000 (new cap) | - | - |

Pass condition, checked headlessly (section 9): a scripted "player proxy" that issues
at most 30 orders a minute reaches at least 80% of the AI's men at 7 and 15 minutes on
the same seat.

## 6. Proposal

Each item lists what changes, the numbers, and whether it is **data** (JSON only),
**constant** (a one-line C++ constant), or **code**.

### 6.1 Civilians report to the barracks on their own (code, S-M)

- A civilian born in a home walks to the nearest friendly barracks with room for 18
  men and is delivered there. If none has room it waits at the home and tries again
  every 5 s. Reuse `DeliverCivilians` and `CivilianDeliveryComponent`; the choice of
  barracks is the one the AI already makes.
- A home with food available recruits its next civilian by itself (one at a time, its
  lifetime cap still applies). A per-home "hold" toggle stops it, for players who want
  to save food.
- Floating "+18" over the barracks stays (essential feedback).
- Impact: removes about 40 of the 100+ actions in section 2.6. Biggest single win.

### 6.2 Camp start (data, S)

Every seat on every skirmish map gets, authored in the map JSON (`structures`,
`spawns`; the base-reseating translation in `map_transformer.cpp` already moves a
retinue with its base):

| Item | Today | Proposed |
| --- | --- | --- |
| Barracks reserve | 140 | 200 |
| Homes | 0 | 2 (6 civilians = 108 men waiting) |
| Farms | 0 | 1 |
| Builder crews | 1 | 3 |
| Fighting squads | 0 | 2 (Rome: legionaries + velites; Carthage: citizen infantry + slingers) |
| Wood / stone / iron / food | 240 / 100 / 60 / 260 (Amber Delta) | 300 / 150 / 200 / 300 on every map |

Men on the field at 0:00: 36 (crews) + 34 (Rome's two squads) + healer and
commander, about 70, with 108 more waiting in the two homes. The AI gets the same start. Campaign missions keep their own
authored starts.

### 6.3 Iron (constant + data, S)

- `k_collect_iron_ore_reward` 30 -> 45. One crew at 42 m then earns 50 iron a minute,
  enough for 1.7 legionary squads a minute.
- Starting iron 200 on every skirmish map (part of 6.2).
- Make gold reachable: the recruit refusal "Need 10 more Iron" gains a second line when
  the player has gold and no marketplace: "A marketplace sells iron for gold." When a
  marketplace stands, the recruit card offers "Buy the missing iron (40 gold)". Code,
  S; text is new (`qsTr`).

### 6.4 Builders go back to work (code, S-M)

- When a crew finishes or abandons a construction, it resumes the gather job or Auto
  Gather it had before (store it on `BuilderProductionComponent` before
  `begin_site_work` clears it).
- A crew with no job for 10 s starts Auto Gather on the resource the player is
  shortest of. One global setting, default on; any player order cancels it.
- Idle-builder hotkey and counter are already on the todo (S1).

### 6.5 Batch and repeat recruiting (code, S)

- Shift-click a recruit card = queue 5. Ctrl-click = repeat: keep recruiting this type
  whenever the barracks queue has room and the resources are there; click again to stop.
- Queue cap per barracks 5 -> 10 (`production_service.cpp` `max_queue_size`).
- Refusals stay explicit and name the missing thing, once per batch, not five times.

### 6.6 Several sites per crew (code, M)

Shift-placing buildings queues them on the selected crews, in order, reusing
`queued_construction_site_ids` and the pending-site ground reservations walls already
use. Several crews selected share the queue (shared-site pooling exists). S1 owns the
"build menu greys out" UI item; this is the simulation half.

### 6.7 Manpower that keeps up (constant, S)

- `k_max_home_civilians` 3 -> 6 (Decision 4): one home is then 108 men.
- Built barracks reserve ceiling 60 -> 120 (`SpawnParams::max_population` default), so
  a delivery is almost never refused.
- Starting barracks 140 -> 200 (data, part of 6.2).

### 6.8 Forward depots (code, M)

A cheap storehouse (40 wood, 6 s) that accepts deliveries like a barracks yard. Halves
trip time at a distant ore field. This is the generic version of Rome's marching camp
in `NATION_ASYMMETRY.md`; build that one instead if the asymmetry plan is approved.

### 6.9 Why these are enough (arithmetic)

Rome, camp start, items 6.1-6.7, 1v1 Amber Delta. To field 600 men by 7:00 (a mixed
army costs about 1.0 iron and 1.0 wood per man):

- Men: 200 reserve + 108 from the starting homes + 300 from 3 more homes (6
  civilians each at 6.7) = 608. Needs 150 wood, 45 stone, 360 food.
- Iron: 600 - 200 start - 225 bought with the 500 gold = 175 to mine = 4 trips, one
  crew for 5 minutes.
- Wood: about 600 for troops + 400 for buildings - 300 start = 700 = 9 trips, two
  crews.
- Food: 360 - 300 start = 60, one farm.
- Construction: 3 homes, 1 farm, 1 marketplace, 1 barracks = about 60 crew-seconds.

Three to four crews are enough; the camp start gives three. The player's work becomes
placing buildings and choosing what to recruit, which is the fun part.

### 6.10 The city grows where you can see it (code + art, L; F1)

The economy should show on the ground, not only in the top bar:

- **Homes upgrade in place** with town size: hut -> house -> courtyard house / insula
  (Rome's `tenement_*` renderers exist) at 4 / 10 / 20 homes. Presentation only.
- **Worn paths.** A traffic map of builder and civilian trips (sampled per second,
  decayed slowly) drives a dirt-path decal between the barracks, homes, fields and
  resource nodes. Busy routes become a gravel road after a few minutes. No gameplay
  effect unless Rome's roads are approved.
- **Fields spread.** Each farm draws a ring of kitchen gardens, fences and haystacks
  that thickens with its harvest count (`FarmComponent::harvests`).
- **More people.** Settlement life (`settlement_life_system.cpp`) gets one ambient
  townsperson per home, capped for performance.
- **The army camp grows.** Tents and weapon racks appear around a barracks in
  proportion to its fielded squads.

The user's city-film notes apply: no dead-end or dirt roads in finished towns, living
places, nation-correct materials (`CITY_VISUAL_DIRECTION.md`).

### 6.11 A sink for late gold (code, M)

Both surviving AIs ended with 3,500-10,600 unused gold. With higher caps, gold needs a
use: for Carthage it is mercenaries (`NATION_ASYMMETRY.md`); for Rome, buying iron and
levies. Without this, raising caps only lengthens the stall.

### 6.12 Teach the opening (text, S)

The skirmish economy coach (`EconomyCoach.qml`) gets an opening line: "Train three
builders, then build homes. Civilians become soldiers." New strings go through
`qsTr` and the orchestrator's translation pass.

## 7. Late-game scale and the performance budget

Measured facts (`docs/PERFORMANCE.md`, November 2026; `sim-tick` profile):

- The simulation runs at 60 Hz: 16.7 ms a tick.
- Cost scales with **squads, not men**: a squad is one entity and its soldiers are not
  collision checked. `sim_benchmark` (all units in melee, the worst case): 500 squads
  6.8 ms a tick, 2,000 squads 28.7 ms. About 14 µs per squad in melee.
- In melee 66-81% of the tick is `CombatSystem` formation contact geometry, an
  attacker-slot x target-slot loop. Doubling a squad's men roughly quadruples its
  contact cost, so bigger squads are not a free way to more men.
- Every rendered sample was GPU-bound. Shadow casters are capped per cascade (60-240 by
  preset). High and Ultra skip unit LOD on purpose.

What the targets cost:

| Scale | Squads (about 20 men each) | Sim tick, all in melee | Verdict |
| --- | --- | --- | --- |
| 1v1, 2,000 men each | ~200 | ~3 ms | Fits. |
| 4 players, 2,000 each | ~400 | ~5.5 ms | Fits. |
| 1v1, 5,000 each | ~500 | ~7 ms | Fits the sim; GPU unmeasured. |
| 4 players, 5,000 each | ~1,000 | ~14 ms | At the budget; not proposed. |

Proposed caps: 2,000 men per side on 1v1 and 2-player maps, 1,500 on 4-6 player maps.
Before shipping the caps, two measurements, both quick:

1. `sim_benchmark --units 400 --ticks 600` and `--units 1000` on a quiet box.
2. A game benchmark (`--benchmark-output`) on Amber Delta, observe mode, four AIs at
   a 1,500 cap, at the minute the first big fight happens, on High at 1080p; read
   `gpu_color_ms`, the shadow pass and `SOI_GPU_BREAKDOWN`. If the rigged-creature
   line exceeds about 6 ms, the cap waits for GPU work, not LOD cuts (the
   perceptually-identical rule stands).

## 8. Ranked changes

| # | Change | Impact | Effort | Kind | Owner |
| --- | --- | --- | --- | --- | --- |
| 1 | Civilians auto-deliver, homes auto-recruit (6.1) | Very high | S-M | code | S5 |
| 2 | Camp start on every skirmish map (6.2) | High | S | data | S5 |
| 3 | Ore 45 per trip, iron 200 at start, iron-for-gold hint (6.3) | High | S | constant + data + small code | S5 |
| 4 | Builders resume work, idle crews auto-gather (6.4) | High | S-M | code | S1 |
| 5 | Shift = 5, Ctrl = repeat, queue 10 (6.5) | High | S | code | S1 |
| 6 | Home lifetime 6, built barracks reserve 120 (6.7) | Medium | S | constant | S5 |
| 7 | Teach the opening (6.12) | Medium | S | text | S1 |
| 8 | Map caps to 1,500-2,000 after the two measurements (7) | High | S + measurement | data | O |
| 9 | Several sites per crew (6.6) | Medium | M | code | S1 |
| 10 | Late gold sink (6.11) | Medium (high once caps rise) | M | code | with D2 |
| 11 | Forward depot (6.8) | Medium | M | code | with D2 |
| 12 | Visible city growth (6.10) | High for feel | L | code + art | F1 |
| 13 | Fix "trained" to count fighting men only, not crews, civilians or splits | Low (honest reporting) | S | code | S3 (B5) |

Order of work: 1-7 together as one tuning pass (none of them needs another), then
measure (section 9), then 8, then 9-12.

## 9. How to verify

1. **Player-proxy test (new, M).** A headless test on the `AiSkirmishOpeningTest`
   fixture, seated with a skirmish barracks (140 or 200 reserve, not the 630 the duel
   fixture uses). One side is the AI; the other runs a scripted opening through the
   same `Command` API the HUD uses, limited to 30 orders a minute: 2 builder recruits,
   Auto Gather, homes, a farm, a marketplace, Shift-batches of the nation's line
   infantry. It prints men in the field per minute for both. Pass: proxy at least 80%
   of the AI at 7 and 15 minutes.
2. **AI duel narration** (`SOI_DUEL_NARRATE=1`): the AI reaches the new cap later than
   minute 5 only if the economy really is the limit; gold below 1,000 at minute 20
   once the gold sink exists.
3. **Hand playtest** of Amber Delta and The Two Fords at Normal, filmed: player men at
   3 / 7 / 15 minutes against the target table.
4. **Perf**: the two measurements in section 7 before caps rise.

**Measured after changes 1, 2, 3 and 6 (2026-11-25).** The proxy is
`tests/headless/player_proxy_growth_test.cpp` (ai_tests): a 160 m fixture, both seats
Roman, same team, skirmish barracks of 140 reserve plus the camp start, at most 36
orders a minute, cap 900. It asserts 80% at minute 7 and passes;
`SOI_PROXY_NARRATE=1` prints the full curve.

| Minute | Computer men | Proxy men (fighting) |
| --- | --- | --- |
| 1 | 234 | 232 (170) |
| 3 | 430 | 502 (442) |
| 7 | 833 | 718 (658) |
| 15 | 900 (cap) | 770 (710) |

The playtest player had 36 men at 7 minutes before these changes. The proxy stops at
about 770 because its script never builds past a second barracks and burns its gold
on iron by minute 4; the computer reaches the cap at minute 9.

Existing tests touched by these numbers: `HomeManpowerSystemTest` (cost equals squad
men), `CompiledDefaultsMatchTheShippedTroopData` and `TroopCatalogLoaderTest` (base.json
mirrors), `skirmish_farmland_test` (room for fields; pre-built farms count against it),
`skirmish_starting_strength_test` (new starting squads must take the field at full
strength), `AiSkirmishOpeningTest`, `AiDuelMatchTest`, campaign playthrough floors.
Update them with the data, never to hide a regression.

## 10. Risks

- **Faster for both sides.** Economy constants (ore yield, home lifetime) help the AI
  as well; it is cap-bound after minute 5, so the main effect is the player reaching
  the cap sooner. Raising caps grows the AI too; that is the intended "big" game, and
  it is Decision 1.
- **Early rush.** A camp start with two squads and a commander makes a minute-1 rush
  possible. The AI's attack wave needs 60% of its size assembled; watch the first
  three minutes in the playtest.
- **Automation hides the economy.** Auto-delivery and auto-gather must stay visible:
  walking civilians, floating numbers, a worker counter in the HUD.
- **Campaign balance.** Constants in 6.3 and 6.7 also change campaign missions. The
  campaign playthrough harness floors will move; adjust the floors, do not revert.
