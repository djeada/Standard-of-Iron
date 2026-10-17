#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>
#include <array>
#include <gtest/gtest.h>
#include <set>
#include <sstream>
#include <string>

#include "app/session/skirmish_loader.h"
#include "core/component.h"
#include "core/world.h"
#include "game/map/base_options.h"
#include "game/map/map_catalog.h"
#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/session/map_session.h"
#include "game/session/session_context.h"
#include "game/systems/ai_system.h"
#include "game/systems/default_content.h"
#include "game/systems/formation_combat_geometry.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/navigation/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "game/systems/troop_count_registry.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_config.h"
#include "render/scene_renderer.h"
#include "scene/camera.h"

namespace {

constexpr float k_tick = 1.0F / 60.0F;
constexpr int k_opening_ticks = 1200;

constexpr std::array<const char*, 6> k_skirmish_maps = {
    "map_amber_delta.json",
    "map_copper_canyons.json",
    "map_forest.json",
    "map_mountain.json",
    "map_rivers.json",
    "map_spanish_grove.json",
};

auto two_versus_two(const Game::Map::MapDefinition& def) -> QVariantList {
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
    config["team_id"] = seat <= (static_cast<int>(seats.size()) + 1) / 2 ? 1 : 2;
    config["colorHex"] = QStringLiteral("#808080");
    config["isHuman"] = seat == 1;
    config["nationId"] = seat % 2 == 1 ? QStringLiteral("roman_republic")
                                       : QStringLiteral("carthaginian_empire");
    configs.append(config);
  }
  return configs;
}

auto short_squads(Engine::Core::World& world) -> std::string {
  std::ostringstream out;
  for (auto* entity : world.collect_entities_with<Engine::Core::UnitComponent>()) {
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || unit->owner_id <= 0 ||
        !Game::Units::is_troop_spawn(unit->spawn_type) ||
        entity->has_component<Engine::Core::BuildingComponent>()) {
      continue;
    }
    const int establishment =
        Game::Units::TroopConfig::instance().get_individuals_per_unit(unit->spawn_type);
    const int listed =
        Game::Systems::FormationCombat::resolve_definition(*unit).total_count;
    const int alive =
        unit->health > 0
            ? Game::Systems::FormationCombat::living_slot_count(*entity, listed)
            : 0;
    if (alive == establishment && listed == establishment) {
      continue;
    }
    out << "\n  owner " << unit->owner_id << " "
        << Game::Units::spawn_typeToString(unit->spawn_type) << " #" << entity->get_id()
        << ": " << alive << " / " << listed << " standing (establishment "
        << establishment << "), health " << unit->health << " / " << unit->max_health
        << ", squad_strength " << unit->squad_strength;
    if (const auto* roster =
            entity
                ->get_component<Engine::Core::FormationRosterPresentationComponent>()) {
      out << ", roster " << roster->live_count << " / " << roster->total_count;
    }
  }
  return out.str();
}

class SkirmishStartingStrengthTest : public ::testing::TestWithParam<const char*> {
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

} // namespace

TEST(SkirmishStartingStrengthCatalogTest, EveryShippedSkirmishMapIsChecked) {
  for (const QVariant& entry : Game::Map::MapCatalog::available_maps()) {
    const QString name = entry.toMap()
                             .value(QStringLiteral("path"))
                             .toString()
                             .section(QLatin1Char('/'), -1);
    if (name.isEmpty()) {
      continue;
    }
    EXPECT_TRUE(std::any_of(
        k_skirmish_maps.begin(),
        k_skirmish_maps.end(),
        [&](const char* checked) { return name == QLatin1String(checked); }))
        << name.toStdString()
        << " is offered in skirmish but its starting squads are not checked";
  }
}

TEST_P(SkirmishStartingStrengthTest, EveryStartingSquadTakesTheFieldAtFullStrength) {
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
  Game::Systems::register_runtime_systems(world);
  int selected_player_id = 1;
  const auto result =
      loader.start(path, two_versus_two(def), 1, true, selected_player_id);
  ASSERT_TRUE(result.ok) << path.toStdString() << ": "
                         << result.error_message.toStdString();
  Game::Session::configure_map_systems(world, def, nullptr);
  auto& session = Game::Session::session_for(world);
  session.terrain().seal();
  session.troop_counts().rebuild_from_world(world);
  if (auto* ai = world.get_system<Game::Systems::AISystem>()) {
    ai->reinitialize();
  }
  Game::Systems::NavGrid::prewarm();

  EXPECT_EQ(short_squads(world), "")
      << path.toStdString() << ": squads short of men as the match opens";

  for (int tick = 1; tick <= k_opening_ticks; ++tick) {
    world.update(k_tick);
    const std::string short_of_men = short_squads(world);
    if (!short_of_men.empty()) {
      ADD_FAILURE() << path.toStdString() << ": a starting squad lost men "
                    << static_cast<float>(tick) * k_tick
                    << " s into the match, before anyone gave an order:"
                    << short_of_men;
      break;
    }
  }
}

INSTANTIATE_TEST_SUITE_P(ShippedSkirmishMaps,
                         SkirmishStartingStrengthTest,
                         ::testing::ValuesIn(k_skirmish_maps),
                         [](const ::testing::TestParamInfo<const char*>& info) {
                           std::string name = info.param;
                           name = name.substr(4, name.size() - 9);
                           return name;
                         });
