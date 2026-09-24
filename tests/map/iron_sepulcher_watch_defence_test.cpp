#include <QVariantList>
#include <QVariantMap>

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "app/session/skirmish_loader.h"
#include "core/component_gameplay.h"
#include "core/world.h"
#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/map_transformer.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/systems/command_service.h"
#include "game/systems/default_content.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "game/systems/undead_awakening_system.h"
#include "game/units/factory.h"
#include "render/scene_renderer.h"
#include "scene/camera.h"

namespace {

using Engine::Core::EntityID;

constexpr char k_map_path[] = "assets/maps/map_iron_sepulcher_watch.json";
constexpr int k_local_player_id = 1;
constexpr float k_tick = 1.0F / 60.0F;
constexpr double k_longest_tolerated_idle_seconds = 4.0;

struct Approach {
  const char* zone_id;
  float direction_x;
  float direction_z;
};

class IronSepulcherWatchDefenceTest : public ::testing::TestWithParam<Approach> {
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

TEST_P(IronSepulcherWatchDefenceTest, NoGuardianStandsIdleWhileItsZoneIsStormed) {
  const Approach approach = GetParam();

  Engine::Core::World world;
  Game::Systems::register_runtime_systems(world);
  Render::GL::Renderer renderer(Render::ShaderQuality::None);
  Render::GL::Camera camera;
  App::Core::SkirmishLoader loader(world, renderer, camera);

  QVariantMap player;
  player["player_id"] = k_local_player_id;
  player["team_id"] = 1;
  player["colorHex"] = QStringLiteral("#C8322D");
  player["isHuman"] = true;
  player["nationId"] = QStringLiteral("roman_republic");
  int selected_player_id = k_local_player_id;
  const auto load_result = loader.start(QString::fromLatin1(k_map_path),
                                        QVariantList{player},
                                        k_local_player_id,
                                        true,
                                        selected_player_id);
  ASSERT_TRUE(load_result.ok) << load_result.error_message.toStdString();

  Game::Map::MapDefinition map_definition;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QString::fromLatin1(k_map_path), map_definition, &error))
      << error.toStdString();
  auto* undead = world.get_system<Game::Systems::UndeadAwakeningSystem>();
  ASSERT_NE(undead, nullptr);
  undead->configure(map_definition);

  const QString zone_id = QString::fromLatin1(approach.zone_id);
  int zone_owner = 0;
  for (const auto& zone : map_definition.undead_zones) {
    if (zone.id == zone_id) {
      zone_owner = zone.owner_id;
    }
  }
  ASSERT_NE(zone_owner, 0) << "the map no longer authors " << approach.zone_id;
  const QVector3D anchor = undead->shrine_world_position(zone_id);

  const float length = std::hypot(approach.direction_x, approach.direction_z);
  const float along_x = approach.direction_x / length;
  const float along_z = approach.direction_z / length;
  auto factory = Game::Map::MapTransformer::get_factory_registry();
  ASSERT_NE(factory, nullptr);
  auto spawn = [&](Game::Units::SpawnType type, float along, float side) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = QVector3D(anchor.x() + along_x * along - along_z * side,
                                0.0F,
                                anchor.z() + along_z * along + along_x * side);
    params.player_id = k_local_player_id;
    params.spawn_type = type;
    params.ai_controlled = false;
    params.is_initial_spawn = false;
    params.nation_id = Game::Systems::NationID::RomanRepublic;
    auto unit = factory->create(type, world, params);
    return unit ? unit->id() : 0U;
  };

  const std::vector<EntityID> column{
      spawn(Game::Units::SpawnType::Swordsman, 12.0F, 0.0F),
      spawn(Game::Units::SpawnType::Swordsman, 12.0F, 3.0F),
      spawn(Game::Units::SpawnType::Spearman, 12.0F, -3.0F),
  };
  const std::vector<EntityID> archers{
      spawn(Game::Units::SpawnType::Archer, 17.0F, 0.0F),
      spawn(Game::Units::SpawnType::Archer, 17.0F, 3.0F),
  };
  std::vector<EntityID> raiders = column;
  raiders.insert(raiders.end(), archers.begin(), archers.end());
  Game::Systems::CommandService::MoveOptions options;
  options.kind = Game::Systems::MoveOrderKind::AttackMove;
  const std::vector<QVector3D> targets(
      column.size(),
      QVector3D(anchor.x() + along_x * 2.0F, 0.0F, anchor.z() + along_z * 2.0F));
  Game::Systems::CommandService::move_units(world, column, targets, options);

  auto living = [&world](int owner) {
    std::vector<EntityID> ids;
    for (auto* entity : world.collect_entities_with<Engine::Core::UnitComponent>()) {
      const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
      if (unit->owner_id == owner && unit->health > 0 &&
          Game::Units::is_troop_spawn(unit->spawn_type)) {
        ids.push_back(entity->get_id());
      }
    }
    return ids;
  };
  auto distance = [&world](EntityID a, EntityID b) {
    const auto* from = world.try_get<Engine::Core::TransformComponent>(a);
    const auto* to = world.try_get<Engine::Core::TransformComponent>(b);
    return std::hypot(to->position.x - from->position.x,
                      to->position.z - from->position.z);
  };
  auto fighting = [&world](EntityID id) {
    const auto* target = world.try_get<Engine::Core::AttackTargetComponent>(id);
    const auto* attack = world.try_get<Engine::Core::AttackComponent>(id);
    return (target != nullptr && target->target_id != 0) ||
           (attack != nullptr && attack->in_melee_lock);
  };

  std::map<EntityID, double> idle_since;
  double longest_idle = 0.0;
  std::string worst;
  for (double elapsed = 0.0; elapsed < 60.0; elapsed += k_tick) {
    world.update(k_tick);
    const auto guardians = living(zone_owner);
    std::vector<EntityID> standing_raiders;
    for (const auto id : raiders) {
      const auto* unit = world.try_get<Engine::Core::UnitComponent>(id);
      if (unit != nullptr && unit->health > 0) {
        standing_raiders.push_back(id);
      }
    }
    if ((guardians.empty() && elapsed > 5.0) || standing_raiders.empty()) {
      break;
    }
    for (const auto guardian : guardians) {
      const auto* unit = world.try_get<Engine::Core::UnitComponent>(guardian);
      float nearest = std::numeric_limits<float>::max();
      for (const auto raider : standing_raiders) {
        nearest = std::min(nearest, distance(guardian, raider));
      }
      const bool ally_fighting_in_sight =
          std::any_of(guardians.begin(), guardians.end(), [&](EntityID ally) {
            return ally != guardian && fighting(ally) &&
                   distance(ally, guardian) <= unit->vision_range;
          });
      if (!(nearest <= unit->vision_range || ally_fighting_in_sight) ||
          fighting(guardian)) {
        idle_since.erase(guardian);
        continue;
      }
      const auto [since, fresh] = idle_since.emplace(guardian, elapsed);
      if (elapsed - since->second > longest_idle) {
        longest_idle = elapsed - since->second;
        std::ostringstream name;
        name << Game::Units::spawn_typeToQString(unit->spawn_type).toStdString() << " #"
             << guardian << " (nearest raider " << nearest << " m)";
        worst = name.str();
      }
    }
  }

  EXPECT_LE(longest_idle, k_longest_tolerated_idle_seconds)
      << worst << " stood idle for " << longest_idle << " s while " << approach.zone_id
      << " was stormed";
}

INSTANTIATE_TEST_SUITE_P(ZonesAndApproaches,
                         IronSepulcherWatchDefenceTest,
                         ::testing::Values(Approach{"ruins_guard", -1.0F, 0.2F},
                                           Approach{"ruins_guard", 0.1F, 1.0F},
                                           Approach{"shrine_sentinels", -1.0F, 0.3F},
                                           Approach{"shrine_sentinels", 0.3F, 1.0F}),
                         [](const ::testing::TestParamInfo<Approach>& info) {
                           std::string name = info.param.zone_id;
                           name += info.index % 2 == 0 ? "_first_approach"
                                                       : "_second_approach";
                           return name;
                         });

} // namespace
