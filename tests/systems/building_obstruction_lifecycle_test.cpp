#include <QVector3D>

#include <cmath>
#include <gtest/gtest.h>
#include <vector>

#include "app/persistence/game_state_restorer.h"
#include "core/component.h"
#include "core/entity.h"
#include "core/world.h"
#include "map/terrain_service.h"
#include "session/session_context.h"
#include "systems/building_collision_registry.h"
#include "systems/combat_system/damage_processor.h"
#include "systems/command_service.h"
#include "systems/movement_system.h"
#include "systems/nation_id.h"
#include "systems/nav_grid.h"
#include "systems/pathfinding.h"
#include "systems/wall_network_service.h"

using namespace Engine::Core;
using namespace Game::Systems;

namespace {

constexpr int k_grid_size = 32;

class BuildingObstructionLifecycleTest : public ::testing::Test {
protected:
  Game::Session::SessionContext m_session;
  Game::Session::ScopedSession m_scope{m_session};

  void SetUp() override {
    BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    NavGrid::initialize(k_grid_size, k_grid_size);
  }

  void TearDown() override {
    Game::Map::TerrainService::instance().clear();
    BuildingCollisionRegistry::instance().clear();
  }

  static auto pathfinder() -> Pathfinding& {
    auto* pf = NavGrid::get_pathfinder();
    EXPECT_NE(pf, nullptr);
    return *pf;
  }

  static auto grid_cell(float world_x, float world_z) -> Point {
    return NavGrid::world_to_grid(world_x, world_z);
  }

  static auto cell_is_walkable(float world_x, float world_z) -> bool {
    auto& pf = pathfinder();
    pf.update_navigation_grid();
    Point const cell = grid_cell(world_x, world_z);
    return pf.is_walkable(cell.x, cell.y);
  }

  auto make_building(World& world,
                     const std::string& building_type,
                     Game::Units::SpawnType spawn_type,
                     float x,
                     float z,
                     int owner_id,
                     int health = 400) -> Entity* {
    auto* entity = world.create_entity();
    entity->add_component<TransformComponent>(x, 0.0F, z);
    entity->add_component<RenderableComponent>("mesh", "texture");
    auto* unit = entity->add_component<UnitComponent>(health, health, 0.0F, 0.0F);
    unit->owner_id = owner_id;
    unit->spawn_type = spawn_type;
    unit->nation_id = Game::Systems::NationID::RomanRepublic;
    entity->add_component<BuildingComponent>();

    if (spawn_type == Game::Units::SpawnType::WallSegment) {
      auto* wall = entity->add_component<WallSegmentComponent>();
      auto const snapped = WallNetworkService::snap_world_position(x, z);
      wall->grid_x = snapped.x;
      wall->grid_z = snapped.z;
    }

    BuildingCollisionRegistry::instance().register_building(
        entity->get_id(), building_type, x, z, owner_id);
    return entity;
  }

  auto
  make_wall(World& world, float x, float z, int owner_id, int health = 400) -> Entity* {
    return make_building(world,
                         "wall_segment",
                         Game::Units::SpawnType::WallSegment,
                         x,
                         z,
                         owner_id,
                         health);
  }

  auto make_attacker(World& world, int owner_id) -> Entity* {
    auto* attacker = world.create_entity();
    attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
    auto* unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
    unit->owner_id = owner_id;
    unit->spawn_type = Game::Units::SpawnType::Catapult;
    return attacker;
  }

  auto make_soldier(World& world, float x, float z, int owner_id) -> Entity* {
    auto* soldier = world.create_entity();
    soldier->add_component<TransformComponent>(x, 0.0F, z);
    soldier->add_component<MovementComponent>();
    auto* unit = soldier->add_component<UnitComponent>(100, 100, 4.0F, 10.0F);
    unit->owner_id = owner_id;
    unit->spawn_type = Game::Units::SpawnType::Knight;
    return soldier;
  }

  auto build_sealing_wall(World& world, int owner_id) -> std::vector<Entity*> {
    std::vector<Entity*> segments;
    for (float z = -16.0F; z <= 16.0F; z += 2.0F) {
      segments.push_back(make_wall(world, 0.0F, z, owner_id));
    }
    return segments;
  }

  static void destroy_by_combat(World& world, Entity* target, Entity* attacker) {
    auto* unit = target->get_component<UnitComponent>();
    ASSERT_NE(unit, nullptr);
    Game::Systems::Combat::deal_damage(
        &world, target, unit->max_health, attacker->get_id());
  }
};

TEST_F(BuildingObstructionLifecycleTest, WallSegmentBlocksAndReleasesNavigationCells) {
  World& world = m_session.world();

  auto* attacker = make_attacker(world, 1);
  auto* wall = make_wall(world, 0.0F, 0.0F, 2);

  EXPECT_FALSE(cell_is_walkable(0.0F, 0.0F));
  EXPECT_FALSE(cell_is_walkable(-0.5F, -0.5F));

  destroy_by_combat(world, wall, attacker);

  EXPECT_TRUE(cell_is_walkable(0.0F, 0.0F));
  EXPECT_TRUE(cell_is_walkable(-0.5F, -0.5F));
}

TEST_F(BuildingObstructionLifecycleTest, DestroyedWallOpensPassableBreachInSealedWall) {
  World& world = m_session.world();

  auto* attacker = make_attacker(world, 1);
  auto segments = build_sealing_wall(world, 2);

  auto& pf = pathfinder();
  pf.update_navigation_grid();

  Point const start = grid_cell(-8.0F, 0.0F);
  Point const goal = grid_cell(8.0F, 0.0F);

  auto const sealed_path = pf.find_path(start, goal);
  ASSERT_FALSE(sealed_path.empty());
  EXPECT_NE(sealed_path.back(), goal) << "wall should seal the map before destruction";

  Entity* breach_segment = nullptr;
  for (auto* segment : segments) {
    auto const* transform = segment->get_component<TransformComponent>();
    if (transform != nullptr && std::fabs(transform->position.z) < 0.01F) {
      breach_segment = segment;
      break;
    }
  }
  ASSERT_NE(breach_segment, nullptr);

  destroy_by_combat(world, breach_segment, attacker);

  pf.update_navigation_grid();
  EXPECT_TRUE(cell_is_walkable(0.0F, 0.0F));

  auto const breached_path = pf.find_path(start, goal);
  ASSERT_FALSE(breached_path.empty());
  EXPECT_EQ(breached_path.back(), goal)
      << "units should be able to route through the breach";
}

TEST_F(BuildingObstructionLifecycleTest, DestroyedTowerSocketOpensTheWallLine) {
  World& world = m_session.world();

  auto* attacker = make_attacker(world, 1);
  auto segments = build_sealing_wall(world, 2);

  Entity* middle = nullptr;
  for (auto* segment : segments) {
    auto const* transform = segment->get_component<TransformComponent>();
    if (transform != nullptr && std::fabs(transform->position.z) < 0.01F) {
      middle = segment;
      break;
    }
  }
  ASSERT_NE(middle, nullptr);
  world.destroy_entity(middle->get_id());

  auto* tower = make_building(
      world, "defense_tower", Game::Units::SpawnType::DefenseTower, 0.0F, 0.0F, 2, 800);

  auto& pf = pathfinder();
  pf.update_navigation_grid();

  Point const start = grid_cell(-8.0F, 0.0F);
  Point const goal = grid_cell(8.0F, 0.0F);
  EXPECT_NE(pf.find_path(start, goal).back(), goal);

  destroy_by_combat(world, tower, attacker);

  pf.update_navigation_grid();
  auto const breached_path = pf.find_path(start, goal);
  ASSERT_FALSE(breached_path.empty());
  EXPECT_EQ(breached_path.back(), goal);
}

TEST_F(BuildingObstructionLifecycleTest, LargeBuildingReleasesItsWholeFootprint) {
  World& world = m_session.world();

  auto* attacker = make_attacker(world, 1);
  auto* barracks = make_building(
      world, "barracks", Game::Units::SpawnType::Barracks, 0.0F, 0.0F, 2, 600);

  std::vector<QVector3D> const probes = {{0.0F, 0.0F, 0.0F},
                                         {2.0F, 0.0F, 2.0F},
                                         {-2.0F, 0.0F, -2.0F},
                                         {2.5F, 0.0F, 0.0F},
                                         {0.0F, 0.0F, -2.5F}};

  for (const auto& probe : probes) {
    EXPECT_FALSE(cell_is_walkable(probe.x(), probe.z()))
        << "expected blocked cell at " << probe.x() << "," << probe.z();
  }

  destroy_by_combat(world, barracks, attacker);

  for (const auto& probe : probes) {
    EXPECT_TRUE(cell_is_walkable(probe.x(), probe.z()))
        << "expected released cell at " << probe.x() << "," << probe.z();
    EXPECT_FALSE(BuildingCollisionRegistry::instance().is_point_in_building(probe.x(),
                                                                            probe.z()));
  }
}

TEST_F(BuildingObstructionLifecycleTest, RepeatedBuildAndDestroyKeepsCellsConsistent) {
  World& world = m_session.world();

  auto* attacker = make_attacker(world, 1);

  for (int iteration = 0; iteration < 3; ++iteration) {
    auto* wall = make_wall(world, 4.0F, 4.0F, 2);
    EXPECT_FALSE(cell_is_walkable(4.0F, 4.0F)) << "iteration " << iteration;

    destroy_by_combat(world, wall, attacker);
    EXPECT_TRUE(cell_is_walkable(4.0F, 4.0F)) << "iteration " << iteration;
    EXPECT_FALSE(BuildingCollisionRegistry::instance().is_point_in_building(4.0F, 4.0F))
        << "iteration " << iteration;

    world.destroy_entity(wall->get_id());
    EXPECT_TRUE(cell_is_walkable(4.0F, 4.0F)) << "iteration " << iteration;
  }
}

TEST_F(BuildingObstructionLifecycleTest,
       EntityRemovalWithoutCombatReleasesObstruction) {
  World& world = m_session.world();

  auto* wall = make_wall(world, -6.0F, 2.0F, 1);
  EXPECT_FALSE(cell_is_walkable(-6.0F, 2.0F));

  world.destroy_entity(wall->get_id());

  EXPECT_TRUE(cell_is_walkable(-6.0F, 2.0F));
  EXPECT_FALSE(BuildingCollisionRegistry::instance().is_point_in_building(-6.0F, 2.0F));
}

TEST_F(BuildingObstructionLifecycleTest, DestructionReleasesAuthoredMapObstacle) {
  World& world = m_session.world();

  auto* attacker = make_attacker(world, 1);

  std::vector<BuildingFootprint> authored;
  authored.emplace_back(0.0F, 0.0F, 2.0F, 2.0F, 0, 0U);
  BuildingCollisionRegistry::instance().set_authored_obstacles(std::move(authored));

  auto* wall = make_wall(world, 0.0F, 0.0F, 2);

  EXPECT_TRUE(BuildingCollisionRegistry::instance().is_point_in_building(0.0F, 0.0F));

  destroy_by_combat(world, wall, attacker);

  EXPECT_FALSE(BuildingCollisionRegistry::instance().is_point_in_building(0.0F, 0.0F))
      << "authored map obstacle must not outlive the structure it describes";
  EXPECT_TRUE(cell_is_walkable(0.0F, 0.0F));
}

TEST_F(BuildingObstructionLifecycleTest, ReloadDoesNotRestoreDestroyedStructures) {
  World& world = m_session.world();

  auto* attacker = make_attacker(world, 1);
  auto* wall = make_wall(world, 2.0F, 0.0F, 2);
  make_wall(world, -6.0F, 0.0F, 2);

  destroy_by_combat(world, wall, attacker);
  ASSERT_TRUE(wall->has_component<PendingRemovalComponent>());

  std::vector<BuildingFootprint> authored;
  authored.emplace_back(2.0F, 0.0F, 2.0F, 2.0F, 0, 0U);
  BuildingCollisionRegistry::instance().set_authored_obstacles(std::move(authored));

  GameStateRestorer::rebuild_building_collisions(&world);

  EXPECT_FALSE(BuildingCollisionRegistry::instance().is_point_in_building(2.0F, 0.0F));
  EXPECT_TRUE(cell_is_walkable(2.0F, 0.0F));

  EXPECT_TRUE(BuildingCollisionRegistry::instance().is_point_in_building(-6.0F, 0.0F));
  EXPECT_FALSE(cell_is_walkable(-6.0F, 0.0F));
}

TEST_F(BuildingObstructionLifecycleTest, UnitsRerouteThroughNewlyOpenedBreach) {
  World& world = m_session.world();

  auto* attacker = make_attacker(world, 1);
  auto segments = build_sealing_wall(world, 2);

  std::vector<Entity*> soldiers = {make_soldier(world, -12.0F, -1.0F, 1),
                                   make_soldier(world, -12.0F, 0.0F, 1),
                                   make_soldier(world, -12.0F, 1.0F, 1)};

  QVector3D const destination(8.0F, 0.0F, 0.0F);
  for (auto* soldier : soldiers) {
    CommandService::move_unit(world, soldier->get_id(), destination);
  }

  MovementSystem movement_system;
  constexpr float k_step = 1.0F / 30.0F;

  for (int step = 0; step < 45; ++step) {
    movement_system.update(&world, k_step);
  }

  for (auto* soldier : soldiers) {
    auto const* transform = soldier->get_component<TransformComponent>();
    ASSERT_NE(transform, nullptr);
    EXPECT_LT(transform->position.x, -1.0F)
        << "sealed wall should keep units on the near side";
  }

  Entity* breach_segment = nullptr;
  for (auto* segment : segments) {
    auto const* transform = segment->get_component<TransformComponent>();
    if (transform != nullptr && std::fabs(transform->position.z) < 0.01F) {
      breach_segment = segment;
      break;
    }
  }
  ASSERT_NE(breach_segment, nullptr);
  destroy_by_combat(world, breach_segment, attacker);

  for (int step = 0; step < 600; ++step) {
    movement_system.update(&world, k_step);
  }

  for (auto* soldier : soldiers) {
    auto const* transform = soldier->get_component<TransformComponent>();
    ASSERT_NE(transform, nullptr);
    EXPECT_GT(transform->position.x, 0.0F)
        << "units should walk through the breach once the wall is destroyed";
  }
}

} // namespace
