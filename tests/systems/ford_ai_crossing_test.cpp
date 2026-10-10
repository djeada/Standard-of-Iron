#include <QVector3D>

#include <cmath>
#include <gtest/gtest.h>
#include <vector>

#include "core/component_core.h"
#include "core/component_gameplay.h"
#include "core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/systems/ai_system/ai_command_applier.h"
#include "game/systems/ai_system/ai_types.h"
#include "game/systems/default_content.h"
#include "game/systems/nation_registry.h"
#include "game/systems/navigation/nav_grid.h"
#include "game/systems/navigation/pathfinding.h"
#include "game/systems/owner_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "units/spawn_type.h"

namespace {

using Game::Map::TerrainService;

constexpr int k_grid = 64;
constexpr float k_tick = 1.0F / 30.0F;

// A river down the middle of the map (x = 0), too deep to cross except at
// one ford.
auto make_ford_map() -> Game::Map::MapDefinition {
  Game::Map::MapDefinition map;
  map.coordSystem = Game::Map::CoordSystem::World;
  map.grid.width = k_grid;
  map.grid.height = k_grid;
  map.grid.tile_size = 1.0F;
  map.biome.procedural_trees_enabled = false;
  map.biome.procedural_boulders_enabled = false;
  map.biome.procedural_iron_ore_enabled = false;
  map.rivers.push_back(
      {QVector3D(0.0F, 0.0F, -40.0F), QVector3D(0.0F, 0.0F, 40.0F), 8.0F});
  Game::Map::FordCrossing ford;
  ford.id = QStringLiteral("ford");
  ford.position = QVector3D(0.0F, 0.0F, -12.0F);
  ford.length = 10.0F;
  map.fords = {ford};
  return map;
}

class FordAiCrossingTest : public ::testing::Test {
protected:
  void SetUp() override {
    TerrainService::instance().clear();
    auto& session = Game::Session::SessionContext::active();
    session.owners().clear();
    session.owners().register_owner_with_id(1, Game::Systems::OwnerType::Player, "P1");
    session.owners().register_owner_with_id(2, Game::Systems::OwnerType::AI, "AI");
    session.owners().set_local_player_id(1);
    session.nations().clear();
    Game::Systems::initialize_default_content(session.nations());
    session.nations().set_player_nation(1, Game::Systems::NationID::RomanRepublic);
    session.nations().set_player_nation(2, Game::Systems::NationID::Carthage);
  }

  void TearDown() override {
    TerrainService::instance().clear();
    Game::Session::SessionContext::active().owners().clear();
    Game::Session::SessionContext::active().nations().clear();
  }
};

} // namespace

TEST_F(FordAiCrossingTest, AnAiPlannerMoveWadesAcrossTheFord) {
  auto const map = make_ford_map();
  TerrainService::instance().initialize(map);
  Game::Systems::NavGrid::initialize(map.grid.width, map.grid.height);
  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->mark_navigation_grid_dirty();
  pathfinder->update_navigation_grid();

  Engine::Core::World world;
  Game::Systems::register_runtime_systems(world);
  std::vector<Engine::Core::EntityID> squad;
  for (int index = 0; index < 4; ++index) {
    auto* entity = world.create_entity();
    auto* transform = entity->add_component<Engine::Core::TransformComponent>();
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    entity->add_component<Engine::Core::MovementComponent>();
    entity->add_component<Engine::Core::AIControlledComponent>();
    transform->position = {-16.0F, 0.0F, 8.0F + static_cast<float>(index) * 1.5F};
    unit->owner_id = 2;
    unit->spawn_type = Game::Units::SpawnType::Swordsman;
    unit->nation_id = Game::Systems::NationID::Carthage;
    unit->health = 1000;
    unit->max_health = 1000;
    unit->speed = 2.1F;
    squad.push_back(entity->get_id());
  }

  QVector3D const target(16.0F, 0.0F, 8.0F);
  Game::Systems::AI::AICommand command;
  command.type = Game::Systems::AI::AICommandType::MoveUnits;
  command.units = squad;
  command.move_target_x = {target.x()};
  command.move_target_y = {0.0F};
  command.move_target_z = {target.z()};
  Game::Systems::AI::AICommandApplier::apply(world, 2, {command});

  bool waded = false;
  int arrived = 0;
  for (int tick = 0; tick < 30 * 90 && arrived < static_cast<int>(squad.size());
       ++tick) {
    world.update(k_tick);
    arrived = 0;
    for (auto const id : squad) {
      if (auto const* wading = world.try_get<Engine::Core::WadingComponent>(id);
          wading != nullptr && wading->in_water()) {
        waded = true;
      }
      auto const* transform = world.try_get<Engine::Core::TransformComponent>(id);
      if (transform != nullptr && transform->position.x > 6.0F &&
          std::hypot(transform->position.x - target.x(),
                     transform->position.z - target.z()) < 8.0F) {
        ++arrived;
      }
    }
  }
  EXPECT_TRUE(waded) << "the AI's troops took the ford";
  EXPECT_GE(arrived, static_cast<int>(squad.size()) - 1)
      << "the AI's squad reached the far bank";
}
