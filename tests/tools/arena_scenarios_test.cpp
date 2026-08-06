#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "game/map/map_definition.h"
#include "game/systems/wall_network_service.h"
#include "game/units/spawn_type.h"
#include "tools/arena/arena_scenario.h"
#include "tools/arena/arena_scenarios.h"

TEST(ArenaScenariosTest, ListsAllPhaseOneScenarioIds) {
  std::vector<QString> ids;
  ids.reserve(Arena::Scenarios::options().size());
  for (const auto& option : Arena::Scenarios::options()) {
    ids.push_back(option.id);
  }

  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_sword_duel_id)),
            ids.end());
  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_spear_duel_id)),
            ids.end());
  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_bow_exchange_id)),
            ids.end());
  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_held_weapon_stances_id)),
            ids.end());
  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_mounted_charge_id)),
            ids.end());
  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_braced_spear_charge_id)),
            ids.end());
  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_archer_melee_lock_id)),
            ids.end());
  for (auto const* transition_id :
       {Arena::Scenarios::k_archer_action_transition_id,
        Arena::Scenarios::k_swordsman_action_transition_id,
        Arena::Scenarios::k_spearman_action_transition_id,
        Arena::Scenarios::k_horse_archer_action_transition_id,
        Arena::Scenarios::k_mounted_knight_action_transition_id,
        Arena::Scenarios::k_horse_spearman_action_transition_id}) {
    EXPECT_NE(std::find(ids.begin(), ids.end(), QString::fromLatin1(transition_id)),
              ids.end());
  }
  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_melee_lock_id)),
            ids.end());
  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_chase_to_attack_id)),
            ids.end());
  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_attack_to_chase_id)),
            ids.end());
  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_target_death_id)),
            ids.end());
  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_retargeting_id)),
            ids.end());
  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_hold_guard_exit_id)),
            ids.end());
  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_lod_switch_id)),
            ids.end());
  for (auto const* settlement_id :
       {Arena::Scenarios::k_roman_marching_camp_id,
        Arena::Scenarios::k_carthage_trade_town_id,
        Arena::Scenarios::k_architecture_and_props_showcase_id,
        Arena::Scenarios::k_roman_fortification_showcase_id,
        Arena::Scenarios::k_carthage_fortification_showcase_id,
        Arena::Scenarios::k_rival_economies_id}) {
    EXPECT_NE(std::find(ids.begin(), ids.end(), QString::fromLatin1(settlement_id)),
              ids.end());
  }
  for (auto const* commander_id :
       {Arena::Scenarios::k_commander_aura_pulse_id,
        Arena::Scenarios::k_commander_identity_lineup_id,
        Arena::Scenarios::k_commander_sword_duel_id,
        Arena::Scenarios::k_commander_bow_duel_id,
        Arena::Scenarios::k_commander_spear_duel_id,
        Arena::Scenarios::k_commander_signature_spear_vs_sword_id,
        Arena::Scenarios::k_commander_signature_sword_vs_bow_id,
        Arena::Scenarios::k_commander_signature_bow_vs_spear_id}) {
    EXPECT_NE(std::find(ids.begin(), ids.end(), QString::fromLatin1(commander_id)),
              ids.end());
  }
  for (auto const* path_id : {Arena::Scenarios::k_path_bridge_crossing_id,
                              Arena::Scenarios::k_path_uphill_advance_id,
                              Arena::Scenarios::k_path_wall_detour_id,
                              Arena::Scenarios::k_path_wall_breach_id}) {
    EXPECT_NE(std::find(ids.begin(), ids.end(), QString::fromLatin1(path_id)),
              ids.end());
  }
  for (auto const* gate_id : {Arena::Scenarios::k_gate_friendly_passage_id,
                              Arena::Scenarios::k_gate_allied_access_id,
                              Arena::Scenarios::k_gate_enemy_blocked_id,
                              Arena::Scenarios::k_gate_destroyed_breach_id,
                              Arena::Scenarios::k_gate_consecutive_transit_id}) {
    EXPECT_NE(std::find(ids.begin(), ids.end(), QString::fromLatin1(gate_id)),
              ids.end());
  }
  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_water_showcase_id)),
            ids.end());
  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_wall_corner_showcase_id)),
            ids.end());
  for (auto const* lighting_id : {Arena::Scenarios::k_lighting_sunrise_sunset_id,
                                  Arena::Scenarios::k_lighting_sunset_id,
                                  Arena::Scenarios::k_lighting_moonlit_night_id,
                                  Arena::Scenarios::k_lighting_heavy_rain_id,
                                  Arena::Scenarios::k_lighting_dense_battle_id,
                                  Arena::Scenarios::k_lighting_world_materials_id}) {
    EXPECT_NE(std::find(ids.begin(), ids.end(), QString::fromLatin1(lighting_id)),
              ids.end());
  }
}

TEST(ArenaScenariosTest, ResolvesDescriptionsForKnownScenarios) {
  const auto* scenario = Arena::Scenarios::find_option(
      QString::fromLatin1(Arena::Scenarios::k_lod_switch_id));
  ASSERT_NE(scenario, nullptr);
  EXPECT_EQ(scenario->label, QStringLiteral("LOD Switch"));
  EXPECT_TRUE(
      scenario->description.contains(QStringLiteral("camera"), Qt::CaseInsensitive));
}

TEST(ArenaScenariosTest,
     CampaignScaleBattleMatchesCannaePopulationAndUsesProductionLod) {
  auto const* scenario = Arena::Scenarios::find_definition(
      QString::fromLatin1(Arena::Scenarios::k_campaign_scale_battle_id));
  ASSERT_NE(scenario, nullptr);

  int unit_count = 0;
  int individual_count = 0;
  for (auto const& group : scenario->groups) {
    unit_count += group.count;
    individual_count += group.count * group.individuals_per_unit;
  }

  EXPECT_EQ(unit_count, 79);
  EXPECT_EQ(individual_count, 1264);
  EXPECT_FALSE(scenario->force_full_creature_lod);
  EXPECT_FALSE(scenario->collect_animation_diagnostics);
  EXPECT_NE(std::find_if(scenario->expectations.begin(),
                         scenario->expectations.end(),
                         [](auto const& item) {
                           return item.kind ==
                                      Arena::ArenaExpectationKind::FrameBudget &&
                                  item.threshold <= 10.0F;
                         }),
            scenario->expectations.end());
}

TEST(ArenaScenariosTest, PerformanceBattlesHaveExactArmySizesAndOver100FpsBudget) {
  struct ExpectedScenario {
    const char* id;
    int units_per_side;
  };
  for (auto const expected :
       {ExpectedScenario{Arena::Scenarios::k_performance_20v20_id, 20},
        ExpectedScenario{Arena::Scenarios::k_performance_30v30_id, 30}}) {
    auto const* scenario =
        Arena::Scenarios::find_definition(QString::fromLatin1(expected.id));
    ASSERT_NE(scenario, nullptr);

    std::map<int, int> unit_count_by_owner;
    std::map<int, int> soldier_count_by_owner;
    for (auto const& group : scenario->groups) {
      unit_count_by_owner[group.owner_id] += group.count;
      soldier_count_by_owner[group.owner_id] +=
          group.count * group.individuals_per_unit;
    }

    EXPECT_EQ(unit_count_by_owner[1], expected.units_per_side);
    EXPECT_EQ(unit_count_by_owner[2], expected.units_per_side);
    EXPECT_EQ(soldier_count_by_owner[1], expected.units_per_side);
    EXPECT_EQ(soldier_count_by_owner[2], expected.units_per_side);
    EXPECT_TRUE(scenario->force_full_creature_lod);
    EXPECT_TRUE(scenario->require_rigged_instancing);
    EXPECT_EQ(scenario->graphics_quality, Render::GraphicsQuality::Ultra);
    EXPECT_FALSE(scenario->collect_animation_diagnostics);

    auto const budget =
        std::find_if(scenario->expectations.begin(),
                     scenario->expectations.end(),
                     [](auto const& item) {
                       return item.kind == Arena::ArenaExpectationKind::FrameBudget;
                     });
    ASSERT_NE(budget, scenario->expectations.end());
    EXPECT_LT(budget->threshold, 10.0F);
    EXPECT_GE(budget->start_seconds, 2.0F);
  }
}

TEST(ArenaScenariosTest, WallGroupsSitOnTheWallNetworkLattice) {

  constexpr int k_spacing = Game::Systems::WallNetworkService::k_segment_spacing;

  for (const auto& scenario : Arena::Scenarios::definitions()) {
    for (const auto& group : scenario.groups) {
      if (!group.spawn_type.has_value() ||
          !Game::Units::is_wall_network_spawn(*group.spawn_type)) {
        continue;
      }

      const auto context = [&](int index) {
        return (scenario.id + QStringLiteral("/") + group.name +
                QStringLiteral("[%1]").arg(index))
            .toStdString();
      };
      const float center = (static_cast<float>(group.count) - 1.0F) * 0.5F;

      for (int index = 0; index < group.count; ++index) {
        const QVector3D position =
            group.origin + (group.spacing * (static_cast<float>(index) - center));
        const int grid_x = static_cast<int>(std::lround(position.x()));
        const int grid_z = static_cast<int>(std::lround(position.z()));

        EXPECT_NEAR(position.x(), static_cast<float>(grid_x), 0.01F) << context(index);
        EXPECT_NEAR(position.z(), static_cast<float>(grid_z), 0.01F) << context(index);
        EXPECT_EQ(grid_x % k_spacing, 0) << context(index);
        EXPECT_EQ(grid_z % k_spacing, 0) << context(index);
      }

      if (group.count > 1) {
        const float step = group.spacing.length();
        EXPECT_NEAR(step, static_cast<float>(k_spacing), 0.01F)
            << context(0) << " members must land on neighbouring cells";
      }
    }
  }
}

TEST(ArenaScenariosTest, WallCornerShowcaseCoversEveryJoinShape) {
  using Game::Systems::WallNetworkService;

  auto const* scenario = Arena::Scenarios::find_definition(
      QString::fromLatin1(Arena::Scenarios::k_wall_corner_showcase_id));
  ASSERT_NE(scenario, nullptr);

  std::map<int, WallNetworkService::OccupancySet> occupancy;
  std::map<int, std::vector<std::pair<int, int>>> cells;
  for (const auto& group : scenario->groups) {
    if (!group.spawn_type.has_value() ||
        *group.spawn_type != Game::Units::SpawnType::WallSegment) {
      continue;
    }
    const float center = (static_cast<float>(group.count) - 1.0F) * 0.5F;
    for (int index = 0; index < group.count; ++index) {
      const QVector3D position =
          group.origin + (group.spacing * (static_cast<float>(index) - center));
      const int grid_x = static_cast<int>(std::lround(position.x()));
      const int grid_z = static_cast<int>(std::lround(position.z()));
      EXPECT_TRUE(occupancy[group.owner_id]
                      .insert(WallNetworkService::encode_key(grid_x, grid_z))
                      .second)
          << "two segments share cell " << grid_x << "," << grid_z;
      cells[group.owner_id].emplace_back(grid_x, grid_z);
    }
  }
  ASSERT_FALSE(cells.empty());

  for (const auto& [owner_id, owner_cells] : cells) {
    std::set<std::string> shapes;
    for (const auto& [grid_x, grid_z] : owner_cells) {
      const auto mask = WallNetworkService::compute_connection_mask(
          occupancy[owner_id], grid_x, grid_z);
      shapes.insert(WallNetworkService::resolve_appearance(
                        Game::Systems::NationID::RomanRepublic, mask)
                        .renderer_id);
    }

    for (const auto* shape : {"wall_segment_isolated",
                              "wall_segment_end",
                              "wall_segment_straight",
                              "wall_segment_corner",
                              "wall_segment_tee",
                              "wall_segment_cross"}) {
      EXPECT_NE(std::find_if(shapes.begin(),
                             shapes.end(),
                             [shape](const std::string& id) {
                               return id.find(shape) != std::string::npos;
                             }),
                shapes.end())
          << "owner " << owner_id << " never produces " << shape;
    }
  }
}

TEST(ArenaScenariosTest, ListsEveryIronSepulcherScenario) {
  for (auto const* id : {Arena::Scenarios::k_sepulcher_roster_lineup_id,
                         Arena::Scenarios::k_sepulcher_spell_fx_showcase_id,
                         Arena::Scenarios::k_sepulcher_vs_rome_infantry_id,
                         Arena::Scenarios::k_sepulcher_vs_rome_ranged_id,
                         Arena::Scenarios::k_sepulcher_vs_carthage_infantry_id,
                         Arena::Scenarios::k_sepulcher_vs_carthage_cavalry_id,
                         Arena::Scenarios::k_sepulcher_shrine_awakening_id,
                         Arena::Scenarios::k_sepulcher_ruins_awakening_waves_id,
                         Arena::Scenarios::k_sepulcher_shrine_siege_id,
                         Arena::Scenarios::k_sepulcher_zone_shrine_spawn_id,
                         Arena::Scenarios::k_sepulcher_twin_zone_shrines_id,
                         Arena::Scenarios::k_sepulcher_shrine_demolition_id,
                         Arena::Scenarios::k_sepulcher_shrine_state_reload_id,
                         Arena::Scenarios::k_sepulcher_fireball_review_id}) {
    EXPECT_NE(Arena::Scenarios::find_option(QString::fromLatin1(id)), nullptr) << id;
    auto const* scenario = Arena::Scenarios::find_definition(QString::fromLatin1(id));
    ASSERT_NE(scenario, nullptr) << id;
    EXPECT_FALSE(scenario->expectations.empty()) << id;
  }
}

TEST(ArenaScenariosTest, SepulcherRosterLineupCoversEveryUndeadTroop) {
  auto const* scenario = Arena::Scenarios::find_definition(
      QString::fromLatin1(Arena::Scenarios::k_sepulcher_roster_lineup_id));
  ASSERT_NE(scenario, nullptr);

  std::set<Game::Units::TroopType> sepulcher_troops;
  for (auto const& group : scenario->groups) {
    if (group.nation_id == Game::Systems::NationID::IronSepulcher) {
      sepulcher_troops.insert(group.troop_type);
    }
  }

  EXPECT_EQ(sepulcher_troops,
            (std::set<Game::Units::TroopType>{Game::Units::TroopType::SkeletonSwordsman,
                                              Game::Units::TroopType::SkeletonArcher,
                                              Game::Units::TroopType::GravePriest}));
}

TEST(ArenaScenariosTest, SepulcherBattlesPitUndeadAgainstBothPlayableNations) {
  const std::map<const char*, Game::Systems::NationID> battles{
      {Arena::Scenarios::k_sepulcher_vs_rome_infantry_id,
       Game::Systems::NationID::RomanRepublic},
      {Arena::Scenarios::k_sepulcher_vs_rome_ranged_id,
       Game::Systems::NationID::RomanRepublic},
      {Arena::Scenarios::k_sepulcher_vs_carthage_infantry_id,
       Game::Systems::NationID::Carthage},
      {Arena::Scenarios::k_sepulcher_vs_carthage_cavalry_id,
       Game::Systems::NationID::Carthage}};

  for (auto const& [id, opponent_nation] : battles) {
    auto const* scenario = Arena::Scenarios::find_definition(QString::fromLatin1(id));
    ASSERT_NE(scenario, nullptr) << id;

    bool has_undead = false;
    bool has_opponent = false;
    for (auto const& group : scenario->groups) {
      if (group.nation_id == Game::Systems::NationID::IronSepulcher) {
        has_undead = true;
        EXPECT_NE(group.owner_id, 1) << id << " undead must not own the player slot";
      } else if (group.nation_id == opponent_nation) {
        has_opponent = true;
      }
    }
    EXPECT_TRUE(has_undead) << id;
    EXPECT_TRUE(has_opponent) << id;
    EXPECT_FALSE(scenario->steps.empty()) << id;
  }
}

TEST(ArenaScenariosTest, FireballReviewSceneIsolatesTheSpellFromEverythingElse) {
  auto const* scenario = Arena::Scenarios::find_definition(
      QString::fromLatin1(Arena::Scenarios::k_sepulcher_fireball_review_id));
  ASSERT_NE(scenario, nullptr);
  ASSERT_EQ(scenario->groups.size(), 2U);

  EXPECT_TRUE(scenario->undead_zones.empty()) << "no zone garrison in the FX bench";
  EXPECT_TRUE(scenario->suppress_terrain_scatter);
  EXPECT_TRUE(scenario->suppress_ui_overlays);

  auto const& target = scenario->groups.back();
  EXPECT_GE(target.health_override, 10000)
      << "the target has to survive the whole review without burning down";

  EXPECT_NE(std::find_if(scenario->expectations.begin(),
                         scenario->expectations.end(),
                         [](auto const& item) {
                           return item.kind ==
                                  Arena::ArenaExpectationKind::ProjectileFlightObserved;
                         }),
            scenario->expectations.end());
  EXPECT_NE(std::find_if(scenario->expectations.begin(),
                         scenario->expectations.end(),
                         [](auto const& item) {
                           return item.kind ==
                                  Arena::ArenaExpectationKind::ProjectileImpactObserved;
                         }),
            scenario->expectations.end());
}

TEST(ArenaScenariosTest, SepulcherZonesCarryTheirOwnHazeAndAnchorRole) {
  auto const* shrine = Arena::Scenarios::find_definition(
      QString::fromLatin1(Arena::Scenarios::k_sepulcher_shrine_awakening_id));
  ASSERT_NE(shrine, nullptr);
  ASSERT_EQ(shrine->undead_zones.size(), 1U);

  auto const& shrine_zone = shrine->undead_zones.front();
  EXPECT_GT(shrine_zone.fog_density, 0.0F);
  EXPECT_EQ(shrine_zone.anchor_type, Game::Map::WorldProp::Type::MagicShrine);
  EXPECT_TRUE(shrine_zone.waves.empty())
      << "the shrine scene exercises the default garrison";

  auto const* ruins = Arena::Scenarios::find_definition(
      QString::fromLatin1(Arena::Scenarios::k_sepulcher_ruins_awakening_waves_id));
  ASSERT_NE(ruins, nullptr);
  ASSERT_EQ(ruins->undead_zones.size(), 1U);
  EXPECT_EQ(ruins->undead_zones.front().anchor_type, Game::Map::WorldProp::Type::Ruins)
      << "the ruins keep their own anchor prop, next to the zone's shrine";
}

TEST(ArenaScenariosTest, SignatureDuelsPairCommandersAcrossWeaponsAndLastLongEnough) {
  for (auto const* id : {Arena::Scenarios::k_commander_signature_spear_vs_sword_id,
                         Arena::Scenarios::k_commander_signature_sword_vs_bow_id,
                         Arena::Scenarios::k_commander_signature_bow_vs_spear_id}) {
    auto const* scenario = Arena::Scenarios::find_definition(QString::fromLatin1(id));
    ASSERT_NE(scenario, nullptr) << id;
    ASSERT_EQ(scenario->groups.size(), 2U) << id;

    auto const& roman = scenario->groups.front();
    auto const& carthaginian = scenario->groups.back();
    EXPECT_NE(roman.troop_type, carthaginian.troop_type) << id;
    EXPECT_NE(roman.owner_id, carthaginian.owner_id) << id;

    EXPECT_GE(scenario->duration_seconds, 20.0F) << id;
    EXPECT_GE(roman.health_override, 5000) << id;
    EXPECT_GE(carthaginian.health_override, 5000) << id;
  }
}

TEST(ArenaScenariosTest, ShrineScenesCoverSpawnMultipleZonesDestructionAndReload) {
  auto const zone_expectation_kinds = [](const Arena::ArenaScenarioDefinition& scenario,
                                         const QString& zone_id) {
    std::set<Arena::ArenaExpectationKind> kinds;
    for (auto const& item : scenario.expectations) {
      if (item.zone_id == zone_id) {
        kinds.insert(item.kind);
      }
    }
    return kinds;
  };

  auto const* spawn = Arena::Scenarios::find_definition(
      QString::fromLatin1(Arena::Scenarios::k_sepulcher_zone_shrine_spawn_id));
  ASSERT_NE(spawn, nullptr);
  ASSERT_EQ(spawn->undead_zones.size(), 1U);
  EXPECT_TRUE(spawn->resource_patches.empty())
      << "the zone has to raise its own shrine, not borrow an authored prop";
  EXPECT_TRUE(zone_expectation_kinds(*spawn, spawn->undead_zones.front().id)
                  .contains(Arena::ArenaExpectationKind::UndeadZoneShrineStands));

  auto const* twins = Arena::Scenarios::find_definition(
      QString::fromLatin1(Arena::Scenarios::k_sepulcher_twin_zone_shrines_id));
  ASSERT_NE(twins, nullptr);
  ASSERT_EQ(twins->undead_zones.size(), 2U);
  for (auto const& zone : twins->undead_zones) {
    EXPECT_TRUE(zone_expectation_kinds(*twins, zone.id)
                    .contains(Arena::ArenaExpectationKind::UndeadZoneShrineStands))
        << zone.id.toStdString();
  }

  auto const* demolition = Arena::Scenarios::find_definition(
      QString::fromLatin1(Arena::Scenarios::k_sepulcher_shrine_demolition_id));
  ASSERT_NE(demolition, nullptr);
  ASSERT_EQ(demolition->undead_zones.size(), 1U);
  auto const demolition_kinds =
      zone_expectation_kinds(*demolition, demolition->undead_zones.front().id);
  EXPECT_TRUE(demolition_kinds.contains(
      Arena::ArenaExpectationKind::UndeadZoneShrineDestroyed));
  EXPECT_TRUE(
      demolition_kinds.contains(Arena::ArenaExpectationKind::UndeadZoneCleared));
  EXPECT_NE(std::find_if(demolition->steps.begin(),
                         demolition->steps.end(),
                         [&](auto const& step) {
                           return step.zone_id == demolition->undead_zones.front().id &&
                                  step.command ==
                                      Arena::ScenarioCommandKind::ApplyDamage;
                         }),
            demolition->steps.end());

  auto const* reload = Arena::Scenarios::find_definition(
      QString::fromLatin1(Arena::Scenarios::k_sepulcher_shrine_state_reload_id));
  ASSERT_NE(reload, nullptr);
  ASSERT_EQ(reload->undead_zones.size(), 1U);
  EXPECT_TRUE(zone_expectation_kinds(*reload, reload->undead_zones.front().id)
                  .contains(Arena::ArenaExpectationKind::UndeadZoneShrineStands));
  EXPECT_NE(std::find_if(reload->steps.begin(),
                         reload->steps.end(),
                         [](auto const& step) {
                           return step.command ==
                                  Arena::ScenarioCommandKind::ReloadUndeadZoneState;
                         }),
            reload->steps.end());
}

TEST(ArenaScenariosTest, ShrineSiegeRequiresTheZoneToBeClearedByLosingItsAnchor) {
  auto const* scenario = Arena::Scenarios::find_definition(
      QString::fromLatin1(Arena::Scenarios::k_sepulcher_shrine_siege_id));
  ASSERT_NE(scenario, nullptr);
  ASSERT_EQ(scenario->undead_zones.size(), 1U);

  EXPECT_NE(std::find_if(scenario->expectations.begin(),
                         scenario->expectations.end(),
                         [](auto const& item) {
                           return item.kind ==
                                  Arena::ArenaExpectationKind::UndeadZoneCleared;
                         }),
            scenario->expectations.end());
}

TEST(ArenaScenariosTest, AwakeningScenariosStayDormantUntilIntrudersArrive) {
  for (auto const* id : {Arena::Scenarios::k_sepulcher_shrine_awakening_id,
                         Arena::Scenarios::k_sepulcher_ruins_awakening_waves_id}) {
    auto const* scenario = Arena::Scenarios::find_definition(QString::fromLatin1(id));
    ASSERT_NE(scenario, nullptr) << id;

    ASSERT_EQ(scenario->undead_zones.size(), 1U) << id;
    auto const& zone = scenario->undead_zones.front();
    EXPECT_NE(std::find(zone.awaken_on.begin(),
                        zone.awaken_on.end(),
                        QStringLiteral("unit_enters_radius")),
              zone.awaken_on.end())
        << id;

    ASSERT_FALSE(scenario->resource_patches.empty()) << id;
    EXPECT_FLOAT_EQ(scenario->resource_patches.front().origin.x(), zone.x) << id;
    EXPECT_FLOAT_EQ(scenario->resource_patches.front().origin.z(), zone.z) << id;

    bool const no_group_owns_the_zone = std::none_of(
        scenario->groups.begin(), scenario->groups.end(), [&zone](auto const& group) {
          return group.owner_id == zone.owner_id;
        });
    EXPECT_TRUE(no_group_owns_the_zone)
        << id << " guardians must come from the awakening system, not authored groups";

    auto const dormant = std::find_if(
        scenario->expectations.begin(),
        scenario->expectations.end(),
        [&zone](auto const& item) {
          return item.kind == Arena::ArenaExpectationKind::UndeadZoneDormantBefore &&
                 item.zone_id == zone.id;
        });
    ASSERT_NE(dormant, scenario->expectations.end()) << id;
    EXPECT_GT(dormant->end_seconds, 0.0F) << id;

    EXPECT_NE(
        std::find_if(scenario->expectations.begin(),
                     scenario->expectations.end(),
                     [&zone](auto const& item) {
                       return item.kind ==
                                  Arena::ArenaExpectationKind::UndeadZoneAwakened &&
                              item.zone_id == zone.id;
                     }),
        scenario->expectations.end())
        << id;
  }
}

TEST(ArenaScenariosTest,
     RuinsAwakeningReleasesAFollowUpWaveOnlyAfterTheFirstIsCleared) {
  auto const* scenario = Arena::Scenarios::find_definition(
      QString::fromLatin1(Arena::Scenarios::k_sepulcher_ruins_awakening_waves_id));
  ASSERT_NE(scenario, nullptr);
  ASSERT_EQ(scenario->undead_zones.size(), 1U);

  auto const& waves = scenario->undead_zones.front().waves;
  ASSERT_EQ(waves.size(), 2U);
  EXPECT_EQ(waves[0].trigger, QStringLiteral("initial"));
  EXPECT_EQ(waves[1].trigger, QStringLiteral("after_clear"));
}

TEST(ArenaScenariosTest, RejectsUnknownScenarioIds) {
  EXPECT_EQ(Arena::Scenarios::find_option(QStringLiteral("not_a_real_scenario")),
            nullptr);
}

TEST(ArenaScenariosTest, PathfindingShowcasesCarryExecutableTerrainContracts) {
  auto const* bridge = Arena::Scenarios::find_definition(
      QString::fromLatin1(Arena::Scenarios::k_path_bridge_crossing_id));
  ASSERT_NE(bridge, nullptr);
  EXPECT_FALSE(bridge->rivers.empty());
  EXPECT_FALSE(bridge->bridges.empty());
  EXPECT_NE(std::find_if(bridge->expectations.begin(),
                         bridge->expectations.end(),
                         [](auto const& item) {
                           return item.kind ==
                                  Arena::ArenaExpectationKind::BridgeTraversalObserved;
                         }),
            bridge->expectations.end());
  EXPECT_NE(std::find_if(bridge->expectations.begin(),
                         bridge->expectations.end(),
                         [](auto const& item) {
                           return item.kind ==
                                  Arena::ArenaExpectationKind::BridgeCenterlineAligned;
                         }),
            bridge->expectations.end());

  auto const* hill = Arena::Scenarios::find_definition(
      QString::fromLatin1(Arena::Scenarios::k_path_uphill_advance_id));
  ASSERT_NE(hill, nullptr);
  EXPECT_FALSE(hill->elevation_patches.empty());
  EXPECT_NE(std::find_if(hill->expectations.begin(),
                         hill->expectations.end(),
                         [](auto const& item) {
                           return item.kind ==
                                  Arena::ArenaExpectationKind::ElevationGainObserved;
                         }),
            hill->expectations.end());

  auto const* breach = Arena::Scenarios::find_definition(
      QString::fromLatin1(Arena::Scenarios::k_path_wall_breach_id));
  ASSERT_NE(breach, nullptr);
  EXPECT_NE(std::find_if(breach->expectations.begin(),
                         breach->expectations.end(),
                         [](auto const& item) {
                           return item.kind ==
                                  Arena::ArenaExpectationKind::GroupDestroyed;
                         }),
            breach->expectations.end());
}

namespace {

auto has_expectation(const Arena::ArenaScenarioDefinition& scenario,
                     Arena::ArenaExpectationKind kind,
                     const QString& group = {},
                     const QString& target_group = {}) -> bool {
  return std::any_of(
      scenario.expectations.begin(),
      scenario.expectations.end(),
      [&](auto const& item) {
        return item.kind == kind && (group.isEmpty() || item.group == group) &&
               (target_group.isEmpty() || item.target_group == target_group);
      });
}

} // namespace

TEST(ArenaScenariosTest, FormationScenariosCoverBothLayers) {
  const QString unit_layout_ids[] = {QStringLiteral("unit_layout_role_lineup"),
                                     QStringLiteral("unit_layout_faction_comparison"),
                                     QStringLiteral("unit_layout_size_range"),
                                     QStringLiteral("unit_layout_move_and_rotate"),
                                     QStringLiteral("unit_layout_casualty_reflow"),
                                     QStringLiteral("unit_layout_slope_adaptation"),
                                     QStringLiteral("unit_layout_defensive_transition"),
                                     QStringLiteral("unit_layout_disruption_recovery"),
                                     QStringLiteral("unit_layout_large_army_cost")};
  const QString army_formation_ids[] = {
      QStringLiteral("army_formation_rome_default"),
      QStringLiteral("army_formation_carthage_default"),
      QStringLiteral("army_formation_rome_vs_carthage"),
      QStringLiteral("army_formation_sepulcher_shrine_defence"),
      QStringLiteral("army_formation_mixed_contingents"),
      QStringLiteral("army_formation_narrow_gate"),
      QStringLiteral("army_formation_around_buildings"),
      QStringLiteral("army_formation_member_losses"),
      QStringLiteral("army_formation_maintain_advance"),
      QStringLiteral("army_formation_siege_escort")};

  for (const auto& id : unit_layout_ids) {
    auto const* scenario = Arena::Scenarios::find_definition(id);
    ASSERT_NE(scenario, nullptr) << id.toStdString();
    EXPECT_FALSE(scenario->groups.empty()) << id.toStdString();
    EXPECT_FALSE(scenario->expectations.empty()) << id.toStdString();
  }
  for (const auto& id : army_formation_ids) {
    auto const* scenario = Arena::Scenarios::find_definition(id);
    ASSERT_NE(scenario, nullptr) << id.toStdString();
    EXPECT_FALSE(scenario->groups.empty()) << id.toStdString();
    EXPECT_FALSE(scenario->expectations.empty()) << id.toStdString();
  }
}

TEST(ArenaScenariosTest, FormationTerrainScenariosCarryExecutableContracts) {
  auto const* crossing = Arena::Scenarios::find_definition(
      QStringLiteral("army_formation_bridge_crossing"));
  ASSERT_NE(crossing, nullptr);
  EXPECT_FALSE(crossing->rivers.empty());
  EXPECT_FALSE(crossing->bridges.empty());
  EXPECT_TRUE(has_expectation(*crossing,
                              Arena::ArenaExpectationKind::BridgeTraversalObserved,
                              QStringLiteral("vanguard")));
  EXPECT_TRUE(has_expectation(*crossing,
                              Arena::ArenaExpectationKind::GroupReachedDestination,
                              QStringLiteral("vanguard")));

  auto const* hill = Arena::Scenarios::find_definition(
      QStringLiteral("army_formation_hill_deployment"));
  ASSERT_NE(hill, nullptr);
  EXPECT_FALSE(hill->elevation_patches.empty());
  EXPECT_TRUE(has_expectation(*hill,
                              Arena::ArenaExpectationKind::ElevationGainObserved,
                              QStringLiteral("climbers")));

  auto const* course = Arena::Scenarios::find_definition(
      QStringLiteral("army_formation_obstacle_course"));
  ASSERT_NE(course, nullptr);
  EXPECT_FALSE(course->rivers.empty());
  EXPECT_FALSE(course->bridges.empty());
  EXPECT_FALSE(course->elevation_patches.empty());
  EXPECT_TRUE(
      std::any_of(course->groups.begin(), course->groups.end(), [](auto const& group) {
        return group.spawn_type == Game::Units::SpawnType::WallSegment;
      }));
  EXPECT_TRUE(
      std::any_of(course->groups.begin(), course->groups.end(), [](auto const& group) {
        return group.spawn_type == Game::Units::SpawnType::Barracks;
      }));
}

TEST(ArenaScenariosTest, ArmyFormationScenariosIssueFormationMoves) {
  for (const auto& option : Arena::Scenarios::options()) {
    if (!option.id.startsWith(QStringLiteral("army_formation_"))) {
      continue;
    }
    auto const* scenario = Arena::Scenarios::find_definition(option.id);
    ASSERT_NE(scenario, nullptr) << option.id.toStdString();
    EXPECT_TRUE(std::any_of(scenario->steps.begin(),
                            scenario->steps.end(),
                            [](auto const& step) {
                              return step.command ==
                                     Arena::ScenarioCommandKind::FormationMove;
                            }))
        << option.id.toStdString() << " never issues a FormationMove";
  }
}

TEST(ArenaScenariosTest, FlamingSiegeScenarioSeparatesFireFromMeleeDamage) {
  auto const* scenario = Arena::Scenarios::find_definition(
      QString::fromLatin1(Arena::Scenarios::k_structure_flaming_siege_id));
  ASSERT_NE(scenario, nullptr);
  EXPECT_TRUE(Arena::validate_scenario(*scenario).empty());

  using Kind = Arena::ArenaExpectationKind;
  EXPECT_TRUE(has_expectation(*scenario,
                              Kind::FlamingProjectileObserved,
                              QStringLiteral("fire_catapult"),
                              QStringLiteral("burning_home")));
  EXPECT_TRUE(has_expectation(
      *scenario, Kind::StructureFireObserved, QStringLiteral("burning_home")));
  EXPECT_TRUE(has_expectation(
      *scenario, Kind::NoStructureFireObserved, QStringLiteral("chipped_wall")));
}

TEST(ArenaScenariosTest, MeleeDestructionScenarioDemandsAFlamelessCollapse) {
  auto const* scenario = Arena::Scenarios::find_definition(
      QString::fromLatin1(Arena::Scenarios::k_structure_melee_destruction_id));
  ASSERT_NE(scenario, nullptr);
  EXPECT_TRUE(Arena::validate_scenario(*scenario).empty());

  using Kind = Arena::ArenaExpectationKind;
  EXPECT_TRUE(
      has_expectation(*scenario, Kind::GroupDestroyed, QStringLiteral("doomed_wall")));
  EXPECT_TRUE(has_expectation(
      *scenario, Kind::NoStructureFireObserved, QStringLiteral("doomed_wall")));
  EXPECT_FALSE(has_expectation(*scenario, Kind::StructureFireObserved));
}

TEST(ArenaScenariosTest, RetargetScenarioChecksBothAmmunitionTypes) {
  auto const* scenario = Arena::Scenarios::find_definition(
      QString::fromLatin1(Arena::Scenarios::k_catapult_ammunition_retarget_id));
  ASSERT_NE(scenario, nullptr);
  EXPECT_TRUE(Arena::validate_scenario(*scenario).empty());

  using Kind = Arena::ArenaExpectationKind;
  EXPECT_TRUE(has_expectation(*scenario,
                              Kind::FlamingProjectileObserved,
                              QStringLiteral("swing_catapult"),
                              QStringLiteral("swing_barracks")));
  EXPECT_TRUE(has_expectation(*scenario,
                              Kind::NoFlamingProjectileObserved,
                              QStringLiteral("swing_catapult"),
                              QStringLiteral("swing_infantry")));

  auto const retarget = std::find_if(
      scenario->steps.begin(), scenario->steps.end(), [](auto const& step) {
        return step.target_group == QStringLiteral("swing_infantry") &&
               step.command == Arena::ScenarioCommandKind::Attack;
      });
  ASSERT_NE(retarget, scenario->steps.end());
  EXPECT_GT(retarget->trigger.time_seconds, 0.0F);
}

TEST(ArenaScenariosTest, ListsEverySettlementAndEconomyScenario) {
  for (auto const* settlement_id : {Arena::Scenarios::k_village_harvest_cycle_id,
                                    Arena::Scenarios::k_village_day_life_id,
                                    Arena::Scenarios::k_colony_founding_id,
                                    Arena::Scenarios::k_village_raid_id,
                                    Arena::Scenarios::k_frontier_outpost_id,
                                    Arena::Scenarios::k_riverside_mill_town_id,
                                    Arena::Scenarios::k_quarry_camp_id,
                                    Arena::Scenarios::k_trade_road_convoy_id}) {
    EXPECT_NE(Arena::Scenarios::find_definition(QString::fromLatin1(settlement_id)),
              nullptr)
        << settlement_id;
  }
}

TEST(ArenaScenariosTest, EachSettlementLaysItsStreetsInOneStyle) {

  for (const auto& scenario : Arena::Scenarios::definitions()) {

    const bool is_settlement =
        std::any_of(scenario.groups.begin(), scenario.groups.end(), [](auto const& g) {
          return g.spawn_type.has_value() &&
                 Game::Units::is_building_spawn(*g.spawn_type);
        });
    if (!is_settlement) {
      continue;
    }

    std::set<std::string> styles;
    for (const auto& road : scenario.roads) {
      styles.insert(road.style.trimmed().toLower().toStdString());
    }
    EXPECT_LE(styles.size(), 1U)
        << scenario.id.toStdString()
        << " mixes road styles; a settlement paves its streets one way";
  }
}

TEST(ArenaScenariosTest, InhabitedSettlementsProveTheirDailyLife) {

  for (const auto& scenario : Arena::Scenarios::definitions()) {
    std::set<QString> resident_groups;
    for (const auto& group : scenario.groups) {
      if (group.settlement_resident) {
        resident_groups.insert(group.name);
        EXPECT_GT(group.settlement_roam_radius, 0.0F)
            << scenario.id.toStdString() << "/" << group.name.toStdString();
      }
    }
    if (resident_groups.empty()) {
      continue;
    }

    const bool asserts_movement = std::any_of(
        scenario.expectations.begin(),
        scenario.expectations.end(),
        [&](const auto& item) {
          return item.kind == Arena::ArenaExpectationKind::MovementAnimationObserved &&
                 resident_groups.contains(item.group);
        });
    EXPECT_TRUE(asserts_movement)
        << scenario.id.toStdString()
        << " spawns settlement residents but never requires them to be seen "
           "going about the settlement";
  }
}

TEST(ArenaScenariosTest, RampartsCloseWithGatesAndCornerTowers) {
  struct WalledSettlement {
    const char* id;
    const char* prefix;
  };
  for (auto const& walled :
       {WalledSettlement{Arena::Scenarios::k_roman_marching_camp_id, "castrum"},
        WalledSettlement{Arena::Scenarios::k_carthage_trade_town_id, "punic"}}) {
    auto const* scenario =
        Arena::Scenarios::find_definition(QString::fromLatin1(walled.id));
    ASSERT_NE(scenario, nullptr) << walled.id;

    std::set<QString> names;
    for (const auto& group : scenario->groups) {
      names.insert(group.name);
    }
    const QString prefix = QString::fromLatin1(walled.prefix);
    for (auto const* suffix : {"_gate_north",
                               "_gate_south",
                               "_gate_west",
                               "_gate_east",
                               "_tower_nw",
                               "_tower_ne",
                               "_tower_sw",
                               "_tower_se"}) {
      EXPECT_TRUE(names.contains(prefix + QString::fromLatin1(suffix)))
          << walled.id << " is missing " << suffix;
    }
  }
}

TEST(ArenaScenariosTest, FormationPromoScenariosDriveTheArmyFormationLayer) {
  using Intent = Game::Formation::ArmyFormationIntent;
  for (auto const* id :
       {"promo_rome_iron_line", "promo_carthage_crescent", "promo_rome_hill_drill"}) {
    auto const* scenario = Arena::Scenarios::find_definition(QString::fromLatin1(id));
    ASSERT_NE(scenario, nullptr) << id;
    EXPECT_TRUE(Arena::validate_scenario(*scenario).empty()) << id;

    EXPECT_GT(scenario->arena_floor_half_extent, 30.0F)
        << id << " manoeuvres an army and needs more than a duelling floor";

    std::set<QString> group_names;
    int soldiers = 0;
    for (auto const& group : scenario->groups) {
      group_names.insert(group.name);
      soldiers += group.count * std::max(1, group.individuals_per_unit);
    }
    EXPECT_GE(soldiers, 300) << id << " does not field an army";

    std::set<Intent> intents;
    for (auto const& step : scenario->steps) {
      if (step.command != Arena::ScenarioCommandKind::FormArmy) {
        continue;
      }
      intents.insert(step.formation.intent);
      EXPECT_FALSE(step.formation.groups.isEmpty())
          << id << " step " << step.name.toStdString() << " forms nobody";
      for (auto const& member : step.formation.groups) {
        EXPECT_TRUE(group_names.contains(member))
            << id << " forms unknown group " << member.toStdString();
      }
    }
    EXPECT_GE(intents.size(), 3U)
        << id << " shows too little of the formation vocabulary";
  }
}
