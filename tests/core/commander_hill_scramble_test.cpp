#include <QVector3D>

#include <cmath>
#include <gtest/gtest.h>
#include <optional>

#include "app/commander/commander_control_controller.h"
#include "app/commander/commander_locomotion.h"
#include "app/commander/commander_motor.h"
#include "game/core/component_commander.h"
#include "game/core/component_gameplay.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/movement/body_profile.h"
#include "game/systems/movement/movement_system_collision.h"
#include "game/systems/navigation/nav_grid.h"
#include "game/systems/navigation/pathfinding.h"
#include "game/systems/navigation/walkability.h"
#include "scene/camera.h"

namespace {

using Game::Systems::NavGrid;
using Game::Systems::Pathfinding;

constexpr float k_dt = 1.0F / 60.0F;
constexpr int k_grid = 96;
constexpr float k_hill_radius = 9.0F;

class CommanderHillScrambleTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
  }

  void TearDown() override {
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
  }

  static void raise(const Game::Map::TerrainFeature& hill) {
    Game::Map::MapDefinition map;
    map.grid.width = k_grid;
    map.grid.height = k_grid;
    map.grid.tile_size = 1.0F;
    map.coordSystem = Game::Map::CoordSystem::World;
    map.biome.procedural_boulders_enabled = false;
    map.biome.procedural_iron_ore_enabled = false;
    map.biome.procedural_trees_enabled = false;
    map.terrain = {hill};

    Game::Map::TerrainService::instance().initialize(map);
    NavGrid::initialize(k_grid, k_grid);
    NavGrid::get_pathfinder()->update_navigation_grid();
  }

  static void raise_hill(float height) {
    Game::Map::TerrainFeature hill;
    hill.type = Game::Map::TerrainType::Hill;
    hill.center_x = 0.0F;
    hill.center_z = 0.0F;
    hill.radius = k_hill_radius;
    hill.height = height;
    hill.exact_height = true;
    hill.shape = Game::Map::HillShape::Blob;
    hill.entrances.emplace_back(k_hill_radius + 2.0F, 0.0F, 0.0F);
    raise(hill);
  }

  static void raise_spur() {
    Game::Map::TerrainFeature spur;
    spur.type = Game::Map::TerrainType::Hill;
    spur.center_x = 0.0F;
    spur.center_z = 0.0F;
    spur.width = 42.0F;
    spur.depth = 14.0F;
    spur.height = 4.2F;
    spur.shape = Game::Map::HillShape::Corridor;
    spur.thickness = 12.0F;
    spur.rotation_deg = 87.0F;
    spur.taper = 0.55F;
    raise(spur);
  }

  static auto spawn_commander(Engine::Core::World& world,
                              float x,
                              float z,
                              bool direct_control) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    entity->add_component<Engine::Core::MovementComponent>();
    unit->health = 400;
    unit->max_health = 400;
    unit->owner_id = 1;
    unit->speed = 2.15F;
    unit->spawn_type = Game::Units::SpawnType::Swordsman;
    entity->add_component<Engine::Core::CommanderComponent>()->fpv_controlled =
        direct_control;
    return entity;
  }

  [[nodiscard]] static auto slope_cell_at(float x, float z) -> bool {
    auto const cell = NavGrid::world_to_grid(x, z);
    return NavGrid::get_pathfinder()->is_scramble_slope(cell.x, cell.y);
  }

  [[nodiscard]] static auto wall_cell_at(float x, float z) -> bool {
    auto const& terrain = Game::Map::TerrainService::instance();
    auto const cell = NavGrid::world_to_grid(x, z);
    return terrain.get_terrain_type(cell.x, cell.y) == Game::Map::TerrainType::Hill &&
           !terrain.is_walkable(cell.x, cell.y);
  }

  [[nodiscard]] static auto crown_at(float x, float z) -> bool {
    auto const& terrain = Game::Map::TerrainService::instance();
    auto const cell = NavGrid::world_to_grid(x, z);
    return terrain.get_terrain_type(cell.x, cell.y) == Game::Map::TerrainType::Hill &&
           terrain.is_walkable(cell.x, cell.y);
  }

  [[nodiscard]] static auto first_wall_z() -> std::optional<float> {
    for (float z = -(k_hill_radius + 12.0F); z < 0.0F; z += 0.25F) {
      if (wall_cell_at(0.0F, z)) {
        return z;
      }
    }
    return std::nullopt;
  }

  [[nodiscard]] static auto steepest_wall_on_approach() -> float {
    float steepest = 0.0F;
    for (float z = -(k_hill_radius + 12.0F); z < 0.0F; z += 0.5F) {
      if (wall_cell_at(0.0F, z)) {
        auto const cell = NavGrid::world_to_grid(0.0F, z);
        steepest = std::max(steepest,
                            NavGrid::get_pathfinder()->terrain_grade(cell.x, cell.y));
      }
    }
    return steepest;
  }

  static void walk(Engine::Core::World& world,
                   Engine::Core::Entity& commander,
                   CommanderControlController& controller,
                   bool forward,
                   float seconds) {
    Render::GL::Camera camera;
    auto const ticks = static_cast<int>(seconds / k_dt);
    for (int tick = 0; tick < ticks; ++tick) {
      controller.input().forward = forward;
      ASSERT_TRUE(controller.update(world, commander.get_id(), 1, camera, k_dt));
    }
  }
};

TEST_F(CommanderHillScrambleTest, ALedCommanderWalksStraightUpAHillside) {
  raise_hill(2.4F);
  auto const wall_z = first_wall_z();
  ASSERT_TRUE(wall_z.has_value()) << "the hill has no wall on the approach";
  ASSERT_LE(steepest_wall_on_approach(), Pathfinding::k_max_scramble_grade)
      << "this hillside is meant to be a bank, not a cliff";

  Engine::Core::World world;
  auto* commander = spawn_commander(world, 0.0F, *wall_z - 3.0F, true);
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  CommanderControlController controller;
  controller.set_view_yaw(0.0F);

  bool crossed_wall = false;
  Render::GL::Camera camera;
  for (int tick = 0; tick < 12 * 60; ++tick) {
    controller.input().forward = true;
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
    crossed_wall =
        crossed_wall || wall_cell_at(transform->position.x, transform->position.z);
    if (crown_at(transform->position.x, transform->position.z)) {
      break;
    }
  }
  EXPECT_TRUE(crossed_wall) << "the walk never touched the hillside";
  EXPECT_TRUE(crown_at(transform->position.x, transform->position.z))
      << "a small hill stopped the commander like an invisible wall at z="
      << transform->position.z;
}

TEST_F(CommanderHillScrambleTest, TheHillsideStaysAWallForEveryoneElse) {
  raise_hill(2.4F);
  auto const wall_z = first_wall_z();
  ASSERT_TRUE(wall_z.has_value());
  QVector3D const on_wall(0.0F, 0.0F, *wall_z + 0.5F);
  ASSERT_TRUE(slope_cell_at(on_wall.x(), on_wall.z()));

  Engine::Core::World world;
  auto* led = spawn_commander(world, 0.0F, *wall_z - 3.0F, true);
  auto* ordered = spawn_commander(world, 2.0F, *wall_z - 3.0F, false);
  auto* soldier = world.create_entity();
  soldier->add_component<Engine::Core::TransformComponent>(4.0F, 0.0F, *wall_z - 3.0F);
  soldier->add_component<Engine::Core::UnitComponent>();
  soldier->add_component<Engine::Core::MovementComponent>();

  using Game::Systems::Walkability::can_stand;
  EXPECT_TRUE(can_stand(on_wall, Game::Systems::motor_profile_for(*led)));
  EXPECT_FALSE(can_stand(on_wall, Game::Systems::motor_profile_for(*ordered)))
      << "a commander under orders keeps to the ramps like the army he leads";
  EXPECT_FALSE(can_stand(on_wall, Game::Systems::motor_profile_for(*soldier)));
  EXPECT_FALSE(NavGrid::get_pathfinder()->is_walkable(
      NavGrid::world_to_grid(on_wall.x(), on_wall.z()).x,
      NavGrid::world_to_grid(on_wall.x(), on_wall.z()).y))
      << "the navigation grid itself must not open";
}

TEST_F(CommanderHillScrambleTest, ACliffIsStillACliff) {
  raise_spur();
  Engine::Core::World world;
  auto* commander = spawn_commander(world, -40.0F, -40.0F, true);
  auto const profile = Game::Systems::motor_profile_for(*commander);
  auto const* pathfinder = NavGrid::get_pathfinder();
  auto const& terrain = Game::Map::TerrainService::instance();

  int cliffs = 0;
  for (int z = 1; z < k_grid - 1; ++z) {
    for (int x = 1; x < k_grid - 1; ++x) {
      if (terrain.get_terrain_type(x, z) != Game::Map::TerrainType::Hill ||
          terrain.is_walkable(x, z) ||
          pathfinder->terrain_grade(x, z) <= Pathfinding::k_max_scramble_grade) {
        continue;
      }
      ++cliffs;
      ASSERT_FALSE(pathfinder->is_scramble_slope(x, z));
      ASSERT_FALSE(Game::Systems::Walkability::can_stand(
          pathfinder->grid_to_world({x, z}), profile))
          << "the commander may stand on a cliff face at cell (" << x << ", " << z
          << ") with grade " << pathfinder->terrain_grade(x, z);
    }
  }
  EXPECT_GT(cliffs, 20) << "this spur is meant to have cliff faces";
}

TEST_F(CommanderHillScrambleTest, ClimbingCostsSpeedAndComingDownDoesNot) {
  raise_hill(2.4F);
  auto const wall_z = first_wall_z();
  ASSERT_TRUE(wall_z.has_value());

  Engine::Core::World world;
  auto* commander = spawn_commander(world, 0.0F, *wall_z - 6.0F, true);
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  CommanderControlController controller;
  controller.set_view_yaw(0.0F);

  walk(world, *commander, controller, true, 0.6F);
  float const flat_start = transform->position.z;
  walk(world, *commander, controller, true, 0.5F);
  float const flat_speed = (transform->position.z - flat_start) / 0.5F;
  ASSERT_FALSE(wall_cell_at(0.0F, transform->position.z))
      << "the flat sample ran onto the hillside";

  float slowest = flat_speed;
  for (int tick = 0; tick < 8 * 60; ++tick) {
    float const before = transform->position.z;
    walk(world, *commander, controller, true, k_dt);
    if (wall_cell_at(0.0F, before) && wall_cell_at(0.0F, transform->position.z)) {
      slowest = std::min(slowest, (transform->position.z - before) / k_dt);
    }
    if (crown_at(transform->position.x, transform->position.z)) {
      break;
    }
  }
  EXPECT_LT(slowest, flat_speed * 0.92F) << "the climb cost nothing";
  EXPECT_GT(slowest, flat_speed * 0.25F) << "the climb all but stopped him";
}

TEST_F(CommanderHillScrambleTest, NobodyRestsOnASteepFace) {
  raise_hill(2.4F);
  std::optional<QVector3D> steep;
  for (float z = -(k_hill_radius + 12.0F); z < 0.0F && !steep.has_value(); z += 0.25F) {
    QVector3D const point(0.0F, 0.0F, z);
    auto const footing = Game::Systems::Walkability::slope_footing(point);
    if (footing.on_slope && footing.grade >= k_commander_slope_slide_grade) {
      steep = point;
    }
  }
  if (!steep.has_value()) {
    GTEST_SKIP() << "this hill has no face steep enough to slide on";
  }

  Engine::Core::World world;
  auto* commander = spawn_commander(world, steep->x(), steep->z(), true);
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  auto const& terrain = Game::Map::TerrainService::instance();
  float const start_height = terrain.get_terrain_height(steep->x(), steep->z());
  CommanderControlController controller;
  controller.set_view_yaw(0.0F);
  walk(world, *commander, controller, false, 4.0F);

  auto const footing = Game::Systems::Walkability::slope_footing(
      QVector3D(transform->position.x, 0.0F, transform->position.z));
  EXPECT_FALSE(footing.on_slope && footing.grade >= k_commander_slope_slide_grade)
      << "the commander stood still on a face too steep to stand on";
  EXPECT_LT(terrain.get_terrain_height(transform->position.x, transform->position.z),
            start_height)
      << "he should have come down, not gone up";
}

TEST_F(CommanderHillScrambleTest, HandingControlBackWalksHimOffTheHillside) {
  raise_hill(2.4F);
  auto const wall_z = first_wall_z();
  ASSERT_TRUE(wall_z.has_value());
  float const on_wall_z = *wall_z + 0.5F;
  ASSERT_TRUE(slope_cell_at(0.0F, on_wall_z));

  Engine::Core::World world;
  auto* commander = spawn_commander(world, 0.0F, on_wall_z, false);
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  for (int tick = 0; tick < 6 * 60; ++tick) {
    Game::Systems::MovementCollision::unstick_body(*commander, *transform, k_dt);
  }
  EXPECT_FALSE(wall_cell_at(transform->position.x, transform->position.z))
      << "a commander left on the hillside when control was handed back stayed "
         "somewhere the army's pathfinding cannot reach";
}

} // namespace
