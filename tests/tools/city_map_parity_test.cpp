

#include <QString>

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>

#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "tools/arena/arena_city_scenarios.h"
#include "tools/arena/arena_scenarios.h"
#include "utils/resource_utils.h"

namespace {

auto city_scenario() -> Arena::ArenaScenarioDefinition {
  for (const auto& candidate : Arena::Scenarios::build_city_definitions()) {
    if (candidate.id == QLatin1String(Arena::Scenarios::k_imperial_capital_id)) {
      return candidate;
    }
  }
  return {};
}

auto city_map() -> Game::Map::MapDefinition {
  Game::Map::MapDefinition map;
  QString error;
  EXPECT_TRUE(Game::Map::MapLoader::load_from_json_file(
      Utils::Resources::resolve_resource_path(
          QStringLiteral(":/assets/maps/map_aurelia_magna.json")),
      map,
      &error))
      << error.toStdString();
  return map;
}

auto find_group(const Arena::ArenaScenarioDefinition& scenario,
                const QString& name) -> const Arena::ArenaScenarioGroup* {
  const auto found = std::find_if(scenario.groups.begin(),
                                  scenario.groups.end(),
                                  [&](const auto& g) { return g.name == name; });
  return found == scenario.groups.end() ? nullptr : &*found;
}

} // namespace

TEST(CityMapParityTest, TheArenaCityIsBuiltFromTheShippedMap) {
  const auto scenario = city_scenario();
  ASSERT_FALSE(scenario.groups.empty())
      << "the imperial capital read no city; the map is missing or unreadable";

  const auto map = city_map();
  ASSERT_FALSE(map.structures.empty());

  int map_owned_structures = 0;
  for (const auto& entry : map.structures) {
    if (entry.player_id == 1) {
      ++map_owned_structures;
    }
  }
  int map_owned_spawns = 0;
  for (const auto& spawn : map.spawns) {
    if (spawn.player_id == 1) {
      ++map_owned_spawns;
    }
  }

  int scenario_structures = 0;
  int scenario_units = 0;
  for (const auto& group : scenario.groups) {
    if (group.spawn_type.has_value() &&
        Game::Units::is_building_spawn(*group.spawn_type)) {

      scenario_structures +=
          *group.spawn_type == Game::Units::SpawnType::WallSegment ? 1 : group.count;
    } else {
      scenario_units += group.count;
    }
  }

  EXPECT_EQ(scenario_structures, map_owned_structures);
  EXPECT_EQ(scenario_units, map_owned_spawns);
  EXPECT_EQ(scenario.roads.size(), map.roads.size());
  EXPECT_EQ(scenario.rivers.size(), map.rivers.size());
  EXPECT_EQ(scenario.bridges.size(), map.bridges.size());
  EXPECT_EQ(scenario.resource_patches.size(), map.world_props.size());
}

TEST(CityMapParityTest, LandmarksStandWhereTheMapPutsThem) {
  const auto scenario = city_scenario();
  const auto map = city_map();
  ASSERT_FALSE(scenario.groups.empty());

  for (const auto& name : {QStringLiteral("capital_temple"),
                           QStringLiteral("capital_gate_south"),
                           QStringLiteral("citadel_gate_citadel"),
                           QStringLiteral("capital_grange_west")}) {
    const auto* group = find_group(scenario, name);
    ASSERT_NE(group, nullptr) << name.toStdString();

    const auto entry = std::find_if(map.structures.begin(),
                                    map.structures.end(),
                                    [&](const auto& s) { return s.id == name; });
    ASSERT_NE(entry, map.structures.end()) << name.toStdString();
    const auto* point =
        std::get_if<Game::Map::PointStructureGeometry>(&entry->geometry);
    ASSERT_NE(point, nullptr) << name.toStdString();

    EXPECT_NEAR(group->origin.x(), point->position.x(), 0.01F) << name.toStdString();
    EXPECT_NEAR(group->origin.z(), point->position.z(), 0.01F) << name.toStdString();
  }
}

TEST(CityMapParityTest, ThePatrolsWalkTheRoutesTheMapCarries) {
  const auto scenario = city_scenario();
  ASSERT_FALSE(scenario.groups.empty());

  const auto* patrol = find_group(scenario, QStringLiteral("capital_patrol_avenue"));
  ASSERT_NE(patrol, nullptr);

  const bool marches =
      std::any_of(scenario.steps.begin(), scenario.steps.end(), [](const auto& step) {
        return step.group == QStringLiteral("capital_patrol_avenue") &&
               step.command == Arena::ScenarioCommandKind::FormationMove;
      });
  EXPECT_TRUE(marches) << "the avenue patrol has no beat to walk";
}

TEST(CityMapParityTest, TheBesiegersCommanderIsMissionContentNotCity) {
  const auto scenario = city_scenario();
  const auto map = city_map();

  const bool map_has_besieger =
      std::any_of(map.spawns.begin(), map.spawns.end(), [](const auto& spawn) {
        return spawn.player_id == 2;
      });
  EXPECT_TRUE(map_has_besieger)
      << "the siege mission needs its commander authored in the map";

  const bool scenario_has_enemy =
      std::any_of(scenario.groups.begin(), scenario.groups.end(), [](const auto& g) {
        return g.owner_id != 1;
      });
  EXPECT_FALSE(scenario_has_enemy)
      << "the promo city should read only its own owner out of the map";
}
