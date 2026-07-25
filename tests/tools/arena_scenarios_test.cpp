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
        Arena::Scenarios::k_commander_consul_vs_broker_id,
        Arena::Scenarios::k_commander_field_vs_cavalry_id,
        Arena::Scenarios::k_commander_legion_vs_elephant_id}) {
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
  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_water_showcase_id)),
            ids.end());
  EXPECT_NE(std::find(ids.begin(),
                      ids.end(),
                      QString::fromLatin1(Arena::Scenarios::k_wall_corner_showcase_id)),
            ids.end());
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

TEST(ArenaScenariosTest, WallGroupsSitOnTheWallNetworkLattice) {

  constexpr int k_spacing = Game::Systems::WallNetworkService::k_segment_spacing;

  for (const auto& scenario : Arena::Scenarios::definitions()) {
    for (const auto& group : scenario.groups) {
      if (!group.spawn_type.has_value() ||
          *group.spawn_type != Game::Units::SpawnType::WallSegment) {
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
                         Arena::Scenarios::k_sepulcher_vs_rome_infantry_id,
                         Arena::Scenarios::k_sepulcher_vs_rome_ranged_id,
                         Arena::Scenarios::k_sepulcher_vs_carthage_infantry_id,
                         Arena::Scenarios::k_sepulcher_vs_carthage_cavalry_id,
                         Arena::Scenarios::k_sepulcher_shrine_awakening_id,
                         Arena::Scenarios::k_sepulcher_ruins_awakening_waves_id,
                         Arena::Scenarios::k_sepulcher_shrine_siege_id}) {
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

TEST(ArenaScenariosTest, SepulcherZonesCarryTheirOwnHazeAndAnchorRole) {
  auto const* shrine = Arena::Scenarios::find_definition(
      QString::fromLatin1(Arena::Scenarios::k_sepulcher_shrine_awakening_id));
  ASSERT_NE(shrine, nullptr);
  ASSERT_EQ(shrine->undead_zones.size(), 1U);

  auto const& shrine_zone = shrine->undead_zones.front();
  EXPECT_GT(shrine_zone.fog_density, 0.0F);
  EXPECT_TRUE(Game::Map::zone_has_structural_anchor(shrine_zone))
      << "a shrine garrisons a capturable sepulcher barracks";
  EXPECT_TRUE(shrine_zone.waves.empty())
      << "the shrine scene exercises the default garrison";

  auto const* ruins = Arena::Scenarios::find_definition(
      QString::fromLatin1(Arena::Scenarios::k_sepulcher_ruins_awakening_waves_id));
  ASSERT_NE(ruins, nullptr);
  ASSERT_EQ(ruins->undead_zones.size(), 1U);
  EXPECT_FALSE(Game::Map::zone_has_structural_anchor(ruins->undead_zones.front()))
      << "ruins stay decorative";
}

TEST(ArenaScenariosTest, ShrineSiegeRequiresTheZoneToBeClearedByLosingItsAnchor) {
  auto const* scenario = Arena::Scenarios::find_definition(
      QString::fromLatin1(Arena::Scenarios::k_sepulcher_shrine_siege_id));
  ASSERT_NE(scenario, nullptr);
  ASSERT_EQ(scenario->undead_zones.size(), 1U);
  EXPECT_TRUE(Game::Map::zone_has_structural_anchor(scenario->undead_zones.front()));

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
