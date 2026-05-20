# TODO: The Iron Sepulcher Non-Playable Death Faction

## Goal

Add **The Iron Sepulcher** as a campaign-only, non-playable ancient death faction. This is not a normal third playable nation. Rome and Carthage stay as the main historical factions; Iron Sepulcher is a defensive undead hazard/enemy awakened by shrines, tombs, ruins, and late-campaign bloodshed.

Core constraints:

- No economy.
- No farms, homes, barracks, towers, walls, or normal production buildings.
- No diplomacy or player selection.
- No normal expansion.
- AI defaults to defense, shrine guarding, ambushes, and local retaliation.
- Units never rout.
- Weak to fire, priests/healers, and heavy cavalry shock.
- Campaign/map scripts spawn them from shrines, ruins, tombs, and battlefields.

## Existing Repo Systems To Use

- Nation data already lives in `assets/data/nations/*.json`.
- Base troop catalog already lives in `assets/data/troops/base.json`.
- Nation parsing is in `game/systems/nation_loader.cpp`.
- Nation IDs are hardcoded in `game/systems/nation_id.h`.
- Troop IDs are hardcoded in `game/units/troop_type.h`.
- Spawn IDs are hardcoded in `game/units/spawn_type.h`.
- Unit factories are registered in `game/units/factory.cpp`.
- Unit implementations live in `game/units/*.cpp`.
- AI strategy/state/config lives under `game/systems/ai_system/`.
- Morale already exists as `Engine::Core::MoraleComponent` in `game/core/component.h`.
- Commander aura/rally/death morale behavior already exists in `game/systems/commander_system.cpp`.
- Map props already support `ruins` and `magic_shrine` in `game/map/map_definition.h`.
- Fire props already exist as `firecamp`.
- Serialization already persists nation/unit/morale/world prop data in `game/core/serialization.cpp`.

## Phase 1: Register The Faction Without Making It Playable

- [ ] Add `NationID::IronSepulcher` to `game/systems/nation_id.h`.
  - String ID: `iron_sepulcher`.
  - Display name: `The Iron Sepulcher`.
  - Update `nation_id_to_qstring`, `try_parse_nation_id`, and hash behavior if needed.

- [ ] Add `assets/data/nations/iron_sepulcher.json`.
  - Include only undead troops.
  - Do not define builder/civilian/building economy troops.
  - Use a defensive formation type until an undead formation exists.
  - Candidate shape:

```json
{
  "id": "iron_sepulcher",
  "display_name": "The Iron Sepulcher",
  "primary_building": "none",
  "playable": false,
  "has_economy": false,
  "ai_profile": "sepulcher_defense",
  "formation_type": "barbarian",
  "troops": [
    {
      "id": "skeleton_swordsman",
      "display_name": "Skeleton Swordsman",
      "production": { "cost": 0, "build_time": 0.0, "priority": 10, "is_melee": true },
      "combat": {
        "health": 620,
        "max_health": 620,
        "speed": 1.9,
        "vision_range": 13.0,
        "ranged_range": 1.4,
        "ranged_damage": 1,
        "ranged_cooldown": 2.0,
        "melee_range": 1.45,
        "melee_damage": 11,
        "melee_cooldown": 0.9,
        "can_ranged": false,
        "can_melee": true
      },
      "visuals": {
        "render_scale": 0.56,
        "selection_ring_size": 1.05,
        "renderer_id": "troops/iron_sepulcher/skeleton_swordsman"
      },
      "formation": { "individuals_per_unit": 18, "max_units_per_row": 6 }
    },
    {
      "id": "skeleton_archer",
      "display_name": "Skeleton Archer",
      "production": { "cost": 0, "build_time": 0.0, "priority": 8, "is_melee": false },
      "combat": {
        "health": 480,
        "max_health": 480,
        "speed": 1.8,
        "vision_range": 16.0,
        "ranged_range": 7.2,
        "ranged_damage": 14,
        "ranged_cooldown": 0.85,
        "melee_range": 1.3,
        "melee_damage": 3,
        "melee_cooldown": 1.2,
        "can_ranged": true,
        "can_melee": true
      },
      "visuals": {
        "render_scale": 0.52,
        "selection_ring_size": 1.05,
        "renderer_id": "troops/iron_sepulcher/skeleton_archer"
      },
      "formation": { "individuals_per_unit": 18, "max_units_per_row": 6 }
    },
    {
      "id": "grave_priest",
      "display_name": "Grave Priest",
      "production": { "cost": 0, "build_time": 0.0, "priority": 20, "is_melee": false },
      "combat": {
        "health": 520,
        "max_health": 520,
        "speed": 1.65,
        "vision_range": 15.0,
        "ranged_range": 8.0,
        "ranged_damage": 24,
        "ranged_cooldown": 2.6,
        "melee_range": 1.2,
        "melee_damage": 2,
        "melee_cooldown": 1.8,
        "can_ranged": true,
        "can_melee": true
      },
      "visuals": {
        "render_scale": 0.6,
        "selection_ring_size": 0.7,
        "renderer_id": "troops/iron_sepulcher/grave_priest"
      },
      "formation": { "individuals_per_unit": 1, "max_units_per_row": 1 }
    }
  ]
}
```

- [ ] Extend `Nation` in `game/systems/nation_registry.h` with metadata flags:
  - `bool playable = true;`
  - `bool has_economy = true;`
  - `QString ai_profile = "standard";`
  - Optional: `bool selectable_in_skirmish = true;`

- [ ] Update `game/systems/nation_loader.cpp`.
  - Parse `playable`.
  - Parse `has_economy`.
  - Parse `ai_profile`.
  - Accept `primary_building: "none"` without warning or fallback to barracks.
  - Keep Rome/Carthage defaults unchanged.

- [ ] Update UI/skirmish nation selection so Iron Sepulcher is hidden.
  - Check `ui/qml/PlayerConfigPanel.qml`, `ui/qml/MapSelect.qml`, `ui/theme.h`, and code exposing nation lists.
  - Acceptance: player can still select Rome/Carthage only; campaign/map scripts can still assign `iron_sepulcher` to owner 3 or higher.

## Phase 2: Add Undead Troop And Spawn Types

- [ ] Extend `game/units/troop_type.h`.
  - Add `SkeletonSwordsman`.
  - Add `SkeletonArcher`.
  - Add `GravePriest`.
  - Add string mappings:
    - `skeleton_swordsman`
    - `skeleton_archer`
    - `grave_priest`
  - Update `is_commander_troop` if Grave Priest should count as a commander for one-per-owner restrictions. Preferred: do **not** use the existing commander limit; allow multiple shrine priests if a map requires them.

- [ ] Extend `game/units/spawn_type.h`.
  - Add `SkeletonSwordsman`.
  - Add `SkeletonArcher`.
  - Add `GravePriest`.
  - Update parse/to-string functions.
  - Update `spawn_typeToTroopType`.
  - Update `spawn_typeFromTroopType`.
  - Update command eligibility helpers:
    - undead can attack, guard, hold, patrol.
    - undead should not use run mode unless deliberately balanced later.

- [ ] Add base troop catalog entries to `assets/data/troops/base.json`.
  - Add the three undead troop IDs so `TroopCatalog` and `TroopProfileService` can resolve them.
  - Keep production cost/build time at `0` because they are spawned by scripts/hazards, not buildings.

- [ ] Create unit implementation files.
  - `game/units/skeleton_swordsman.h`
  - `game/units/skeleton_swordsman.cpp`
  - `game/units/skeleton_archer.h`
  - `game/units/skeleton_archer.cpp`
  - `game/units/grave_priest.h`
  - `game/units/grave_priest.cpp`

- [ ] Register factories in `game/units/factory.cpp`.
  - `SpawnType::SkeletonSwordsman -> SkeletonSwordsman::Create`
  - `SpawnType::SkeletonArcher -> SkeletonArcher::Create`
  - `SpawnType::GravePriest -> GravePriest::Create`

- [ ] Reuse existing unit creation patterns.
  - Skeleton Swordsman can start as a copy of `Swordsman` with undead profile lookup.
  - Skeleton Archer can start as a copy of `Archer` with undead profile lookup.
  - Grave Priest can start as a copy of `Healer`, then add magic attack behavior in a later phase.

- [ ] Update `game/CMakeLists.txt` with new unit source/header files.

## Phase 3: Add Undead Rule Components

- [ ] Add an `UndeadComponent` to `game/core/component.h`.
  - Fields:
    - `bool morale_immune = true;`
    - `float fire_damage_multiplier = 1.5F;`
    - `float priest_damage_multiplier = 1.4F;`
    - `float cavalry_charge_damage_multiplier = 1.25F;`
    - `bool counts_for_economy = false;`

- [ ] Add `UndeadComponent` to the three undead unit constructors.

- [ ] Update morale handling.
  - Find all logic that applies `MoraleComponent`, especially `game/systems/commander_system.cpp` and combat morale shock code.
  - If target has `UndeadComponent::morale_immune`, skip morale reduction, wavering, and routing.
  - Acceptance: skeletons and Grave Priests never route even under commander death shock or cursed volleys.

- [ ] Decide whether undead units should have no `MoraleComponent` or have one that never changes.
  - Preferred: do not add `MoraleComponent` to undead units unless UI requires it.
  - If UI expects morale, set morale to max and hide routing.

- [ ] Update serialization in `game/core/serialization.cpp`.
  - Persist `UndeadComponent`.
  - Add tests in `tests/core/serialization_test.cpp`.

## Phase 4: Grave Priest Healing And Fire Magic

- [ ] Reuse `HealerComponent` for Grave Priest undead healing.
  - Healing target filter must heal only allies with `UndeadComponent`.
  - It must not heal Rome/Carthage living units.
  - It must not be healed by normal Roman/Carthage healers unless deliberately allowed later.

- [ ] Add a fire magic projectile path.
  - Existing ranged logic uses `AttackComponent` and projectile systems such as `game/systems/arrow_system.cpp`.
  - Add either:
    - a new `MagicProjectileComponent` and `MagicProjectileSystem`, or
    - an extension to existing projectile code with projectile type `Fireball`.
  - Preferred first pass: add `ProjectileKind { Arrow, Fireball, CursedArrow }` rather than duplicating all projectile movement logic.

- [ ] Grave Priest ability: `Fireball`.
  - Medium range.
  - Slow cooldown.
  - Splash damage in a small radius.
  - Applies high bonus damage to units marked vulnerable to fire.
  - Adds visible VFX through existing render/effects paths.

- [ ] Add renderer/VFX assets.
  - Search existing VFX hooks in `app/controllers/action_vfx.cpp`.
  - Add a small orange/red projectile trail and impact burst.
  - Avoid making this a UI-only effect; it must be tied to combat events.

- [ ] Tests.
  - Fireball damages enemies in area.
  - Fireball does not damage allied undead unless friendly fire is globally enabled.
  - Fireball respects cooldown.
  - Fireball can kill units and triggers existing cleanup/death behavior.

## Phase 5: Skeleton Archer Cursed Volley

- [ ] Add cursed arrow support.
  - Implement as projectile/combat status, not raw extra damage.
  - Suggested status component: `CursedStatusComponent`.
  - Fields:
    - `float morale_penalty_per_hit = 8.0F;`
    - `float duration = 6.0F;`
    - `int stacks = 1;`

- [ ] Hook cursed arrows to `SkeletonArcher`.
  - Early campaign: normal low-damage arrows.
  - Late campaign/map flag: cursed volley enabled.
  - Data option in nation/troop JSON: `abilities: ["cursed_arrow_volley"]`.

- [ ] Extend nation/troop loader to parse ability IDs.
  - Add `std::vector<std::string> abilities` to `NationTroopVariant`.
  - Parse from troop JSON.
  - Merge through `TroopProfileService`.

- [ ] Morale interaction.
  - Living units hit by cursed arrows lose morale.
  - Undead units ignore cursed morale effects.
  - Cursed volley should pressure the player without doing high raw damage.

- [ ] Tests.
  - Cursed arrows reduce living morale.
  - Cursed arrows do not affect undead morale.
  - Cursed arrows stack or refresh exactly as specified.

## Phase 6: Defensive No-Economy AI

- [ ] Add `AIStrategy::SepulcherDefense` in `game/systems/ai_system/ai_types.h`.

- [ ] Update `game/systems/ai_system/ai_strategy.cpp`.
  - Parse strategy strings:
    - `sepulcher_defense`
    - `undead_defense`
  - Config:
    - `target_builder_count = 0`
    - `base_home_target = 0`
    - `desired_barracks_count = 0`
    - `desired_defense_tower_count = 0`
    - `desired_wall_segment_count = 0`
    - `desired_catapult_count = 0`
    - `desired_outpost_barracks_count = 0`
    - `outpost_home_target = 0`
    - `reserve_units = high`
    - `harass_units = low or 0`
    - `proactive_attack_size = very high`
    - `reactive_attack_size = low`
    - `scouting_distance = shrine guard radius`

- [ ] Update AI behaviors to respect no-economy nations.
  - `game/systems/ai_system/behaviors/production_behavior.cpp`
    - If `nation->has_economy == false`, return immediately.
  - `game/systems/ai_system/behaviors/builder_behavior.cpp`
    - If no economy, return immediately.
  - `game/systems/ai_system/behaviors/expand_behavior.cpp`
    - If no economy, return immediately.
  - `game/systems/ai_system/ai_reasoner.cpp`
    - Do not transition no-economy AI into economy/expansion states.

- [ ] Add shrine/ruin guard anchors.
  - Current AI can use buildings/unit clusters as anchors.
  - Add support for map hazard anchors:
    - shrine position
    - tomb/ruin position
    - configured undead spawn zone
  - If no building exists, Iron Sepulcher should still defend its anchor rather than wandering.

- [ ] Add local retaliation.
  - If living units enter shrine radius, undead attack.
  - If living units retreat beyond leash radius, undead return to shrine.
  - Do not chase across the full map unless a campaign event explicitly unlocks the horde.

- [ ] Tests in `tests/systems/ai_system_test.cpp`.
  - Sepulcher AI never requests builders.
  - Sepulcher AI never requests barracks/home/tower/wall construction.
  - Sepulcher AI does not expand.
  - Sepulcher AI defends nearest shrine/ruin anchor.
  - Sepulcher AI returns to anchor when enemies leave leash radius.

## Phase 7: Shrine/Ruin Awakening System

- [ ] Add an `UndeadAwakeningSystem`.
  - Suggested files:
    - `game/systems/undead_awakening_system.h`
    - `game/systems/undead_awakening_system.cpp`
  - Runs after map/world bootstrap and during gameplay.

- [ ] Add map data for undead spawn zones.
  - Option A: extend existing `WorldProp::MagicShrine`/`Ruins` with undead fields.
  - Option B: add a new JSON section `undead_zones`.
  - Preferred for designer clarity: `undead_zones`.

```json
"undead_zones": [
  {
    "id": "old_temple_north",
    "anchor_type": "magic_shrine",
    "x": 42.0,
    "z": 18.0,
    "radius": 12.0,
    "leash_radius": 28.0,
    "owner_id": 3,
    "nation": "iron_sepulcher",
    "awaken_on": ["unit_enters_radius", "shrine_desecrated", "turn_8"],
    "waves": [
      { "trigger": "initial", "skeleton_swordsman": 3, "skeleton_archer": 2, "grave_priest": 1 },
      { "trigger": "late_campaign", "skeleton_swordsman": 8, "skeleton_archer": 5, "grave_priest": 2 }
    ]
  }
]
```

- [ ] Update map loader.
  - Add data structs in `game/map/map_definition.h`.
  - Parse in `game/map/map_loader.cpp`.
  - Serialize in `game/core/serialization.cpp` if saves need active zone state.

- [ ] Trigger conditions.
  - Unit enters shrine/ruin radius.
  - Shrine is desecrated/captured/interacted with.
  - Battle deaths near shrine exceed threshold.
  - Campaign turn/event reaches threshold.

- [ ] Spawn behavior.
  - Use `UnitFactoryRegistry` with `SpawnParams`.
  - Owner should usually be player id `3` or a reserved AI owner.
  - Nation must be `NationID::IronSepulcher`.
  - `ai_controlled = true`.
  - Spawn around anchor with spacing and collision checks.

- [ ] State persistence.
  - Zone awakened/not awakened.
  - Wave index.
  - Cooldowns.
  - Remaining anchor/leash data.

- [ ] Tests.
  - Zone does not spawn before trigger.
  - Zone spawns correct units after trigger.
  - Spawned units have Iron Sepulcher nation and AI component.
  - Dead/cleared zone does not respawn unless configured.

## Phase 8: Campaign Integration

- [ ] Decide campaign role.
  - Early: shrine warning, ruins guarded by a few skeletons.
  - Mid: desecrated shrines awaken patrols.
  - Late: larger sepulcher zones and Grave Priests appear.

- [ ] Add campaign mission/map JSON examples.
  - Use existing mission loading in `game/map/mission_loader.cpp`.
  - Add a map with `magic_shrine`, `ruins`, and one `undead_zone`.

- [ ] Make victory conditions ignore undead unless mission opts in.
  - Current victory rules likely check enemy structures/units.
  - Update `game/map/mission_victory_rules.cpp` so Iron Sepulcher can be:
    - ignored as ambient hazard, or
    - required objective: clear shrine/banish priest.

- [ ] Add objective types if needed.
  - `clear_undead_zone`
  - `purify_shrine`
  - `survive_undead_wave`

- [ ] Save/load.
  - Campaign saves must remember awakened zones and surviving undead units.

## Phase 9: Rendering And Audio

- [ ] Add renderer IDs.
  - `troops/iron_sepulcher/skeleton_swordsman`
  - `troops/iron_sepulcher/skeleton_archer`
  - `troops/iron_sepulcher/grave_priest`

- [ ] Implement renderers.
  - Search existing renderer registry and nation renderer patterns under `render/entity/nations/`.
  - First pass can reuse humanoid geometry with bone/ash colors, no new mesh requirement.
  - Grave Priest should be visually distinct from skeleton infantry: darker robe, staff/flame hand, shrine glow.

- [ ] Update `assets/visuals/unit_visuals.json` and `assets/visuals/unit_equipment_loadouts.json` if renderer lookup depends on those files.

- [ ] Add audio hooks.
  - Existing voice prefix logic is in `game/audio/audio_event_handler.cpp`.
  - Do not give Iron Sepulcher normal human command barks.
  - Use ambient rattle/whisper/stone scrape events for spawn/attack/death if the audio system has suitable placeholders.

## Phase 10: Fire And Priest Weaknesses

- [ ] Define damage tags.
  - `Physical`
  - `Piercing`
  - `Fire`
  - `Holy` or `Priest`
  - `Cursed`

- [ ] Extend damage application.
  - Check `game/systems/combat_system/damage_application.cpp`.
  - Apply `UndeadComponent` multipliers based on damage tag.

- [ ] Existing healer/medicus interaction.
  - Roman/Carthage healer can either:
    - damage undead as priest/holy attack, or
    - stay pure support and a later priest unit handles undead.
  - Preferred short-term: Healer attacks do small holy damage to undead only when directly ordered/auto-threatened.

- [ ] Cavalry charge vulnerability.
  - Check existing cavalry/elephant attack systems.
  - If there is no charge system yet, defer this to balance pass and document it.

- [ ] Tests.
  - Fireball deals increased damage to undead.
  - Holy/priest damage deals increased damage to undead.
  - Normal arrows/swords do baseline damage.
  - Morale damage does nothing to undead.

## Phase 11: Tooling And Validation

- [ ] Update content validator.
  - `tools/content_validator/main.cpp` should accept:
    - `iron_sepulcher`
    - undead troop IDs
    - non-playable/no-economy nation metadata
    - `primary_building: "none"`
    - `undead_zones`

- [ ] Update map editor.
  - `tools/map_editor/` should expose undead zones if map JSON supports them.
  - At minimum, it must preserve unknown `undead_zones` data instead of deleting it on save.

- [ ] Update arena tool.
  - `tools/arena/unit_spawn_options.h`
  - Add undead units for debug spawning.
  - Mark them as Iron Sepulcher units by default.

- [ ] Tests to run before considering complete:

```bash
make test
```

If the project does not use `make test` in the local environment, run the relevant CTest target from the build directory.

## Phase 12: Minimum Vertical Slice

The first playable slice should be intentionally small:

- [ ] Add `NationID::IronSepulcher`.
- [ ] Add three troop/spawn types.
- [ ] Add `assets/data/nations/iron_sepulcher.json`.
- [ ] Add base catalog entries.
- [ ] Add factories using simple copied unit behavior.
- [ ] Add `UndeadComponent` with morale immunity.
- [ ] Add one test map or mission with:
  - one `magic_shrine`
  - one `undead_zone`
  - 3 Skeleton Swordsmen
  - 2 Skeleton Archers
  - 1 Grave Priest
- [ ] AI behavior: hold/guard shrine only, no production, no building.
- [ ] Cursed volley and fireball can be placeholders in slice 1, but Grave Priest must at least heal undead.

Acceptance for slice 1:

- Rome/Carthage selection still works.
- Iron Sepulcher is not selectable by player UI.
- Map can spawn Iron Sepulcher units.
- Undead units do not route.
- Iron Sepulcher AI issues no economy/build commands.
- Undead guard a shrine/ruin area and attack living units entering it.
- Save/load does not lose undead unit nation IDs.

## Balance Targets

- Skeleton Swordsman:
  - Low damage, low speed, weak one-on-one.
  - Threat comes from numbers and morale immunity.
  - Should lose to disciplined Roman infantry at equal population unless supported.

- Skeleton Archer:
  - Low accuracy/low damage feel.
  - High pressure from volume and cursed morale later.
  - Dangerous from ruins, weak if caught in melee.

- Grave Priest:
  - Highest priority target.
  - Heals undead.
  - Uses fire magic.
  - Fragile if reached by cavalry or focused archers.

## Important Non-Goals

- Do not add Iron Sepulcher to normal skirmish nation selection.
- Do not give Iron Sepulcher builders, civilians, homes, barracks, farms, resource gathering, or capture economy.
- Do not make Iron Sepulcher a symmetric Rome/Carthage opponent.
- Do not let undead use normal morale/routing.
- Do not require new final art before the gameplay slice works.
