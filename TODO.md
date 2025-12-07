# Campaign Expansion Plan (Hannibal vs Rome)

Goal: define the campaign authoring pipeline and player-facing experience before implementing code. Two complementary tracks: (1) developer tooling and data needed to author missions/maps/events reliably, and (2) runtime systems + UI that surface those missions, track progress in SQLite, and present a cohesive Hannibal-vs-Rome campaign.

## A) Mission Framework (Authoring)
- Mission config schema (JSON) — what authors control:
  - Identity: `id`, `title`, `summary`, `order_index`, `world_region_id` (ties to strategy map).
  - Level: `map_path` (playable map), optional `base_map` (override a shared base), biome override (ground type, lighting, weather seeds).
  - Player setup: nation, color, starting units/buildings, starting resources/tech, population cap tweaks, rally points.
  - AI setups: nation, color, difficulty, personality knobs (aggression/defense/harass), build/tech pacing, waves/spawns (timing, composition, entry points), reinforcements on triggers.
  - Victory conditions: destroy target(s), survive duration, hold capture points, escort to zone, resource quota; allow multiple win clauses and optional secondary objectives.
  - Defeat conditions: lose key structure/unit, timer expires, lose control points.
  - Events: timed or state-based triggers that spawn units, play VO/text, change AI stance, weather shifts.
  - Presentation: thumbnail, intro text, loading-tip, ambient track; optional cut-in messages.
  - Rewards/progression: unlock next mission, medal tier, stat tracking fields.
- Validator CLI (dev safety rail): JSON schema lint; cross-check referenced assets/maps exist; verify `order_index` is contiguous; ensure entity/unit types referenced exist; optional “dry-run compile” to catch missing fields. Hook into CMake/`make` so invalid missions break the build.
- Loader wiring: `start_campaign_mission(id)` reads mission config, constructs player/AI configs, applies biome/ground overrides, configures victory service from mission rules, and injects events/waves into existing systems. Removes any hardcoded Carthage-vs-Rome defaults.

## B) Campaign Content (Hannibal Perspective, 8 Missions)
- Mission 1 (Crossing the Rhône): teach crossings/tempo; map with two fords + bridge; objective to hold crossings for N minutes or destroy Roman outpost; Carthage starts with a veteran core, minimal eco; timed reinforcements (rafts) to introduce pacing; Romans probe then counter with cavalry if timer hits.
- Later beats (to detail later): Ticino (flanking tutorial), Trebia (ambush with cold river penalty), Trasimene (fog/LOS ambush), Cannae (double envelopment), Campania (siege/attrition), Alps (retreat/supply/logistics strain), Zama (finale vs Scipio, elephant counters).
- Progression rules: seed 8 missions with only Mission 1 unlocked; victory sets completed/unlocks next; defeat increments attempts only; stats tracked for UI medals.

### Mission-to-Ground-Type Mapping
| **Mission**                          | **Assigned Ground Type**       |
| ------------------------------------ | ------------------------------ |
| **Crossing the Rhône**               | **Light-Brown Rocky Soil**     |
| **Ticino (flanking tutorial)**       | **Dark Fertile Farmland Soil** |
| **Trebia (ambush)**                  | **Deep Green + Mud**           |
| **Trasimene (fog ambush)**           | **Dry Mediterranean Grass**    |
| **Cannae (double envelopment)**      | **Dry Mediterranean Grass**    |
| **Campania (siege/attrition)**       | **Light-Brown Rocky Soil**     |
| **Alps (retreat / supply struggle)** | **Alpine Rock + Snow Mix**     |
| **Zama (finale)**                    | **Dry Mediterranean Grass**    |

## C) Player Progress (SQLite)
- Tables:
  - `campaigns`: id/slug, title, summary, map_path, mission_config_path, order_index, difficulty tag, thumbnail, world_region_id.
  - `campaign_progress`: campaign_id PK, completed (bool), unlocked (bool), best_time (ms or ISO duration), attempts (int), completed_at (timestamp).
- Migration: bump schema version; create/alter tables to add new columns; seed 8 missions (only Mission 1 unlocked); keep backward compatibility with existing saves.
- Runtime flow: on start, load config; on victory, mark completed, unlock next mission, update best_time/attempts; on defeat, increment attempts only; refresh `available_campaigns` for UI bindings.

## D) UI/UX for Campaign Screen
- Layout: left column list of missions with title, short blurb, lock/completed badges, difficulty tag; right pane is the interactive Mediterranean map.
- Detail card (when selected): objectives (primary/secondary), recommended approach, start CTA, stats (attempts, best time), thumbnail, mission difficulty. Disable start when locked; show why it’s locked.
- Progress affordances: header progress bar (completed/total) and “Continue Campaign” button that picks the first unlocked-but-incomplete mission.
- Interaction: selecting a mission highlights its province on the map, pans/zooms to it, and shows a tooltip with historical note/control status.

## E) Mediterranean Strategic Map (Right Pane)
- Asset/data: stylized Mediterranean map (Iberia → Asia Minor, North Africa → Alps) authored as static geometry plus `assets/campaign_map/provinces.json` describing province polygons, owner, label anchors, and mission linkages.
- Rendering/interaction: pan/zoom with bounds; hover highlights province; click shows tooltip (who controls it, mission association, short historical note). Keep draw calls reasonable (cached geometry, batched fills).
- State visualization: provinces tinted by control (Carthage/Rome/neutral); selected mission’s province pulses or outlines; optional subtle sea/shore gradient for depth.
- Data link: mission config includes `world_region_id`; map uses that to focus/tint and to show progress along Hannibal’s path.

## F) Next Decisions to Unblock Implementation
- Freeze mission config fields and file locations (e.g., `assets/campaigns/mission_crossing_rhone.json`) so the validator and loader targets are stable.
- Approve province list and visual style for the Mediterranean map (level of stylization, color palette, label density).
- Confirm Mission 1 objectives, map layout (two fords + bridge), and which existing systems (victory service, AI waves/events) to extend or reuse for timed reinforcements and stance changes.
