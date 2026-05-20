#include <gtest/gtest.h>

#include "core/component.h"
#include "core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/command_service.h"
#include "game/systems/movement_system.h"
#include "game/systems/pathfinding.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/production_system.h"

namespace {

class ProductionSystemTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::PlayerResourceRegistry::instance().clear();
    Game::Systems::CommandService::initialize(8, 8);
  }

  void TearDown() override {
    Game::Systems::PlayerResourceRegistry::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
  }
};

TEST_F(ProductionSystemTest, BuilderChopsTreeAndAwardsWood) {
  auto const tree_world = Game::Systems::CommandService::grid_to_world({4, 4});
  auto const work_world = Game::Systems::CommandService::grid_to_world({3, 4});

  Game::Map::MapDefinition map_def;
  map_def.grid.width = 8;
  map_def.grid.height = 8;
  map_def.grid.tile_size = 1.0F;
  map_def.world_props.push_back(
      {.type = Game::Map::WorldProp::Type::PineTree, .x = 4.0F, .z = 4.0F});

  auto& terrain = Game::Map::TerrainService::instance();
  terrain.initialize(map_def);
  ASSERT_EQ(terrain.world_props().size(), 1U);

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_building_obstacles();
  EXPECT_TRUE(pathfinder->is_tree(4, 4));

  std::uint64_t const tree_id = terrain.world_props().front().id;
  ASSERT_TRUE(terrain.reserve_world_prop(tree_id));

  Engine::Core::World world;
  auto* builder = world.create_entity();
  ASSERT_NE(builder, nullptr);

  auto* transform = builder->add_component<Engine::Core::TransformComponent>(
      work_world.x(), 0.0F, work_world.z());
  auto* movement = builder->add_component<Engine::Core::MovementComponent>();
  auto* unit = builder->add_component<Engine::Core::UnitComponent>();
  auto* production = builder->add_component<Engine::Core::BuilderProductionComponent>();
  ASSERT_NE(transform, nullptr);
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(production, nullptr);

  unit->owner_id = 1;
  production->in_progress = true;
  production->time_remaining = 0.0F;
  production->product_type = "cut_tree";
  production->has_construction_site = true;
  production->construction_site_x = work_world.x();
  production->construction_site_z = work_world.z();
  production->at_construction_site = true;
  production->has_task_target = true;
  production->task_target_id = tree_id;
  production->task_target_x = tree_world.x();
  production->task_target_z = tree_world.z();
  production->task_target_reserved = true;

  Game::Systems::ProductionSystem system;
  system.update(&world, 0.1F);

  EXPECT_EQ(Game::Systems::PlayerResourceRegistry::instance().get(
                1, Game::Systems::ResourceType::Wood),
            25);
  EXPECT_TRUE(terrain.world_props().empty());

  pathfinder->update_building_obstacles();
  EXPECT_FALSE(pathfinder->is_tree(4, 4));
  EXPECT_TRUE(pathfinder->is_walkable(4, 4));

  EXPECT_FALSE(production->in_progress);
  EXPECT_TRUE(production->construction_complete);
  EXPECT_FALSE(production->has_construction_site);
  EXPECT_FALSE(production->has_task_target);
  EXPECT_FALSE(production->task_target_reserved);
}

TEST_F(ProductionSystemTest, BuilderCollectsStoneAndAwardsStone) {
  auto const boulder_world = Game::Systems::CommandService::grid_to_world({4, 4});
  auto const work_world = Game::Systems::CommandService::grid_to_world({3, 4});

  Game::Map::MapDefinition map_def;
  map_def.grid.width = 8;
  map_def.grid.height = 8;
  map_def.grid.tile_size = 1.0F;
  map_def.world_props.push_back(
      {.type = Game::Map::WorldProp::Type::Boulder, .x = 4.0F, .z = 4.0F});

  auto& terrain = Game::Map::TerrainService::instance();
  terrain.initialize(map_def);
  ASSERT_EQ(terrain.world_props().size(), 1U);

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_building_obstacles();
  EXPECT_TRUE(pathfinder->is_boulder(4, 4));

  std::uint64_t const boulder_id = terrain.world_props().front().id;
  ASSERT_TRUE(terrain.reserve_world_prop(boulder_id));

  Engine::Core::World world;
  auto* builder = world.create_entity();
  ASSERT_NE(builder, nullptr);

  auto* transform = builder->add_component<Engine::Core::TransformComponent>(
      work_world.x(), 0.0F, work_world.z());
  auto* movement = builder->add_component<Engine::Core::MovementComponent>();
  auto* unit = builder->add_component<Engine::Core::UnitComponent>();
  auto* production = builder->add_component<Engine::Core::BuilderProductionComponent>();
  ASSERT_NE(transform, nullptr);
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(production, nullptr);

  unit->owner_id = 1;
  production->in_progress = true;
  production->time_remaining = 0.0F;
  production->product_type = "collect_stone";
  production->has_construction_site = true;
  production->construction_site_x = work_world.x();
  production->construction_site_z = work_world.z();
  production->at_construction_site = true;
  production->has_task_target = true;
  production->task_target_id = boulder_id;
  production->task_target_x = boulder_world.x();
  production->task_target_z = boulder_world.z();
  production->task_target_reserved = true;

  Game::Systems::ProductionSystem system;
  system.update(&world, 0.1F);

  EXPECT_EQ(Game::Systems::PlayerResourceRegistry::instance().get(
                1, Game::Systems::ResourceType::Stone),
            25);
  EXPECT_TRUE(terrain.world_props().empty());

  pathfinder->update_building_obstacles();
  EXPECT_FALSE(pathfinder->is_boulder(4, 4));
  EXPECT_TRUE(pathfinder->is_walkable(4, 4));

  EXPECT_FALSE(production->in_progress);
  EXPECT_TRUE(production->construction_complete);
  EXPECT_FALSE(production->has_construction_site);
  EXPECT_FALSE(production->has_task_target);
  EXPECT_FALSE(production->task_target_reserved);
}

TEST_F(ProductionSystemTest, BuilderCollectsIronOreAndAwardsIron) {
  auto const iron_ore_world = Game::Systems::CommandService::grid_to_world({4, 4});
  auto const work_world = Game::Systems::CommandService::grid_to_world({3, 4});

  Game::Map::MapDefinition map_def;
  map_def.grid.width = 8;
  map_def.grid.height = 8;
  map_def.grid.tile_size = 1.0F;
  map_def.world_props.push_back(
      {.type = Game::Map::WorldProp::Type::IronOre, .x = 4.0F, .z = 4.0F});

  auto& terrain = Game::Map::TerrainService::instance();
  terrain.initialize(map_def);
  ASSERT_EQ(terrain.world_props().size(), 1U);

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_building_obstacles();
  EXPECT_TRUE(pathfinder->is_iron_ore(4, 4));

  std::uint64_t const iron_ore_id = terrain.world_props().front().id;
  ASSERT_TRUE(terrain.reserve_world_prop(iron_ore_id));

  Engine::Core::World world;
  auto* builder = world.create_entity();
  ASSERT_NE(builder, nullptr);

  auto* transform = builder->add_component<Engine::Core::TransformComponent>(
      work_world.x(), 0.0F, work_world.z());
  auto* movement = builder->add_component<Engine::Core::MovementComponent>();
  auto* unit = builder->add_component<Engine::Core::UnitComponent>();
  auto* production = builder->add_component<Engine::Core::BuilderProductionComponent>();
  ASSERT_NE(transform, nullptr);
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(production, nullptr);

  unit->owner_id = 1;
  production->in_progress = true;
  production->time_remaining = 0.0F;
  production->product_type = "collect_iron_ore";
  production->has_construction_site = true;
  production->construction_site_x = work_world.x();
  production->construction_site_z = work_world.z();
  production->at_construction_site = true;
  production->has_task_target = true;
  production->task_target_id = iron_ore_id;
  production->task_target_x = iron_ore_world.x();
  production->task_target_z = iron_ore_world.z();
  production->task_target_reserved = true;

  Game::Systems::ProductionSystem system;
  system.update(&world, 0.1F);

  EXPECT_EQ(Game::Systems::PlayerResourceRegistry::instance().get(
                1, Game::Systems::ResourceType::Iron),
            25);
  EXPECT_TRUE(terrain.world_props().empty());

  pathfinder->update_building_obstacles();
  EXPECT_FALSE(pathfinder->is_iron_ore(4, 4));
  EXPECT_TRUE(pathfinder->is_walkable(4, 4));

  EXPECT_FALSE(production->in_progress);
  EXPECT_TRUE(production->construction_complete);
  EXPECT_FALSE(production->has_construction_site);
  EXPECT_FALSE(production->has_task_target);
  EXPECT_FALSE(production->task_target_reserved);
}

TEST_F(ProductionSystemTest, HarvestingBuilderStaysCenteredOnResourceAnchor) {
  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);

  auto& terrain = Game::Map::TerrainService::instance();

  auto run_case = [&](Game::Map::WorldProp::Type type,
                      const char* product_type,
                      auto resource_check,
                      const char* label) {
    SCOPED_TRACE(label);

    terrain.clear();

    Game::Map::MapDefinition map_def;
    map_def.grid.width = 8;
    map_def.grid.height = 8;
    map_def.grid.tile_size = 1.0F;
    map_def.world_props.push_back({.type = type, .x = 4.0F, .z = 4.0F});
    terrain.initialize(map_def);
    ASSERT_EQ(terrain.world_props().size(), 1U);

    pathfinder->update_building_obstacles();
    EXPECT_TRUE((pathfinder->*resource_check)(4, 4));

    auto const target_world = Game::Systems::CommandService::grid_to_world({4, 4});
    std::uint64_t const target_id = terrain.world_props().front().id;
    ASSERT_TRUE(terrain.reserve_world_prop(target_id));

    Engine::Core::World world;
    auto* builder = world.create_entity();
    ASSERT_NE(builder, nullptr);

    auto* transform = builder->add_component<Engine::Core::TransformComponent>(
        target_world.x(), 0.0F, target_world.z());
    auto* movement = builder->add_component<Engine::Core::MovementComponent>();
    auto* unit = builder->add_component<Engine::Core::UnitComponent>();
    auto* production =
        builder->add_component<Engine::Core::BuilderProductionComponent>();
    ASSERT_NE(transform, nullptr);
    ASSERT_NE(movement, nullptr);
    ASSERT_NE(unit, nullptr);
    ASSERT_NE(production, nullptr);

    unit->spawn_type = Game::Units::SpawnType::Builder;
    movement->goal_x = target_world.x();
    movement->goal_y = target_world.z();
    movement->target_x = target_world.x();
    movement->target_y = target_world.z();
    movement->has_target = false;

    production->in_progress = true;
    production->build_time = 6.0F;
    production->time_remaining = 3.0F;
    production->product_type = product_type;
    production->has_construction_site = true;
    production->construction_site_x = target_world.x();
    production->construction_site_z = target_world.z();
    production->at_construction_site = true;
    production->has_task_target = true;
    production->task_target_id = target_id;
    production->task_target_x = target_world.x();
    production->task_target_z = target_world.z();
    production->task_target_reserved = true;

    Game::Systems::MovementSystem movement_system;
    movement_system.update(&world, 0.1F);

    EXPECT_FALSE(movement->has_target);
    EXPECT_NEAR(transform->position.x, target_world.x(), 0.0001F);
    EXPECT_NEAR(transform->position.z, target_world.z(), 0.0001F);
  };

  run_case(Game::Map::WorldProp::Type::PineTree,
           "cut_tree",
           &Game::Systems::Pathfinding::is_tree,
           "tree");
  run_case(Game::Map::WorldProp::Type::Boulder,
           "collect_stone",
           &Game::Systems::Pathfinding::is_boulder,
           "boulder");
  run_case(Game::Map::WorldProp::Type::IronOre,
           "collect_iron_ore",
           &Game::Systems::Pathfinding::is_iron_ore,
           "iron_ore");
}

} // namespace
