# Nation Asymmetry: Rome and Carthage

Status: decided 2026-11-24, implementation in progress (todo item D2).
Written 2026-11-24. Companion to `PLAY_BIG_SOON.md`, which fixes growth for both
nations; this document makes the two nations grow, fight and win differently.

The pillar: Rome and Carthage play differently, not ±10% apart.

## Decisions (made 2026-11-24)

1. **Core split: yes.** Rome turns men into an army; Carthage turns gold into an army.
2. **Mercenary upkeep:** yes, gold every minute. Unpaid bands lose morale and fight
   worse; they do not desert. Desertion is too punishing for a first version.
3. **Roads:** later, in phase 4. Not part of the first asymmetric release.
4. **Harbour:** Carthage's trade building is the Emporion on every map; the Harbour
   variant comes later, only on coastal maps.
5. **Roster cuts: yes.** Rome loses the horse archer; Carthage's horse archer becomes
   Numidian javelin riders.
6. **The Iron Sepulcher:** yes as proposed. Rome purifies shrines better; Carthage
   can hire the dead for gold at a captured shrine.
7. **Commander traits: yes**, one economic or strategic trait each on top of the aura.
8. **Balance bar: yes.** Either nation wins 45-55% of mirror-free Normal 1v1s across
   the nine commander pairings.

### Implementation decisions (stream N, 2026-11-24)

Details settled while building phases 0-3, in the spirit of the decisions above.

- **Data lives in the nation files.** Each troop entry may say `recruitable`,
  `recruited_at`, `max_squads`, `requires_building`, `fabrica_discount`; a nation
  has `economy` (civilian grant, levy, loss refund, trade income, Fabrica
  multiplier, castra healing, veterancy), `unique_buildings` and
  `mercenary_bands`. One price rule (`nation_recruitment.h`, `quote_recruit`)
  serves players, the AI and the HUD.
- **Roster cuts keep the troops.** A cut troop is `recruitable: false`, so missions
  and map retinues still field it; no building recruits it. Rome's horse archer
  and horse spearman are cut; equites take the horse spear's stats, capped at 4.
- **Domus.** The 6-civilian home is now everyone's baseline (PLAY_BIG_SOON
  decision 4), so Rome's edge is the grant: a Roman civilian is 18 men, a
  Carthaginian 12.
- **Bands are (troop, nation) pairs.** Six bands: Gallic warband, Iberian
  scutarii (Iberian swordsmen), Balearic slingers, Numidian horse (the javelin
  riders), Iberian cavalry, Gallic cavalry. A recruit card names a band as
  `nation/troop`. A camp holds one contract at a time. Carthage's barracks no
  longer offers slingers or any horse.
- **Upkeep.** Paid in full each minute or not at all. Unpaid: -20 morale once and
  80% damage until the next full payday. No desertion (decision 2).
- **Emporion** is Carthage's marketplace, not a new building: +30 gold a minute,
  +5 per own farm within 40 m, at most three count; sells at 1.5x.
- **Fabrica** counts while one stands anywhere (not per barracks); triarii need
  one standing. Iron x0.67 for velites, hastati and triarii.
- **Castra** is Rome's forward depot (PLAY_BIG_SOON change 11) and heals 1% a
  second within 10 m. No palisade yet (later, with the wall plan service); the
  Mercenary Camp is Carthage's forward depot.
- **Levy** countdown sits in the barracks panel, not the top bar. Replacement
  refunds 25% of a squad's men unless it fell within 30 m of its own buildings.
- **Veterancy.** A battle counts when a squad that fought has been out of combat
  for 20 s. Hastati: rank 1 (principes) at 2 battles, rank 2 at 4; +15% health
  and +10% damage per rank. No new shield emblem yet (art).
- **Late gold sink (PLAY_BIG_SOON change 10):** Carthage spends gold on bands and
  elephants; Rome on Cretan archers (40 gold, no iron) and market iron.
- **Art:** the four new buildings borrow barracks-yard and portico archetypes in
  the nation palette until the art pass.
- **Balance pass (duel matrix, 18 Normal matches x 25 min).** Levy 5 men a home (not
  4); every band's upkeep +1 gold a minute; Hanno hires less (mercenary share 0.15),
  Hasdrubal more (0.7, ranged share 0.5, wave 6, Gallic/Iberian foot in his list).
  Result: Rome 10/18, Carthage 8/18. Open: Hanno still wins 5 of 6 against the
  Romans; Scipio and Marcellus attack his full garrison by minute 3-6 and lose their
  armies. His recruitment share, personality and the economic strategy's pace did not
  move it; the fix belongs in the AI's decision to attack, not in Hanno's numbers.

## 1. Where the nations stand today

From `assets/data/nations/roman_republic.json`, `carthage.json`, `troops/base.json`,
`commander_catalog.cpp` and `assets/data/ai/doctrines.json`.

| Aspect | Rome | Carthage | Difference |
| --- | --- | --- | --- |
| Buildings | barracks, home, farm, market, temple, tower, walls | same | art only |
| Economy | food -> civilians -> 18 men; wood, stone, iron, gold | same | none |
| Core troops | archer, legionary, triarius, equites, horse archer, horse spear, healer, 4 siege, builder | same 12 types | stats within ~10% |
| Unique troops | velites (10 iron) | slinger (0 iron), elephant | 2 units |
| Squad sizes | legionary 18, triarius 26, archer 24 | citizen infantry 16, spear 28, archer 22 | small |
| Defensive layout | testudo (missiles x0.15, 0.42 speed) | shield wall (missiles x0.45, 0.58 speed) | real but narrow |
| Commanders | Fabius: regen aura, spearmen; Scipio: attack aura, swordsmen; Marcellus: speed aura, archers | Hanno: regen, spearmen; Hannibal: attack, swordsmen; Hasdrubal: speed, archers | **exact mirrors** |
| AI town plans | Bulwark, Star Camp, Vanguard Chevron | Ring Town, Barcid Triangle, Hexagon | shape only |

Two facts from the headless duels run for `PLAY_BIG_SOON.md` matter here: both nations
hit the army cap by minute 3-5 and then bank 3,500-10,600 gold they cannot spend. Gold
is the natural lever to split them.

## 2. The design in one table

| | Rome: the Republic in arms | Carthage: the merchant empire |
| --- | --- | --- |
| Turns into an army | men (citizen levy) | gold (hired bands) |
| Scarce | iron, time | citizens, steady gold |
| Rhythm | steady flow plus a levy pulse every 2 minutes | bursts: hire a whole band at once, then pay for it |
| Unique buildings | Castra (marching camp), Via (road), Fabrica (armoury) | Mercenary Camp, Emporion / Harbour, Elephant Stables |
| Core of the army | legion: velites, hastati, principes, triarii, few equites | small citizen phalanx plus Gauls, Iberians, Balearics, Numidians, elephants |
| Strength | replaces losses, digs in anywhere, grinds | fast, varied, decisive in one battle |
| Weakness | slow, poor cavalry, iron-hungry heavy infantry | expensive, fragile loyalty, few men of its own |
| Wins by | attrition: lose battles, win the war | annihilation: one perfect battle, envelopment |
| Loses by | being out-manoeuvred before the levy arrives | running out of money |

## 3. Rome

### 3.1 Economy: the levy

- **Domus.** Roman homes hold 6 civilians in their lifetime (Carthage stays at the
  `PLAY_BIG_SOON` baseline). Citizens are Rome's resource.
- **Dilectus (levy day).** Every 120 s each Roman barracks receives 4 men of reserve per
  standing home within 40 m, up to its ceiling. The top bar shows a countdown; a horn
  sounds. This is the pulse in Rome's rhythm. Code, S-M (a system next to
  `HomeSystem`, using `grant_manpower`).
- **Replacement.** When a Roman squad is destroyed, 25% of its men return to the
  nearest barracks reserve as wounded veterans ("the Senate will find more"). Code, S.
- **Iron is the pinch.** Legionary stays at 30 iron. The **Fabrica** (armoury: 80 wood,
  60 stone, 12 s) cuts Roman infantry iron costs by a third while it stands, max one
  counts. Code, S (a cost modifier in `production_ruling`), data for the building.
- **Gold.** Rome has little use for gold beyond temples and the market. That is
  intended: Rome's gold sink is buying iron.

### 3.2 Unique buildings

| Building | Cost | Does | Reuses |
| --- | --- | --- | --- |
| Castra (marching camp) | 120 wood, 14 s, any crews | Palisade square with gates; a resource drop-off; units inside heal 1% a second; the barracks rally point can target it and new squads walk there | wall plan service (`wall_plan_service.cpp`) for the palisade, barracks yard depot code, healing system |
| Via (road) | 5 wood a segment, drag to place | +25% move speed for every unit on it, +40% for haulers; never blocks | wall-link drag placement, terrain road decal (maps already author `roads`) |
| Fabrica | 80 wood, 60 stone | Infantry iron -33% | barracks body and renderer slot |

### 3.3 Roster

| Slot | Today | Proposed | Notes |
| --- | --- | --- | --- |
| Skirmisher | velites 16 | velites 16, 10 iron | unchanged |
| Line | legionary 18, 30 iron | **hastati** 18, 20 iron | the levy's line infantry |
| Veteran line | - | **principes**: hastati squads that survive two battles are promoted, +15% health, +10% damage, new shield emblem | rank, no recruit |
| Reserve | triarius 26 | triarii 26, only from a barracks with a Fabrica | elite, few |
| Cavalry | equites, horse spear, horse archer | equites (horse spear stats) only, capped at 4 squads | Rome's cavalry was weak |
| Missile | auxiliary archer | Cretan archers, cost 40 gold instead of iron | Rome hired Cretan archers; gives Rome a small gold sink |
| Siege | onager, ballista, aries, turris | unchanged, Marcellus builds them 30% faster | siege of Syracuse |

Rank (principes, then a third "triarius-veteran" tier) is new code: a per-squad
battle count on `UnitComponent`, promotion on survival, saved with the unit. Medium.

### 3.4 Recruitment rhythm

Continuous trickle from the barracks, a levy pulse every two minutes, losses partly
refunded. A Roman player always has the next army coming; their question is iron and
where to dig in.

## 4. Carthage

### 4.1 Economy: trade and silver

- **Emporion** (trade house, replaces the marketplace for Carthage; 60 wood, 40 stone,
  50 gold): +30 gold a minute, +5 for every farm within 40 m (grain exported). Sells
  goods at 1.5x the marketplace rate. Code, S (a timer in `MarketplaceSystem`).
- **Harbour** (coastal maps only, Decision 4): +60 gold a minute; mercenary contracts
  can arrive by ship there.
- **Silver.** Carthaginian crews collect 40 gold a trip from cursed gold veins and
  silver seams (Iberian mines). Code, S.
- **Citizens are few.** Carthage homes keep the baseline lifetime and civilians grant
  12 men, not 18. The citizen army is a core, not the bulk.

### 4.2 Mercenaries

- **Mercenary Camp** (100 wood, 40 stone, 12 s): recruits bands for **gold only, no
  reserve, no iron**. A band appears at the camp after its contract time.
- Bands use the ally troop sets that already exist (`assets/data/nations/gauls.json`,
  `iberians.json`) plus Carthage's own slinger and Numidian entries. The unit keeps its
  owner but carries the ally nation in `UnitComponent::nation_id`, so it renders and
  fights with that nation's profile.

| Band | Men | Gold | Contract | Upkeep / min | Character |
| --- | --- | --- | --- | --- | --- |
| Gallic warband | 16 | 110 | 8 s | 6 | hits hard, breaks fast |
| Iberian scutarii | 16 | 130 | 8 s | 7 | steady line |
| Balearic slingers | 16 | 90 | 6 s | 5 | longest-ranged foot |
| Numidian horse | 9 | 140 | 9 s | 8 | fastest unit, javelins |
| Iberian / Gallic cavalry | 9 | 150 | 10 s | 8 | shock horse |

- **Upkeep.** Charged every minute. At zero gold, bands are unpaid: -20% morale and a
  warning. After 60 s unpaid, the band with the lowest loyalty leaves (becomes neutral
  and walks to the map edge). Hannibal's presence halves the morale loss. Decision 2.
- **Cap.** Mercenaries count against the army cap like anyone else.

### 4.3 Elephants

- **Elephant Stables** (120 wood, 80 stone, 30 gold): the only place elephants come from;
  barracks no longer offer them.
- An elephant costs 150 gold and 90 food, no reserve; 20 s to train; at most 8 alive.
- Panic: a routed elephant tramples the nearest squads of either side for 6 s. Check
  what exists in combat before building it; this document did not verify it.

### 4.4 Roster

| Slot | Proposed | Source of men |
| --- | --- | --- |
| Citizen line | Liby-Phoenician spear (28), citizen infantry (16) | reserve |
| Elite | Sacred Band: citizen infantry from a barracks with a temple; +20% health | reserve |
| Missile | Libyan archers (reserve), Balearic slingers (gold) | both |
| Cavalry | Numidian horse (gold), Iberian/Gallic horse (gold); the horse archer becomes Numidian javelin riders | gold |
| Shock | elephants (stables) | gold + food |
| Foot mercenaries | Gauls, Iberians | gold |
| Siege | as today; Hannibal's catapults +10% range | reserve |

### 4.5 Recruitment rhythm

Save, then hire a whole army in one minute; fight; pay; repeat. A Carthaginian player
watches the treasury, not the reserve.

## 5. How each nation wins

- **Rome** wants a long match: dig a castra near the enemy, build roads home, absorb a
  defeat, come back with the next levy. It should lose the first big battle against a
  good Carthaginian about half the time and still win the match.
- **Carthage** wants a short match: hire at the right moment, envelop, break the Roman
  army before the next levy pulse, and strike the barracks. If the war drags, upkeep
  and few citizens catch up with it.

## 6. Commanders

Today the three commanders of each nation mirror the other nation's three exactly
(regen/spearmen, attack/swordsmen, speed/archers). Keep the auras and signature moves;
add one trait each that plays into the nation's economy.

| Commander | Historical hook | Trait proposed |
| --- | --- | --- |
| Fabius Maximus "Cunctator" | delaying war after Trasimene, 217 BC | enemy crews inside his territory gather 20% slower; walls and towers 20% cheaper |
| Scipio (Africanus) | took New Carthage 209 BC; Masinissa's Numidians joined him | after minute 10 may hire Numidian horse from a captured Carthaginian camp; attack aura unchanged |
| Marcellus, "the Sword of Rome" | siege of Syracuse 214-212 BC | siege engines 30% faster to build; levy pulse every 90 s instead of 120 |
| Hannibal Barca | held a many-nation army together in Italy for 15 years; Cannae 216 BC | halves unpaid-morale loss; +15% damage on flank and rear attacks |
| Hasdrubal Barca | ran the Iberian silver country | +50% gold from silver and veins; Iberian bands 20% cheaper |
| Hanno the Great | led the anti-Barcid, Africa-first faction | Emporion income +50%; armies hold back (garrison doctrine already) |

## 7. Historical grounding (brief)

- **Roman manpower.** Polybius (2.24) lists Rome and its allies as able to field about
  700,000 foot and 70,000 horse in 225 BC. After Cannae Rome raised new legions,
  including 8,000 slaves bought by the state (Livy 22.57). The levy and the refund of
  losses come from this.
- **The camp.** Polybius (6.27-42) describes the legion building the same fortified
  camp at the end of every march. The castra.
- **Roads.** The Via Flaminia was built in 220 BC by Gaius Flaminius, who died at Lake
  Trasimene in 217 BC. (Humour hook: the road was finished; the consul was not.)
- **Cohorts and ranks.** The legion of the period fought in velites, hastati, principes
  and triarii by age and experience (Polybius 6.21-23). Promotion by survival is a
  game abstraction of that.
- **Carthage's armies** were mostly hired or subject troops: Libyans, Iberians, Gauls,
  Balearic slingers and Numidian horse. Hannibal's own inscription at Cape Lacinium,
  quoted by Polybius (3.33, 3.56), lists them.
- **Mercenaries and pay.** After the First Punic War Carthage could not pay its
  mercenaries and they revolted (the Mercenary War, 241-237 BC; Polybius 1.65-88).
  Upkeep and desertion come from this.
- **Silver.** Polybius, quoted by Strabo (3.2.10), describes the New Carthage silver
  mines employing 40,000 men and paying 25,000 drachmae a day.
- **Elephants.** Hannibal brought 37 elephants across the Rhone on rafts (Polybius
  3.42-46; the game already has rafts). At Zama, Scipio left lanes in his line for
  Carthage's 80 elephants to run through (Polybius 15.9-12).
- **Two consuls.** At Cannae the consuls Varro and Paullus commanded on alternate days
  (Polybius 3.110). A humour hook, not a mechanic.

## 8. Dark fantasy and humour

The history stays the frame; the dark layer and the jokes sit on top of it.

- **Rome and the dead.** Rome is pious and superstitious: the Sibylline Books were
  consulted after Trasimene and Cannae (Livy 22.9, 22.57), and the Lemuria rites drove
  out the restless dead. Roman temples purify Iron Sepulcher shrines 50% faster and
  show the next rising's timer. Clearing a zone grants Rome a levy pulse.
- **Carthage and the dead.** Carthage pays anyone. At a captured shrine, Carthage can
  hire a skeleton band (data exists: `skeleton_swordsman`, `skeleton_archer`) for gold;
  the band decays 2% health a minute and costs no upkeep. Announcement: "The dead have
  read the contract. They want it in silver." Use Baal Hammon, Tanit and Melqart for
  Punic flavour; leave the tophet alone.
- **Humour, by nation.** Rome: Senate dispatches ("The Senate acknowledges your
  forty-seventh request for iron and has formed a committee."), the levy horn, the two
  consuls. Carthage: invoices, Gauls who discover they were not paid ("The Gauls are
  leaving. The Gauls are taking the cart."), elephants with opinions.
- Floating numbers for upkeep, levy and mercenary payments follow the essential-feedback
  rule.

## 9. What the AI must learn

Today the AI plays both nations with the same economy logic; only doctrine shares and
town plans differ (`ai_doctrine_catalog.cpp`, `doctrines.json`, `town_plans.json`).

**Rome AI**
- Macro targets (`ai_macro_targets.cpp`): homes before barracks; one Fabrica at the
  third barracks; one castra toward the enemy before the first wave.
- Waves (`ai_attack_wave.cpp`): time the commit to land just after a levy pulse; on a
  lost battle, regroup at the castra instead of home.
- Roads: one road from home to the castra (builder wish list, after life support).
- Retreat earlier; losses refund men.

**Carthage AI**
- Treasury model in `EconomyBehavior`: income per minute vs upkeep. Never hire a band
  whose upkeep pushes income below zero for more than 2 minutes.
- Production (`production_behavior.cpp`): the Mercenary Camp is a recruitment building
  with gold prices; mercenary share becomes a doctrine field
  (`recruitment.mercenary_share`: Hannibal 0.6, Hasdrubal 0.5, Hanno 0.3).
- Hire in bursts: bank for a wave, then hire the wave in one go (the wave
  `assembling` phase already exists).
- Elephants only from stables; Hannibal's doctrine already prefers elephants.
- Never let gold pile up: the 3,500-10,600 idle gold seen in the duels is the bug this
  fixes.

**Both**
- `doctrines.json` gains the new fields; `generate-town-plans.py` gains the new
  buildings as plan steps (mind the 9 m anchor clearance that silently dropped every
  marketplace before).
- The headless duel matrix (all nine Rome x Carthage pairings plus mirrors) is the
  regression check.

## 10. Phased plan

Each phase ships on its own and keeps the game playable.

**Phase 0 - spikes (1-2 days, O or D).**
- Spawn a Gallic band owned by a Carthage player (`UnitComponent::nation_id` =
  Gauls) in the arena; confirm render, formation profile, combat profile and save/load.
- Find the hook for a per-minute gold charge (`MarketplaceSystem` or a new small
  system) and for a cost modifier in `production_ruling`.
- Player-proxy duel harness from `PLAY_BIG_SOON.md` section 9.

**Phase 1 - data identity (data only, low risk, S5).**
- Roster cuts (Decision 5), squad sizes and costs from sections 3.3 and 4.4 where they
  are plain numbers: hastati iron 20, Cretan archers, Carthage civilian grant.
- Commander doctrine fields that already exist (recruitment shares) aligned with
  section 6.
- Update `HomeManpowerSystemTest`, `CompiledDefaultsMatchTheShippedTroopData`,
  `TroopCatalogLoaderTest` with the data.

**Phase 2 - Carthage gold army (code, M-L).**
- Mercenary Camp (a barracks variant with its own recruit list via
  `recruiting_building_for`), gold-priced bands with per-unit nation, upkeep and
  desertion, Emporion income, Elephant Stables. AI treasury model and mercenary share.
- Art: camp and stables renderers (F1/F2, reuse barracks parts).

**Phase 3 - Rome levy army (code, M-L).**
- Levy pulse, loss refund, Fabrica modifier, castra (palisade plan + depot + heal),
  rank promotion. AI levy timing and castra.

**Phase 4 - roads, harbour, commander traits, dark layer (code, M).**
- Roads (Decision 3), Harbour on coastal maps, the six traits, shrine piety and
  skeleton contracts, dispatch and invoice texts (`qsTr`; orchestrator runs
  translations).

**Phase 5 - balance (O with S agents).**
- Headless duel matrix at Normal, 30 minutes, 3 seeds each: win rate per nation,
  minute of first wave, gold at minute 20, men at 3/7/15 minutes.
- Campaign playthrough harness: adjust floors where nation data moved missions.
- Hand playtests filmed, one per nation per map type.

## 11. Balance risks

- **Gold snowball.** Emporion income plus silver can outgrow Rome's levy on long
  matches. Guard: income caps per building count; measure gold at minute 20 in the
  duel matrix.
- **Mercenary burst too strong.** Hiring 500 men in one minute can beat any defence.
  Guard: contract time per band and one band at a time per camp; the cap still applies.
- **Desertion feels unfair.** Guard: a clear warning at 30 s of income left, a 60 s
  grace, never more than one band leaving per minute. Decision 2 can drop it.
- **Rome too passive.** Refunds and levies can make Rome unbeatable on defence and dull
  to play. Guard: refund only on squads destroyed in the field, not inside walls.
- **Cavalry gap.** Rome with 4 equites squads may have no answer to Numidian horse.
  Guard: triarii and the testudo must counter cavalry in the arena who-wins scenarios
  before shipping.
- **Elephants.** Moving them to gold makes them earlier and more common. Guard: the
  limit of 8, and massed archers stay the counter (lore already says so).
- **AI cannot play it.** Every mechanic the AI ignores becomes a player exploit or a
  dead feature. No phase ships without its AI half and a duel narration showing the AI
  using it.
- **Campaign drift.** Missions fix a nation and often a commander; traits and roster
  cuts change them. Guard: the campaign playthrough harness, and authored mission
  overrides where history needs it (e.g. Scipio without Numidians before 204 BC).
- **Tests and saves.** Data tests pin costs and squad sizes; update them with the data.
  Saves from 0.x may be wiped (accepted for 0.x).
- **Performance.** Mercenary bursts and skeleton contracts add squads, not new
  per-soldier work; the budget in `PLAY_BIG_SOON.md` section 7 covers them.
