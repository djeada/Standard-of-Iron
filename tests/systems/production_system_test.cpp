#include <gtest/gtest.h>
#include <memory>

#include "core/component.h"
#include "core/world.h"
#include "game/map/map_definition.h"
#include "game/map/map_transformer.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/marketplace_system.h"
#include "game/systems/movement_system.h"
#include "game/systems/nav_grid.h"
#include "game/systems/pathfinding.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/production_system.h"
#include "game/systems/resource_delivery_system.h"
#include "game/units/factory.h"
#include "tests/support/movement_test_access.h"

namespace {

class ProductionSystemTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::PlayerResourceRegistry::instance().clear();
    Game::Systems::NavGrid::initialize(8, 8);

    auto registry = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*registry);
    Game::Map::MapTransformer::setFactoryRegistry(std::move(registry));
  }

  void TearDown() override {
    Game::Map::MapTransformer::setFactoryRegistry(nullptr);
    Game::Systems::PlayerResourceRegistry::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
  }
};

TEST_F(ProductionSystemTest, BuilderChopsTreeAndAwardsWood) {
  auto const tree_world = Game::Systems::NavGrid::grid_to_world({4, 4});
  auto const work_world = Game::Systems::NavGrid::grid_to_world({3, 4});

  Game::Map::MapDefinition map_def;
  map_def.grid.width = 8;
  map_def.grid.height = 8;
  map_def.grid.tile_size = 1.0F;
  map_def.world_props.push_back(
      {.type = Game::Map::WorldProp::Type::PineTree, .x = 4.0F, .z = 4.0F});

  auto& terrain = Game::Map::TerrainService::instance();
  terrain.initialize(map_def);
  ASSERT_EQ(terrain.world_props().size(), 1U);

  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();
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

  const auto* carry = builder->get_component<Engine::Core::ResourceCarryComponent>();
  ASSERT_NE(carry, nullptr) << "the gathered load should be on the worker's back";
  EXPECT_EQ(carry->amounts.get(Game::Systems::ResourceType::Wood), 40);
  EXPECT_EQ(Game::Systems::PlayerResourceRegistry::instance().get(
                1, Game::Systems::ResourceType::Wood),
            0)
      << "gathering alone must not move the counters";

  Game::Systems::ResourceDeliverySystem delivery;
  delivery.update(&world, 0.1F);

  EXPECT_EQ(Game::Systems::PlayerResourceRegistry::instance().get(
                1, Game::Systems::ResourceType::Wood),
            40);
  EXPECT_FALSE(builder->has_component<Engine::Core::ResourceCarryComponent>());
  EXPECT_TRUE(terrain.world_props().empty());

  pathfinder->update_navigation_grid();
  EXPECT_FALSE(pathfinder->is_tree(4, 4));
  EXPECT_TRUE(pathfinder->is_walkable(4, 4));

  EXPECT_FALSE(production->in_progress);
  EXPECT_TRUE(production->construction_complete);
  EXPECT_FALSE(production->has_construction_site);
  EXPECT_FALSE(production->has_task_target);
  EXPECT_FALSE(production->task_target_reserved);
}

TEST_F(ProductionSystemTest, BuilderCollectsStoneAndAwardsStone) {
  auto const boulder_world = Game::Systems::NavGrid::grid_to_world({4, 4});
  auto const work_world = Game::Systems::NavGrid::grid_to_world({3, 4});

  Game::Map::MapDefinition map_def;
  map_def.grid.width = 8;
  map_def.grid.height = 8;
  map_def.grid.tile_size = 1.0F;
  map_def.world_props.push_back(
      {.type = Game::Map::WorldProp::Type::Boulder, .x = 4.0F, .z = 4.0F});

  auto& terrain = Game::Map::TerrainService::instance();
  terrain.initialize(map_def);
  ASSERT_EQ(terrain.world_props().size(), 1U);

  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();
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

  const auto* carry = builder->get_component<Engine::Core::ResourceCarryComponent>();
  ASSERT_NE(carry, nullptr) << "the gathered load should be on the worker's back";
  EXPECT_EQ(carry->amounts.get(Game::Systems::ResourceType::Stone), 35);
  EXPECT_EQ(Game::Systems::PlayerResourceRegistry::instance().get(
                1, Game::Systems::ResourceType::Stone),
            0)
      << "gathering alone must not move the counters";

  Game::Systems::ResourceDeliverySystem delivery;
  delivery.update(&world, 0.1F);

  EXPECT_EQ(Game::Systems::PlayerResourceRegistry::instance().get(
                1, Game::Systems::ResourceType::Stone),
            35);
  EXPECT_FALSE(builder->has_component<Engine::Core::ResourceCarryComponent>());
  EXPECT_TRUE(terrain.world_props().empty());

  pathfinder->update_navigation_grid();
  EXPECT_FALSE(pathfinder->is_boulder(4, 4));
  EXPECT_TRUE(pathfinder->is_walkable(4, 4));

  EXPECT_FALSE(production->in_progress);
  EXPECT_TRUE(production->construction_complete);
  EXPECT_FALSE(production->has_construction_site);
  EXPECT_FALSE(production->has_task_target);
  EXPECT_FALSE(production->task_target_reserved);
}

TEST_F(ProductionSystemTest, BuilderCollectsIronOreAndAwardsIron) {
  auto const iron_ore_world = Game::Systems::NavGrid::grid_to_world({4, 4});
  auto const work_world = Game::Systems::NavGrid::grid_to_world({3, 4});

  Game::Map::MapDefinition map_def;
  map_def.grid.width = 8;
  map_def.grid.height = 8;
  map_def.grid.tile_size = 1.0F;
  map_def.world_props.push_back(
      {.type = Game::Map::WorldProp::Type::IronOre, .x = 4.0F, .z = 4.0F});

  auto& terrain = Game::Map::TerrainService::instance();
  terrain.initialize(map_def);
  ASSERT_EQ(terrain.world_props().size(), 1U);

  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();
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

  const auto* carry = builder->get_component<Engine::Core::ResourceCarryComponent>();
  ASSERT_NE(carry, nullptr) << "the gathered load should be on the worker's back";
  EXPECT_EQ(carry->amounts.get(Game::Systems::ResourceType::Iron), 30);
  EXPECT_EQ(Game::Systems::PlayerResourceRegistry::instance().get(
                1, Game::Systems::ResourceType::Iron),
            0)
      << "gathering alone must not move the counters";

  Game::Systems::ResourceDeliverySystem delivery;
  delivery.update(&world, 0.1F);

  EXPECT_EQ(Game::Systems::PlayerResourceRegistry::instance().get(
                1, Game::Systems::ResourceType::Iron),
            30);
  EXPECT_FALSE(builder->has_component<Engine::Core::ResourceCarryComponent>());
  EXPECT_TRUE(terrain.world_props().empty());

  pathfinder->update_navigation_grid();
  EXPECT_FALSE(pathfinder->is_iron_ore(4, 4));
  EXPECT_TRUE(pathfinder->is_walkable(4, 4));

  EXPECT_FALSE(production->in_progress);
  EXPECT_TRUE(production->construction_complete);
  EXPECT_FALSE(production->has_construction_site);
  EXPECT_FALSE(production->has_task_target);
  EXPECT_FALSE(production->task_target_reserved);
}

TEST_F(ProductionSystemTest, HarvestingBuilderStaysCenteredOnResourceAnchor) {
  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
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

    pathfinder->update_navigation_grid();
    EXPECT_TRUE((pathfinder->*resource_check)(4, 4));

    auto const target_world = Game::Systems::NavGrid::grid_to_world({4, 4});
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
    MovementTestAccess::set_goal_x(*movement, target_world.x());
    MovementTestAccess::set_goal_y(*movement, target_world.z());
    MovementTestAccess::set_target_x(*movement, target_world.x());
    MovementTestAccess::set_target_y(*movement, target_world.z());
    MovementTestAccess::set_has_target(*movement, false);

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

    EXPECT_FALSE(movement->get_has_target());
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

TEST_F(ProductionSystemTest, BuilderCompletesMarketplaceConstruction) {
  auto const build_world = Game::Systems::NavGrid::grid_to_world({4, 4});

  Engine::Core::World world;
  auto* builder = world.create_entity();
  ASSERT_NE(builder, nullptr);

  auto* transform = builder->add_component<Engine::Core::TransformComponent>(
      build_world.x(), 0.0F, build_world.z());
  auto* movement = builder->add_component<Engine::Core::MovementComponent>();
  auto* unit = builder->add_component<Engine::Core::UnitComponent>();
  auto* production = builder->add_component<Engine::Core::BuilderProductionComponent>();
  ASSERT_NE(transform, nullptr);
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(production, nullptr);

  unit->owner_id = 1;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  production->in_progress = true;
  production->time_remaining = 0.0F;
  production->product_type = "marketplace";
  production->has_construction_site = true;
  production->construction_site_x = build_world.x();
  production->construction_site_z = build_world.z();
  production->construction_site_rotation_y = 15.0F;
  production->at_construction_site = true;

  Game::Systems::ProductionSystem system;
  system.update(&world, 0.1F);

  Engine::Core::Entity* marketplace = nullptr;
  for (auto* entity : world.get_entities_with<Engine::Core::UnitComponent>()) {
    if (entity == builder) {
      continue;
    }
    auto* spawned_unit = entity->get_component<Engine::Core::UnitComponent>();
    if (spawned_unit != nullptr &&
        spawned_unit->spawn_type == Game::Units::SpawnType::Marketplace) {
      marketplace = entity;
      break;
    }
  }

  ASSERT_NE(marketplace, nullptr);
  EXPECT_TRUE(marketplace->has_component<Engine::Core::BuildingComponent>());
  EXPECT_TRUE(Game::Systems::MarketplaceSystem::owner_has_marketplace(world, 1));

  auto* marketplace_transform =
      marketplace->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(marketplace_transform, nullptr);
  EXPECT_FLOAT_EQ(marketplace_transform->rotation.y, 15.0F);

  EXPECT_FALSE(production->in_progress);
  EXPECT_TRUE(production->construction_complete);
  EXPECT_FALSE(production->has_construction_site);
}

TEST_F(ProductionSystemTest, RepairMendsAStructureOneTickAtATime) {
  Engine::Core::World world;

  auto* structure = world.create_entity();
  structure->add_component<Engine::Core::TransformComponent>(4.0F, 0.0F, 4.0F);
  structure->add_component<Engine::Core::BuildingComponent>();
  auto* structure_unit = structure->add_component<Engine::Core::UnitComponent>();
  structure_unit->owner_id = 1;
  structure_unit->max_health = 600;
  structure_unit->health = 200;

  auto* builder = world.create_entity();
  builder->add_component<Engine::Core::TransformComponent>(2.0F, 0.0F, 4.0F);
  builder->add_component<Engine::Core::MovementComponent>();
  builder->add_component<Engine::Core::UnitComponent>()->owner_id = 1;
  auto* production = builder->add_component<Engine::Core::BuilderProductionComponent>();
  production->product_type = "repair_structure";
  production->structure_task_entity_id = structure->get_id();
  production->build_time = 1.0F;
  production->time_remaining = 0.0F;
  production->has_construction_site = true;
  production->construction_site_x = 2.0F;
  production->construction_site_z = 4.0F;
  production->at_construction_site = true;
  production->in_progress = true;

  Game::Systems::ProductionSystem system;
  system.update(&world, 0.1F);

  EXPECT_GT(structure_unit->health, 200) << "a repair tick must restore health";
  EXPECT_LT(structure_unit->health, structure_unit->max_health)
      << "one tick must not mend the whole building";
  EXPECT_TRUE(production->in_progress) << "the crew keeps working while damage remains";
  EXPECT_FLOAT_EQ(production->time_remaining, production->build_time);

  for (int tick = 0; tick < 40; ++tick) {
    production->time_remaining = 0.0F;
    system.update(&world, 0.1F);
  }

  EXPECT_EQ(structure_unit->health, structure_unit->max_health);
  EXPECT_FALSE(production->in_progress);
  EXPECT_TRUE(production->construction_complete);
  EXPECT_EQ(production->structure_task_entity_id, 0U);
}

TEST_F(ProductionSystemTest, RepairingAStructureThatIsGoneReportsALostTarget) {
  Engine::Core::World world;

  auto* builder = world.create_entity();
  builder->add_component<Engine::Core::TransformComponent>(2.0F, 0.0F, 4.0F);
  builder->add_component<Engine::Core::MovementComponent>();
  builder->add_component<Engine::Core::UnitComponent>()->owner_id = 1;
  auto* production = builder->add_component<Engine::Core::BuilderProductionComponent>();
  production->product_type = "repair_structure";
  production->structure_task_entity_id = 4242U;
  production->build_time = 1.0F;
  production->time_remaining = 0.0F;
  production->has_construction_site = true;
  production->construction_site_x = 2.0F;
  production->construction_site_z = 4.0F;
  production->at_construction_site = true;
  production->in_progress = true;

  Game::Systems::ProductionSystem system;
  system.update(&world, 0.1F);

  EXPECT_TRUE(production->has_active_fault());
  EXPECT_EQ(production->fault, Engine::Core::BuilderTaskFault::TargetLost);
}

TEST_F(ProductionSystemTest, WalkingOffASiteIsRecordedAsAnInterruption) {
  Engine::Core::World world;

  auto* builder = world.create_entity();
  builder->add_component<Engine::Core::TransformComponent>(40.0F, 0.0F, 40.0F);
  builder->add_component<Engine::Core::MovementComponent>();
  builder->add_component<Engine::Core::UnitComponent>()->owner_id = 1;
  auto* production = builder->add_component<Engine::Core::BuilderProductionComponent>();
  production->product_type = "barracks";
  production->build_time = 10.0F;
  production->time_remaining = 5.0F;
  production->has_construction_site = true;
  production->construction_site_x = 2.0F;
  production->construction_site_z = 4.0F;
  production->at_construction_site = true;
  production->in_progress = true;

  Game::Systems::ProductionSystem system;
  system.update(&world, 0.1F);

  EXPECT_FALSE(production->in_progress);
  EXPECT_TRUE(production->has_active_fault());
  EXPECT_EQ(production->fault, Engine::Core::BuilderTaskFault::Interrupted);
}

} // namespace
