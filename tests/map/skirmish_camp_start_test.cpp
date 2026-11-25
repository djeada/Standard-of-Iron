#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <array>
#include <gtest/gtest.h>
#include <map>
#include <set>
#include <sstream>
#include <string>

#include "app/session/skirmish_loader.h"
#include "core/component.h"
#include "core/world.h"
#include "game/map/base_options.h"
#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/systems/default_content.h"
#include "game/systems/economy/build_site.h"
#include "game/systems/economy/recruitment_gate.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/navigation/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "game/units/spawn_type.h"
#include "render/scene/scene_renderer.h"
#include "scene/camera.h"

namespace {

constexpr std::array<const char*, 8> k_skirmish_maps = {
    "map_amber_delta.json",
    "map_carthage.json",
    "map_copper_canyons.json",
    "map_forest.json",
    "map_mountain.json",
    "map_rivers.json",
    "map_rome.json",
    "map_spanish_grove.json",
};

auto every_seat(const Game::Map::MapDefinition& def) -> QVariantList {
  std::set<int> seats;
  for (const auto& option : Game::Map::collect_base_options(def)) {
    if (option.default_player_id > 0) {
      seats.insert(option.default_player_id);
    }
  }
  QVariantList configs;
  for (const int seat : seats) {
    QVariantMap config;
    config["player_id"] = seat;
    config["team_id"] = seat;
    config["colorHex"] = QStringLiteral("#808080");
    config["isHuman"] = seat == 1;
    config["nationId"] = seat % 2 == 1 ? QStringLiteral("roman_republic")
                                       : QStringLiteral("carthage");
    configs.append(config);
  }
  return configs;
}

class SkirmishCampStartTest : public ::testing::TestWithParam<const char*> {
protected:
  void SetUp() override {
    auto& nations = Game::Systems::NationRegistry::instance();
    nations.clear();
    Game::Systems::initialize_default_content(nations);
    Game::Systems::OwnerRegistry::instance().clear();
    Game::Systems::GlobalStatsRegistry::instance().clear();
  }

  void TearDown() override {
    Game::Map::TerrainService::instance().clear();
    Game::Map::VisibilityService::instance().reset();
    Game::Systems::GlobalStatsRegistry::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::OwnerRegistry::instance().clear();
  }
};

TEST_P(SkirmishCampStartTest, EverySeatStartsWithAWorkingCamp) {
  const QString path =
      QStringLiteral("assets/maps/%1").arg(QString::fromLatin1(GetParam()));
  Game::Map::MapDefinition def;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(path, def, &error))
      << path.toStdString() << ": " << error.toStdString();

  Engine::Core::World world;
  Render::GL::Renderer renderer{Render::ShaderQuality::None};
  Render::GL::Camera camera;
  App::Core::SkirmishLoader loader{world, renderer, camera};
  loader.set_camp_start_enabled(true);
  Game::Systems::register_runtime_systems(world);
  int selected_player_id = 1;
  const auto result = loader.start(path, every_seat(def), 1, true, selected_player_id);
  ASSERT_TRUE(result.ok) << path.toStdString() << ": "
                         << result.error_message.toStdString();
  Game::Systems::NavGrid::prewarm();

  const auto& owners = Game::Systems::OwnerRegistry::instance();
  const auto& camps = loader.camp_start_reports();
  int seats = 0;
  for (const auto& camp : camps) {
    ++seats;
    std::ostringstream where;
    where << path.toStdString() << " owner " << camp.owner_id;
    ASSERT_TRUE(owners.is_player(camp.owner_id) || owners.is_ai(camp.owner_id))
        << where.str();

    int builders = 0;
    int owned_farms = 0;
    for (auto* entity : world.collect_entities_with<Engine::Core::UnitComponent>()) {
      const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
      if (unit == nullptr || unit->owner_id != camp.owner_id || unit->health <= 0) {
        continue;
      }
      builders += unit->spawn_type == Game::Units::SpawnType::Builder ? 1 : 0;
      owned_farms += unit->spawn_type == Game::Units::SpawnType::Farm ? 1 : 0;
    }
    EXPECT_EQ(camp.homes.size(), 2U) << where.str() << ": homes raised";
    EXPECT_GE(owned_farms, 1) << where.str()
                              << ": no farm raised and none owned already";
    EXPECT_GE(builders, 3) << where.str() << ": builder crews";
    EXPECT_EQ(camp.squads.size(), 2U) << where.str() << ": fighting squads";

    const auto* hall = world.get_entity(camp.barracks);
    ASSERT_NE(hall, nullptr) << where.str();
    EXPECT_GE(hall->get_component<Engine::Core::ProductionComponent>()->manpower_available,
              200)
        << where.str() << ": barracks reserve";
    const auto gate = Game::Systems::barracks_gate(
                          *hall->get_component<Engine::Core::TransformComponent>())
                          .position;
    std::vector<Engine::Core::EntityID> raised = camp.homes;
    raised.insert(raised.end(), camp.farms.begin(), camp.farms.end());
    for (const auto id : raised) {
      const auto* transform =
          world.get_entity(id)->get_component<Engine::Core::TransformComponent>();
      EXPECT_NE(Game::Systems::site_access_on_foot(
                    gate.x(), gate.z(), transform->position.x, transform->position.z),
                Game::Systems::SiteAccess::CutOff)
          << where.str() << ": the camp building at " << transform->position.x << ","
          << transform->position.z << " cannot be walked to from the barracks gate";
    }
  }
  EXPECT_GE(seats, 2) << path.toStdString() << ": no seats were raised";
}

INSTANTIATE_TEST_SUITE_P(ShippedSkirmishMaps,
                         SkirmishCampStartTest,
                         ::testing::ValuesIn(k_skirmish_maps),
                         [](const ::testing::TestParamInfo<const char*>& info) {
                           std::string name = info.param;
                           name = name.substr(4, name.size() - 9);
                           return name;
                         });

} // namespace
