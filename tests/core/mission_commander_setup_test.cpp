#include <algorithm>
#include <gtest/gtest.h>
#include <map>
#include <set>
#include <vector>

#include "game/map/campaign_loader.h"
#include "game/map/mission_loader.h"
#include "game/mission/mission_commander_setup.h"
#include "game/systems/nation_id.h"
#include "game/units/commander_catalog.h"
#include "game/units/troop_type.h"
#include "utils/resource_utils.h"

TEST(MissionCommanderSetupTest, FallsBackToHannibalForCarthage) {
  EXPECT_EQ(Game::Mission::resolve_commander_troop("carthage", std::nullopt),
            QStringLiteral("carthage_sword_commander"));
}

TEST(MissionCommanderSetupTest, KeepsConfiguredCommanderWhenPresent) {
  EXPECT_EQ(Game::Mission::resolve_commander_troop(
                "carthage", QStringLiteral("carthage_bow_commander")),
            QStringLiteral("carthage_bow_commander"));
}

TEST(MissionCommanderSetupTest, FallsBackWhenConfiguredCommanderBelongsToOtherNation) {
  EXPECT_EQ(Game::Mission::resolve_commander_troop(
                "carthage", QStringLiteral("roman_veteran_consul")),
            QStringLiteral("carthage_sword_commander"));
}

TEST(MissionCommanderSetupTest, PrefersAuthoredTroopPositions) {
  std::vector<Game::Mission::UnitSetup> units = {
      {.type = "spearman", .count = 2, .position = {10.0F, 20.0F}},
      {.type = "archer", .count = 1, .position = {16.0F, 26.0F}}};

  const auto resolved =
      Game::Mission::resolve_commander_position(units, {}, {}, {90.0F, 90.0F});

  EXPECT_EQ(resolved.space, Game::Mission::CommanderPositionSpace::Mission);
  EXPECT_FLOAT_EQ(resolved.position.x, 12.0F);
  EXPECT_FLOAT_EQ(resolved.position.z, 22.0F);
}

TEST(MissionCommanderSetupTest, UsesExistingWorldTroopsWhenMissionHasNoSpawns) {
  std::vector<Game::Mission::ExistingOwnerSpawnAnchor> existing_spawns = {
      {{-64.5F, -36.5F}, false},
      {{-64.5F, -32.5F}, false},
      {{-68.5F, -34.5F}, false},
      {{-68.5F, -38.5F}, false},
      {{-62.5F, -40.5F}, false},
      {{-62.5F, -28.5F}, false},
      {{-70.5F, -36.5F}, false},
      {{-70.5F, -32.5F}, false},
      {{-69.5F, -34.5F}, false},
  };

  const auto resolved = Game::Mission::resolve_commander_position(
      {}, {}, existing_spawns, {68.0F, 70.0F});

  EXPECT_EQ(resolved.space, Game::Mission::CommanderPositionSpace::World);
  EXPECT_FLOAT_EQ(resolved.position.x, -66.8333359F);
  EXPECT_FLOAT_EQ(resolved.position.z, -34.9444427F);
}

TEST(MissionCommanderSetupTest,
     UsesLocalWorldClusterWhenExistingSpawnsAreSpreadAcrossMap) {
  std::vector<Game::Mission::ExistingOwnerSpawnAnchor> existing_spawns = {
      {{32.5F, 57.5F}, false},
      {{36.5F, 57.5F}, false},
      {{35.5F, 61.5F}, false},
      {{63.5F, 57.5F}, false},
      {{67.5F, 57.5F}, false},
  };

  const auto resolved = Game::Mission::resolve_commander_position(
      {}, {}, existing_spawns, {132.0F, 80.0F});

  EXPECT_EQ(resolved.space, Game::Mission::CommanderPositionSpace::World);
  EXPECT_FLOAT_EQ(resolved.position.x, 34.8333321F);
  EXPECT_FLOAT_EQ(resolved.position.z, 58.8333321F);
}

TEST(MissionCommanderSetupTest, CommanderNationLookupMatchesTheCatalog) {
  for (const auto& definition : Game::Units::all_commander_definitions()) {
    const auto nation = Game::Units::commander_troop_nation(definition.troop_type);
    ASSERT_TRUE(nation.has_value())
        << "catalog commander " << definition.id << " has no nation in troop_type.h";
    EXPECT_EQ(*nation, definition.nation_id) << "nation drift for " << definition.id;
  }
}

TEST(MissionCommanderSetupTest, NationDefaultsResolveToCatalogCommanders) {
  for (const auto nation :
       {Game::Systems::NationID::RomanRepublic, Game::Systems::NationID::Carthage}) {
    const auto troop = Game::Units::default_commander_troop_for_nation(nation);
    const auto* definition = Game::Units::commander_definition(troop);
    ASSERT_NE(definition, nullptr);
    EXPECT_EQ(definition->nation_id, nation);
  }
}

namespace {

auto campaign_missions() -> std::vector<Game::Mission::MissionDefinition> {
  Game::Campaign::CampaignDefinition campaign;
  QString error;
  EXPECT_TRUE(Game::Campaign::CampaignLoader::load_from_json_file(
      Utils::Resources::resolve_resource_path(
          QStringLiteral(":/assets/campaigns/second_punic_war.json")),
      campaign,
      &error))
      << error.toStdString();

  std::vector<Game::Mission::MissionDefinition> missions;
  for (const auto& entry : campaign.missions) {
    Game::Mission::MissionDefinition mission;
    QString mission_error;
    EXPECT_TRUE(Game::Mission::MissionLoader::load_from_json_file(
        Utils::Resources::resolve_resource_path(
            QStringLiteral(":/assets/missions/%1.json").arg(entry.mission_id)),
        mission,
        &mission_error))
        << entry.mission_id.toStdString() << ": " << mission_error.toStdString();
    missions.push_back(std::move(mission));
  }
  return missions;
}

auto commander_pool_size(Game::Systems::NationID nation) -> std::size_t {
  std::size_t count = 0;
  for (const auto& definition : Game::Units::all_commander_definitions()) {
    if (definition.nation_id == nation) {
      ++count;
    }
  }
  return count;
}

} // namespace

TEST(MissionCommanderSetupTest, EveryCampaignForceHasExactlyOneMapCommander) {
  const auto missions = campaign_missions();
  ASSERT_FALSE(missions.empty());

  for (const auto& mission : missions) {
    const auto commanders = Game::Mission::commander_troops_for_map(mission.map_path);
    const std::size_t force_count = 1 + mission.ai_setups.size();

    for (std::size_t index = 0; index < force_count; ++index) {
      const int owner_id = static_cast<int>(index) + 1;
      const auto it = commanders.find(owner_id);
      ASSERT_NE(it, commanders.end())
          << mission.id.toStdString() << ": " << mission.map_path.toStdString()
          << " authors no commander for owner " << owner_id;
      EXPECT_FALSE(it->second.trimmed().isEmpty())
          << mission.id.toStdString() << " owner " << owner_id;
    }
  }
}

TEST(MissionCommanderSetupTest, CampaignCommandersMatchTheirForcesNation) {
  for (const auto& mission : campaign_missions()) {
    const auto commanders = Game::Mission::commander_troops_for_map(mission.map_path);

    std::vector<QString> nations{mission.player_setup.nation};
    for (const auto& ai_setup : mission.ai_setups) {
      nations.push_back(ai_setup.nation);
    }

    for (std::size_t index = 0; index < nations.size(); ++index) {
      const int owner_id = static_cast<int>(index) + 1;
      const auto it = commanders.find(owner_id);
      if (it == commanders.end()) {
        continue;
      }
      Game::Units::TroopType troop_type{};
      ASSERT_TRUE(Game::Units::try_parse_troop_type(it->second, troop_type))
          << mission.id.toStdString() << " owner " << owner_id;
      const auto* definition = Game::Units::commander_definition(troop_type);
      ASSERT_NE(definition, nullptr) << mission.id.toStdString();

      Game::Systems::NationID expected{};
      ASSERT_TRUE(Game::Systems::try_parse_nation_id(nations[index], expected))
          << nations[index].toStdString();
      EXPECT_EQ(definition->nation_id, expected)
          << mission.id.toStdString() << " owner " << owner_id << " fields "
          << it->second.toStdString() << " under the wrong flag";
    }
  }
}

TEST(MissionCommanderSetupTest, CampaignCommandersAreUniqueUntilThePoolRunsOut) {
  for (const auto& mission : campaign_missions()) {
    const auto commanders = Game::Mission::commander_troops_for_map(mission.map_path);

    std::map<Game::Systems::NationID, std::vector<QString>> by_nation;
    for (const auto& [owner_id, troop] : commanders) {
      Game::Units::TroopType troop_type{};
      if (!Game::Units::try_parse_troop_type(troop, troop_type)) {
        continue;
      }
      const auto* definition = Game::Units::commander_definition(troop_type);
      if (definition == nullptr) {
        continue;
      }
      by_nation[definition->nation_id].push_back(troop);
    }

    for (const auto& [nation, troops] : by_nation) {
      std::set<QString> distinct(troops.begin(), troops.end());
      const std::size_t pool = commander_pool_size(nation);
      const std::size_t expected = std::min(troops.size(), pool);
      EXPECT_EQ(distinct.size(), expected)
          << mission.id.toStdString() << " fields " << troops.size()
          << " forces for one nation using only " << distinct.size()
          << " distinct commanders, though " << pool << " exist";
    }
  }
}
