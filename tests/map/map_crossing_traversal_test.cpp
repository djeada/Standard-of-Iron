#include <QDebug>
#include <QDir>
#include <QString>
#include <QVector3D>

#include <cmath>
#include <gtest/gtest.h>
#include <optional>
#include <vector>

#include "core/component.h"
#include "core/world.h"
#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/systems/command_service.h"
#include "game/systems/default_content.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/pathfinding.h"
#include "game/systems/runtime_system_registry.h"
#include "game/systems/walkability.h"
#include "units/spawn_type.h"

namespace {

constexpr float k_tick_seconds = 1.0F / 30.0F;

constexpr float k_approach_metres = 12.0F;
constexpr int k_crossing_seconds = 120;

auto load_map(const QString& file_name) -> Game::Map::MapDefinition {
  Game::Map::MapDefinition map;
  QString error;
  EXPECT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QDir(QStringLiteral("assets/maps")).filePath(file_name), map, &error))
      << file_name.toStdString() << ": " << error.toStdString();
  return map;
}

class MapCrossingTraversalTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto& session = Game::Session::SessionContext::active();
    session.owners().clear();
    session.owners().register_owner_with_id(1, Game::Systems::OwnerType::Player, "P1");
    session.owners().set_local_player_id(1);
    session.nations().clear();
    Game::Systems::initialize_default_content(session.nations());
    session.nations().set_player_nation(1, Game::Systems::NationID::Carthage);
  }

  void TearDown() override {
    Game::Map::TerrainService::instance().clear();
    Game::Session::SessionContext::active().owners().clear();
    Game::Session::SessionContext::active().nations().clear();
  }

  static void build_navigation(const Game::Map::MapDefinition& map) {
    Game::Map::TerrainService::instance().initialize(map);
    Game::Systems::NavGrid::initialize(map.grid.width, map.grid.height);
    auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
    ASSERT_NE(pathfinder, nullptr);
    pathfinder->mark_navigation_grid_dirty();
    pathfinder->update_navigation_grid();
  }

  static auto spawn_squad(Engine::Core::World& world,
                          const QVector3D& centre,
                          int count) -> std::vector<Engine::Core::EntityID> {
    std::vector<Engine::Core::EntityID> ids;
    ids.reserve(static_cast<std::size_t>(count));
    const int per_row = 4;
    for (int index = 0; index < count; ++index) {
      auto* entity = world.create_entity();
      EXPECT_NE(entity, nullptr);
      auto* transform = entity->add_component<Engine::Core::TransformComponent>();
      auto* unit = entity->add_component<Engine::Core::UnitComponent>();
      entity->add_component<Engine::Core::MovementComponent>();
      const float offset_x = static_cast<float>(index % per_row) - 1.5F;
      const float offset_z = static_cast<float>(index / per_row) - 1.0F;
      transform->position = {centre.x() + offset_x, 0.0F, centre.z() + offset_z};
      unit->spawn_type = Game::Units::SpawnType::Knight;
      unit->owner_id = 1;
      unit->nation_id = Game::Systems::NationID::Carthage;
      unit->health = 1000;
      unit->max_health = 1000;
      unit->speed = 2.1F;
      ids.push_back(entity->get_id());
    }
    return ids;
  }

  static auto arrived_count(Engine::Core::World& world,
                            const std::vector<Engine::Core::EntityID>& ids,
                            const QVector3D& target,
                            float tolerance) -> int {
    int arrived = 0;
    for (const auto id : ids) {
      auto* entity = world.get_entity(id);
      if (entity == nullptr) {
        continue;
      }
      const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
      if (transform == nullptr) {
        continue;
      }
      const float distance = std::hypot(transform->position.x - target.x(),
                                        transform->position.z - target.z());
      if (distance <= tolerance) {
        ++arrived;
      }
    }
    return arrived;
  }

  static auto standable(const QVector3D& position) -> std::optional<QVector3D> {
    const Game::Systems::BodyProfile profile{
        .radius = 0.5F, .passability = Game::Systems::Pathfinding::Passability::Light};
    return Game::Systems::Walkability::nearest_standable(position, profile, 24.0F);
  }

  static auto
  worst_stuck_time(Engine::Core::World& world,
                   const std::vector<Engine::Core::EntityID>& ids) -> float {
    float worst = 0.0F;
    for (const auto id : ids) {
      auto* entity = world.get_entity(id);
      if (entity == nullptr) {
        continue;
      }
      const auto* movement = entity->get_component<Engine::Core::MovementComponent>();
      if (movement != nullptr) {
        worst = std::max(worst, movement->get_stuck_time());
      }
    }
    return worst;
  }
};

} // namespace

TEST_F(MapCrossingTraversalTest, ASquadOrderedOverEveryShippedFordReachesTheFarBank) {
  const QStringList maps = {QStringLiteral("map_rivers.json"),
                            QStringLiteral("map_mountain.json"),
                            QStringLiteral("map_copper_canyons.json"),
                            QStringLiteral("map_amber_delta.json")};

  for (const QString& file_name : maps) {
    auto map = load_map(file_name);
    if (map.bridges.empty()) {
      continue;
    }
    build_navigation(map);

    const auto* height_map = Game::Map::TerrainService::instance().get_height_map();
    ASSERT_NE(height_map, nullptr) << file_name.toStdString();
    const auto& fitted = height_map->get_bridges();
    ASSERT_FALSE(fitted.empty()) << file_name.toStdString();

    int crossings_tested = 0;
    for (std::size_t index = 0; index < fitted.size(); ++index) {
      const auto& bridge = fitted[index];
      const QVector3D span = bridge.end - bridge.start;
      const float length = std::hypot(span.x(), span.z());
      if (length < 1.0e-3F) {
        continue;
      }
      const QVector3D axis(span.x() / length, 0.0F, span.z() / length);

      const auto start = standable(bridge.start - axis * k_approach_metres);
      const auto target = standable(bridge.end + axis * k_approach_metres);
      if (!start.has_value() || !target.has_value()) {
        continue;
      }
      ++crossings_tested;

      Engine::Core::World world;
      Game::Systems::register_runtime_systems(world);
      const int squad_size = qEnvironmentVariableIsSet("SOI_CROSSING_SIZE")
                                 ? qEnvironmentVariableIntValue("SOI_CROSSING_SIZE")
                                 : 12;
      const auto squad = spawn_squad(world, *start, squad_size);
      ASSERT_EQ(static_cast<int>(squad.size()), squad_size);

      Game::Systems::CommandService::move_units(
          world, squad, std::vector<QVector3D>(squad.size(), *target));

      for (int tick = 0; tick < k_crossing_seconds * 30; ++tick) {
        world.update(k_tick_seconds);
        if (qEnvironmentVariableIsSet("SOI_CROSSING_DUMP") && tick % 300 == 0) {
          auto* first = world.get_entity(squad.front());
          const auto* t = first->get_component<Engine::Core::TransformComponent>();
          const auto* m = first->get_component<Engine::Core::MovementComponent>();
          const auto& path = m->get_path();
          const std::size_t wp = m->get_path_index();
          const float wp_x = wp < path.size() ? path[wp].first : 0.0F;
          const float wp_z = wp < path.size() ? path[wp].second : 0.0F;
          const Game::Systems::BodyProfile probe{
              .radius = 0.5F,
              .passability = Game::Systems::Pathfinding::Passability::Light};
          const bool wp_standable =
              Game::Systems::Walkability::can_stand(QVector3D(wp_x, 0.0F, wp_z), probe);
          const bool here_standable = Game::Systems::Walkability::can_stand(
              QVector3D(t->position.x, 0.0F, t->position.z), probe);
          qWarning("%s[%zu] t=%5.1fs pos=(%7.2f,%7.2f)%s goal=(%7.2f,%7.2f) "
                   "target=%d stuck=%5.2f wp=%zu/%zu v=(%5.2f,%5.2f) "
                   "next=(%7.2f,%7.2f)%s",
                   file_name.toLatin1().constData(),
                   index,
                   static_cast<double>(tick) * k_tick_seconds,
                   t->position.x,
                   t->position.z,
                   here_standable ? "" : "!",
                   m->get_goal_x(),
                   m->get_goal_y(),
                   static_cast<int>(m->get_has_target()),
                   m->get_stuck_time(),
                   m->get_path_index(),
                   m->get_path().size(),
                   m->get_vx(),
                   m->get_vz(),
                   wp_x,
                   wp_z,
                   wp_standable ? "" : "!");
        }
      }

      const int arrived = arrived_count(world, squad, *target, 10.0F);
      EXPECT_GE(arrived, (squad_size * 3) / 4)
          << file_name.toStdString() << " crossing " << index << ": only " << arrived
          << " of " << squad_size << " bodies crossed in " << k_crossing_seconds
          << "s; worst stuck time " << worst_stuck_time(world, squad) << "s";
      if (arrived < (squad_size * 3) / 4) {
        break;
      }
    }

    EXPECT_GT(crossings_tested, 0)
        << file_name.toStdString() << " has bridges but none could be approached";
  }
}
