#include <algorithm>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "core/component_economy.h"
#include "core/component_presentation.h"
#include "core/world.h"
#include "game/command/command_dispatcher.h"
#include "game/game_config.h"
#include "game/map/map_definition.h"
#include "game/map/map_transformer.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/systems/builder_product_types.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/economy/construction_cost_catalog.h"
#include "game/systems/economy/marketplace_system.h"
#include "game/systems/economy/production_service.h"
#include "game/systems/economy/production_system.h"
#include "game/systems/economy/resource_delivery_system.h"
#include "game/systems/harvest_yields.h"
#include "game/systems/movement/movement_pipeline.h"
#include "game/systems/movement/order_service.h"
#include "game/systems/navigation/nav_grid.h"
#include "game/systems/navigation/pathfinding.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/troop_profile_service.h"
#include "game/units/factory.h"
#include "game/units/troop_catalog_loader.h"
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
  EXPECT_EQ(carry->amounts.get(Game::Systems::ResourceType::Wood),
            Game::Systems::k_cut_tree_wood_reward);
  EXPECT_EQ(Game::Systems::PlayerResourceRegistry::instance().get(
                1, Game::Systems::ResourceType::Wood),
            0)
      << "gathering alone must not move the counters";

  Game::Systems::ResourceDeliverySystem delivery;
  delivery.update(&world, 0.1F);

  EXPECT_EQ(Game::Systems::PlayerResourceRegistry::instance().get(
                1, Game::Systems::ResourceType::Wood),
            Game::Systems::k_cut_tree_wood_reward);
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
  EXPECT_EQ(carry->amounts.get(Game::Systems::ResourceType::Stone),
            Game::Systems::k_collect_stone_reward);
  EXPECT_EQ(Game::Systems::PlayerResourceRegistry::instance().get(
                1, Game::Systems::ResourceType::Stone),
            0)
      << "gathering alone must not move the counters";

  Game::Systems::ResourceDeliverySystem delivery;
  delivery.update(&world, 0.1F);

  EXPECT_EQ(Game::Systems::PlayerResourceRegistry::instance().get(
                1, Game::Systems::ResourceType::Stone),
            Game::Systems::k_collect_stone_reward);
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
  EXPECT_EQ(carry->amounts.get(Game::Systems::ResourceType::Iron),
            Game::Systems::k_collect_iron_ore_reward);
  EXPECT_EQ(Game::Systems::PlayerResourceRegistry::instance().get(
                1, Game::Systems::ResourceType::Iron),
            0)
      << "gathering alone must not move the counters";

  Game::Systems::ResourceDeliverySystem delivery;
  delivery.update(&world, 0.1F);

  EXPECT_EQ(Game::Systems::PlayerResourceRegistry::instance().get(
                1, Game::Systems::ResourceType::Iron),
            Game::Systems::k_collect_iron_ore_reward);
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

    Game::Systems::MovementPipeline movement_system;
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

struct CrewMember {
  Engine::Core::Entity* entity = nullptr;
  Engine::Core::BuilderProductionComponent* builder = nullptr;
};

auto add_site_crew(Engine::Core::World& world,
                   const std::string& product,
                   QVector3D site,
                   bool at_site) -> CrewMember {
  auto* entity = world.create_entity();
  entity->add_component<Engine::Core::TransformComponent>(
      site.x() + (at_site ? 0.0F : 40.0F), 0.0F, site.z());
  entity->add_component<Engine::Core::MovementComponent>();
  auto* unit = entity->add_component<Engine::Core::UnitComponent>();
  unit->owner_id = 1;
  unit->health = 100;
  unit->max_health = 100;
  unit->spawn_type = Game::Units::SpawnType::Builder;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  auto* builder = entity->add_component<Engine::Core::BuilderProductionComponent>();

  builder->product_type = product;
  builder->build_time = Game::Systems::construction_build_time(product);
  builder->time_remaining = builder->build_time;
  builder->has_construction_site = true;
  builder->construction_site_x = site.x();
  builder->construction_site_z = site.z();
  builder->at_construction_site = at_site;
  builder->in_progress = at_site;
  return {entity, builder};
}

auto count_spawned(Engine::Core::World& world, Game::Units::SpawnType type) -> int {
  int count = 0;
  for (auto* entity : world.collect_entities_with<Engine::Core::UnitComponent>()) {
    if (entity->get_component<Engine::Core::UnitComponent>()->spawn_type == type) {
      ++count;
    }
  }
  return count;
}

auto run_until_built(Engine::Core::World& world,
                     Game::Systems::ProductionSystem& system,
                     const std::vector<CrewMember>& crews,
                     float limit_seconds) -> float {
  constexpr float k_dt = 0.1F;
  float elapsed = 0.0F;
  while (elapsed < limit_seconds) {
    system.update(&world, k_dt);
    elapsed += k_dt;
    const bool anyone_working =
        std::any_of(crews.begin(), crews.end(), [](const CrewMember& crew) {
          return crew.builder->in_progress || crew.builder->has_construction_site;
        });
    if (!anyone_working) {
      break;
    }
  }
  return elapsed;
}

TEST_F(ProductionSystemTest, SeveralCrewsOnOneSiteRaiseOneBuildingTogether) {
  Game::Systems::NavGrid::initialize(64, 64);
  const std::string product = "home";
  const float full_time = Game::Systems::construction_build_time(product);
  const auto site = Game::Systems::NavGrid::grid_to_world({32, 32});

  Engine::Core::World world;
  std::vector<CrewMember> crews;
  for (int i = 0; i < 3; ++i) {
    crews.push_back(add_site_crew(world, product, site, true));
  }

  Game::Systems::ProductionSystem system;
  const float took = run_until_built(world, system, crews, full_time * 3.0F);

  EXPECT_EQ(count_spawned(world, Game::Units::SpawnType::Home), 1)
      << "one order is one building, however many crews raise it";
  EXPECT_LT(took, full_time * 0.5F) << "three crews must work faster than one";
  EXPECT_EQ(Game::Systems::PlayerResourceRegistry::instance().get(
                1, Game::Systems::ResourceType::Wood),
            0)
      << "helpers finishing must not refund the cost";
  for (const auto& crew : crews) {
    EXPECT_FALSE(crew.builder->in_progress);
    EXPECT_FALSE(crew.builder->has_construction_site) << "every crew is released";
  }
}

TEST_F(ProductionSystemTest, ACrewStillWalkingJoinsWorkAlreadyUnderWay) {
  Game::Systems::NavGrid::initialize(64, 64);
  const std::string product = "home";
  const float full_time = Game::Systems::construction_build_time(product);
  const auto site = Game::Systems::NavGrid::grid_to_world({32, 32});

  Engine::Core::World world;
  auto first = add_site_crew(world, product, site, true);
  auto late = add_site_crew(world, product, site, false);

  Game::Systems::ProductionSystem system;
  for (int i = 0; i < 10; ++i) {
    system.update(&world, 0.1F);
  }
  const float progress_before =
      1.0F - (first.builder->time_remaining / first.builder->build_time);
  EXPECT_GT(progress_before, 0.0F);

  auto* late_transform = late.entity->get_component<Engine::Core::TransformComponent>();
  late_transform->position.x = site.x();
  late_transform->position.z = site.z();
  system.update(&world, 0.1F);
  ASSERT_TRUE(late.builder->at_construction_site);
  system.update(&world, 0.1F);
  const float late_progress =
      1.0F - (late.builder->time_remaining / late.builder->build_time);
  EXPECT_GE(late_progress, progress_before)
      << "the late crew shares the site's progress";

  run_until_built(world, system, {first, late}, full_time * 3.0F);
  EXPECT_EQ(count_spawned(world, Game::Units::SpawnType::Home), 1);
}

TEST_F(ProductionSystemTest, CrewsOnDifferentSitesBuildSeparately) {
  Game::Systems::NavGrid::initialize(64, 64);
  const std::string product = "home";
  const float full_time = Game::Systems::construction_build_time(product);

  Engine::Core::World world;
  std::vector<CrewMember> crews{
      add_site_crew(
          world, product, Game::Systems::NavGrid::grid_to_world({16, 16}), true),
      add_site_crew(
          world, product, Game::Systems::NavGrid::grid_to_world({48, 48}), true)};

  Game::Systems::ProductionSystem system;
  run_until_built(world, system, crews, full_time * 3.0F);
  EXPECT_EQ(count_spawned(world, Game::Units::SpawnType::Home), 2);
}

auto site_ghosts(Engine::Core::World& world) -> std::vector<Engine::Core::Entity*> {
  std::vector<Engine::Core::Entity*> ghosts;
  for (auto* entity :
       world.collect_entities_with<Engine::Core::ConstructionPreviewComponent>()) {
    if (entity->get_component<Engine::Core::ConstructionPreviewComponent>()
            ->site_ghost) {
      ghosts.push_back(entity);
    }
  }
  return ghosts;
}

TEST_F(ProductionSystemTest, TheGhostStandsOverTheSiteUntilTheBuildingReplacesIt) {
  Game::Systems::NavGrid::initialize(64, 64);
  const std::string product = "home";
  const float full_time = Game::Systems::construction_build_time(product);
  const auto site = Game::Systems::NavGrid::grid_to_world({32, 32});

  Engine::Core::World world;
  std::vector<CrewMember> crews{add_site_crew(world, product, site, true)};
  Game::Systems::ProductionSystem system;

  system.update(&world, 0.1F);
  auto ghosts = site_ghosts(world);
  ASSERT_EQ(ghosts.size(), 1U) << "the ghost stays standing over the site";
  auto* ghost = ghosts.front();
  const auto* transform = ghost->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);
  EXPECT_NEAR(transform->position.x, site.x(), 0.01F);
  EXPECT_NEAR(transform->position.z, site.z(), 0.01F);
  EXPECT_FALSE(ghost->has_component<Engine::Core::UnitComponent>())
      << "a ghost is presentation only: no unit, no target, no population";

  const auto ghost_id = ghost->get_id();
  for (int tick = 0; tick < 5; ++tick) {
    system.update(&world, 0.1F);
  }
  ghosts = site_ghosts(world);
  ASSERT_EQ(ghosts.size(), 1U) << "the same ghost is kept, not respawned each tick";
  EXPECT_EQ(ghosts.front()->get_id(), ghost_id);
  EXPECT_GT(ghosts.front()
                ->get_component<Engine::Core::ConstructionPreviewComponent>()
                ->progress,
            0.0F);

  run_until_built(world, system, crews, full_time * 3.0F);
  EXPECT_EQ(count_spawned(world, Game::Units::SpawnType::Home), 1);
  EXPECT_TRUE(site_ghosts(world).empty()) << "the finished building replaces the ghost";
}

TEST_F(ProductionSystemTest, AnAbandonedSiteTakesItsGhostWithIt) {
  Game::Systems::NavGrid::initialize(64, 64);
  const std::string product = "home";
  const auto site = Game::Systems::NavGrid::grid_to_world({32, 32});

  Engine::Core::World world;
  auto crew = add_site_crew(world, product, site, true);
  Game::Systems::ProductionSystem system;

  system.update(&world, 0.1F);
  ASSERT_EQ(site_ghosts(world).size(), 1U);

  Game::Systems::OrderService::clear_builder_task(world, crew.entity);
  system.update(&world, 0.1F);
  EXPECT_TRUE(site_ghosts(world).empty())
      << "a crew pulled off the site leaves no ghost behind";
}

TEST_F(ProductionSystemTest, AGhostGoesWhenTheWholeCrewDies) {
  Game::Systems::NavGrid::initialize(64, 64);
  const std::string product = "home";
  const auto site = Game::Systems::NavGrid::grid_to_world({32, 32});

  Engine::Core::World world;
  auto first = add_site_crew(world, product, site, true);
  auto second = add_site_crew(world, product, site, true);
  Game::Systems::ProductionSystem system;

  system.update(&world, 0.1F);
  ASSERT_EQ(site_ghosts(world).size(), 1U) << "one site, one ghost, two crews";

  world.destroy_entity(first.entity->get_id());
  system.update(&world, 0.1F);
  EXPECT_EQ(site_ghosts(world).size(), 1U)
      << "the survivor keeps the site, so the ghost keeps standing";

  world.destroy_entity(second.entity->get_id());
  system.update(&world, 0.1F);
  EXPECT_TRUE(site_ghosts(world).empty()) << "no crew left, no site, no ghost";
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
  for (auto* entity : world.collect_entities_with<Engine::Core::UnitComponent>()) {
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
  auto* completion =
      marketplace->get_component<Engine::Core::ProductionCompletionComponent>();
  ASSERT_NE(completion, nullptr);
  EXPECT_FLOAT_EQ(completion->remaining,
                  Engine::Core::ProductionCompletionComponent::k_duration);
  EXPECT_GT(completion->radius, 1.0F);
  EXPECT_FALSE(builder->has_component<Engine::Core::ProductionCompletionComponent>());
  EXPECT_TRUE(Game::Systems::MarketplaceSystem::owner_has_marketplace(world, 1));

  auto* marketplace_transform =
      marketplace->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(marketplace_transform, nullptr);
  EXPECT_FLOAT_EQ(marketplace_transform->rotation.y, 15.0F);

  EXPECT_FALSE(production->in_progress);
  EXPECT_TRUE(production->construction_complete);
  EXPECT_FALSE(production->has_construction_site);
}

TEST_F(ProductionSystemTest, BuilderFinishesTheSiegeEngineItWasAskedToBuild) {
  for (const auto& [product, expected] :
       {std::pair{"catapult", Game::Units::SpawnType::Catapult},
        std::pair{"ballista", Game::Units::SpawnType::Ballista}}) {
    SCOPED_TRACE(product);
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    auto const build_world = Game::Systems::NavGrid::grid_to_world({4, 4});

    Engine::Core::World world;
    auto* builder = world.create_entity();
    ASSERT_NE(builder, nullptr);
    builder->add_component<Engine::Core::TransformComponent>(
        build_world.x(), 0.0F, build_world.z());
    builder->add_component<Engine::Core::MovementComponent>();
    auto* unit = builder->add_component<Engine::Core::UnitComponent>();
    auto* production =
        builder->add_component<Engine::Core::BuilderProductionComponent>();
    unit->owner_id = 1;
    unit->spawn_type = Game::Units::SpawnType::Builder;
    unit->nation_id = Game::Systems::NationID::RomanRepublic;
    production->in_progress = true;
    production->time_remaining = 0.0F;
    production->product_type = product;
    production->has_construction_site = true;
    production->construction_site_x = build_world.x();
    production->construction_site_z = build_world.z();
    production->at_construction_site = true;

    Game::Systems::ProductionSystem system;
    system.update(&world, 0.1F);

    Engine::Core::Entity* engine = nullptr;
    for (auto* entity : world.collect_entities_with<Engine::Core::UnitComponent>()) {
      auto* spawned = entity->get_component<Engine::Core::UnitComponent>();
      if (entity != builder && spawned != nullptr && spawned->spawn_type == expected) {
        engine = entity;
      }
    }
    ASSERT_NE(engine, nullptr) << "the finished site must roll out a " << product;
    EXPECT_EQ(engine->get_component<Engine::Core::UnitComponent>()->owner_id, 1);
    EXPECT_TRUE(production->construction_complete);
    EXPECT_FALSE(production->has_construction_site);
  }
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

TEST_F(ProductionSystemTest, TwoCrewsHeldApartOnOneSiteBothStartWork) {

  auto const site = Game::Systems::NavGrid::grid_to_world({4, 4});
  Engine::Core::World world;
  std::vector<Engine::Core::BuilderProductionComponent*> crews;
  for (float const side : {-0.25F, 0.25F}) {
    auto* builder = world.create_entity();
    builder->add_component<Engine::Core::TransformComponent>(
        site.x() + side, 0.0F, site.z());
    builder->add_component<Engine::Core::MovementComponent>();
    builder->add_component<Engine::Core::UnitComponent>()->owner_id = 1;
    auto* production =
        builder->add_component<Engine::Core::BuilderProductionComponent>();
    production->product_type = "defense_tower";
    production->build_time = 10.0F;
    production->time_remaining = 10.0F;
    production->has_construction_site = true;
    production->construction_site_x = site.x();
    production->construction_site_z = site.z();
    crews.push_back(production);
  }

  Game::Systems::ProductionSystem system;
  system.update(&world, 0.1F);

  for (auto const* crew : crews) {
    EXPECT_TRUE(crew->at_construction_site)
        << "a crew held 0.25 m from its site by another crew never started work";
  }
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

TEST_F(ProductionSystemTest, ABuiltTempleCanRecruitHealersAndFillsFromDelivery) {
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
  production->product_type = "temple";
  production->has_construction_site = true;
  production->construction_site_x = build_world.x();
  production->construction_site_z = build_world.z();
  production->at_construction_site = true;

  Game::Systems::ProductionSystem system;
  system.update(&world, 0.1F);

  Engine::Core::Entity* temple = nullptr;
  for (auto* entity : world.collect_entities_with<Engine::Core::UnitComponent>()) {
    if (entity == builder) {
      continue;
    }
    auto* spawned_unit = entity->get_component<Engine::Core::UnitComponent>();
    if (spawned_unit != nullptr &&
        spawned_unit->spawn_type == Game::Units::SpawnType::Temple) {
      temple = entity;
      break;
    }
  }
  ASSERT_NE(temple, nullptr);

  auto* temple_production = temple->get_component<Engine::Core::ProductionComponent>();
  ASSERT_NE(temple_production, nullptr)
      << "a temple must be able to take recruits, or the healer card has nowhere to go";
  EXPECT_EQ(temple_production->product_type, Game::Units::TroopType::Healer);

  EXPECT_EQ(temple_production->manpower_available, 0);
  EXPECT_GT(temple_production->max_units, 0);
}

TEST_F(ProductionSystemTest, ACollectOrderStandsTheCrewOnTheNodeAndLeavesItThere) {

  Game::Map::MapDefinition map_def;
  map_def.grid.width = 16;
  map_def.grid.height = 16;
  map_def.grid.tile_size = 1.0F;
  map_def.world_props.push_back(
      {.type = Game::Map::WorldProp::Type::PineTree, .x = 8.0F, .z = 8.0F});

  Game::Systems::NavGrid::initialize(16, 16);
  auto& terrain = Game::Map::TerrainService::instance();
  terrain.initialize(map_def);
  ASSERT_EQ(terrain.world_props().size(), 1U);

  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();

  const QVector3D tree =
      terrain.world_prop_world_position(terrain.world_props().front());
  const auto tree_grid = Game::Systems::NavGrid::world_to_grid(tree.x(), tree.z());
  ASSERT_FALSE(pathfinder->is_walkable(tree_grid.x, tree_grid.y))
      << "the fixture is pointless unless the node blocks the ground it stands on";

  Engine::Core::World world;
  auto* builder = world.create_entity();
  ASSERT_NE(builder, nullptr);
  auto* transform = builder->add_component<Engine::Core::TransformComponent>(
      tree.x() - 5.0F, 0.0F, tree.z());
  builder->add_component<Engine::Core::MovementComponent>();
  auto* unit = builder->add_component<Engine::Core::UnitComponent>();
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Builder;
  unit->health = 720;
  unit->max_health = 720;
  unit->speed = 2.0F;
  builder->add_component<Engine::Core::BuilderProductionComponent>();

  Game::Command::dispatch(
      world,
      Game::Command::Command{.source = Game::Command::Source::Script,
                             .owner_id = 1,
                             .payload = Game::Command::StartHarvest{
                                 .units = {builder->get_id()},
                                 .construction_type = std::string(
                                     Game::Systems::k_builder_product_cut_tree),
                                 .resource_target = terrain.world_props().front().id,
                                 .site = tree}});

  auto* production = builder->get_component<Engine::Core::BuilderProductionComponent>();
  ASSERT_NE(production, nullptr);
  ASSERT_TRUE(production->has_construction_site);
  EXPECT_NEAR(production->construction_site_x, tree.x(), 0.0001F);
  EXPECT_NEAR(production->construction_site_z, tree.z(), 0.0001F);

  production->build_time = 1000.0F;
  production->time_remaining = production->build_time;

  Game::Systems::MovementPipeline movement;
  Game::Systems::ProductionSystem production_system;
  bool arrived = false;
  for (int step = 0; step < 400 && !arrived; ++step) {
    movement.update(&world, 0.05F);
    production_system.update(&world, 0.05F);
    arrived = production->at_construction_site;
  }

  ASSERT_TRUE(arrived) << "the crew never reached the node it was sent to work";
  EXPECT_NEAR(transform->position.x, tree.x(), 0.05F);
  EXPECT_NEAR(transform->position.z, tree.z(), 0.05F);

  for (int step = 0; step < 100; ++step) {
    movement.update(&world, 0.05F);
    production_system.update(&world, 0.05F);
  }

  EXPECT_TRUE(production->in_progress);
  EXPECT_NEAR(transform->position.x, tree.x(), 0.05F)
      << "the working crew was pushed off the node";
  EXPECT_NEAR(transform->position.z, tree.z(), 0.05F)
      << "the working crew was pushed off the node";
}

} // namespace

TEST_F(ProductionSystemTest, CompletionEffectExpiresAndIgnoresNegativeDelta) {
  Engine::Core::World world;
  auto* entity = world.create_entity();
  auto* effect = entity->add_component<Engine::Core::ProductionCompletionComponent>();
  Game::Systems::ProductionSystem system;
  system.update(&world, -1.0F);
  EXPECT_FLOAT_EQ(effect->remaining,
                  Engine::Core::ProductionCompletionComponent::k_duration);
  system.update(&world, 0.5F);
  EXPECT_FLOAT_EQ(effect->remaining,
                  Engine::Core::ProductionCompletionComponent::k_duration - 0.5F);
  system.update(&world, 10.0F);
  EXPECT_FALSE(entity->has_component<Engine::Core::ProductionCompletionComponent>());
}

TEST_F(ProductionSystemTest, RecruitmentGlowsOnTheRecruitOnly) {
  Engine::Core::World world;
  auto* barracks = world.create_entity();
  barracks->add_component<Engine::Core::TransformComponent>();
  auto* unit = barracks->add_component<Engine::Core::UnitComponent>();
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Barracks;
  auto* production = barracks->add_component<Engine::Core::ProductionComponent>();
  production->product_type = Game::Units::TroopType::Swordsman;
  production->in_progress = true;
  production->time_remaining = 0.0F;

  Game::Systems::ProductionSystem system;
  system.update(&world, 0.1F);

  const auto glowing =
      world.collect_entities_with<Engine::Core::ProductionCompletionComponent>();
  ASSERT_EQ(glowing.size(), 1U);
  EXPECT_NE(glowing.front()->get_id(), barracks->get_id());
  const auto* flare =
      glowing.front()->get_component<Engine::Core::ProductionCompletionComponent>();
  ASSERT_NE(flare, nullptr);
  EXPECT_EQ(flare->style, Engine::Core::SpawnFlareStyle::Recruit);
  EXPECT_FLOAT_EQ(flare->duration,
                  Engine::Core::ProductionCompletionComponent::k_duration);
  const auto* recruit = glowing.front()->get_component<Engine::Core::UnitComponent>();
  ASSERT_NE(recruit, nullptr);
  EXPECT_EQ(recruit->owner_id, 1);
  EXPECT_EQ(recruit->spawn_type, Game::Units::SpawnType::Swordsman);
  EXPECT_FALSE(barracks->has_component<Engine::Core::ProductionCompletionComponent>());
}

TEST_F(ProductionSystemTest, TheArrivalFlareReachesTheRenderSnapshot) {
  Engine::Core::World world;
  auto* recruit = world.create_entity();
  recruit->add_component<Engine::Core::TransformComponent>();
  recruit->add_component<Engine::Core::UnitComponent>()->owner_id = 1;
  recruit->add_component<Engine::Core::ProductionCompletionComponent>()->radius = 2.5F;

  Engine::Core::World snapshot;
  auto* copy = snapshot.create_entity();
  (void)Engine::Core::copy_render_components(*recruit, *copy);
  const auto* flare =
      copy->get_component<Engine::Core::ProductionCompletionComponent>();
  ASSERT_NE(flare, nullptr)
      << "the effect pass draws from the snapshot, never the live world";
  EXPECT_FLOAT_EQ(flare->radius, 2.5F);

  recruit->remove_component<Engine::Core::ProductionCompletionComponent>();
  (void)Engine::Core::copy_render_components(*recruit, *copy);
  EXPECT_FALSE(copy->has_component<Engine::Core::ProductionCompletionComponent>());
}

namespace {

class BarracksQueueTest : public ProductionSystemTest {
protected:
  void SetUp() override {
    ProductionSystemTest::SetUp();
    ASSERT_TRUE(Game::Units::TroopCatalogLoader::load_default_catalog());
    Game::Systems::TroopProfileService::instance().clear();
    Game::GameConfig::instance().set_max_troops_per_player(100000);
    barracks = world.create_entity();
    auto* unit = barracks->add_component<Engine::Core::UnitComponent>();
    unit->spawn_type = Game::Units::SpawnType::Barracks;
    unit->owner_id = 1;
    unit->health = 100;
    production = barracks->add_component<Engine::Core::ProductionComponent>();
    production->max_units = 1000;
    production->manpower_available = 1000;
    stock(1000);
  }

  void stock(int each) {
    auto& resources = Game::Systems::PlayerResourceRegistry::instance();
    for (const auto type : Game::Systems::k_all_resource_types) {
      resources.set(1, type, each);
    }
  }

  [[nodiscard]] auto in_line() const -> int {
    return (production->in_progress ? 1 : 0) +
           static_cast<int>(production->production_queue.size());
  }

  Engine::Core::World world;
  Engine::Core::Entity* barracks = nullptr;
  Engine::Core::ProductionComponent* production = nullptr;
  static constexpr auto k_line = Game::Units::TroopType::Swordsman;
};

TEST_F(BarracksQueueTest, ABarracksQueueHoldsTenRecruits) {
  using Game::Systems::ProductionService;
  EXPECT_EQ(ProductionService::start_production_batch(
                world, barracks->get_id(), k_line, 10),
            10);
  EXPECT_EQ(in_line(), 10);
  EXPECT_EQ(ProductionService::start_production(world, barracks->get_id(), k_line),
            Game::Systems::ProductionResult::QueueFull);
}

TEST_F(BarracksQueueTest, AShiftBatchStopsWhereTheReserveRunsOut) {
  using Game::Systems::ProductionService;
  const int cost = Game::Systems::TroopProfileService::instance()
                       .get_profile(Game::Systems::NationID::RomanRepublic, k_line)
                       .production.cost;
  production->manpower_available = cost * 3 + cost / 2;

  const auto plan = ProductionService::plan_batch(
      world, barracks->get_id(), k_line, ProductionService::k_recruit_batch);
  EXPECT_EQ(plan.count, 3);
  EXPECT_EQ(plan.limit, Game::Systems::ProductionResult::InsufficientManpower);

  EXPECT_EQ(ProductionService::start_production_batch(
                world, barracks->get_id(), k_line, plan.count),
            3);
  EXPECT_EQ(in_line(), 3);
}

TEST_F(BarracksQueueTest, AShiftBatchNamesTheResourceThatRanOutNotTheReserve) {
  using Game::Systems::ProductionService;
  const auto& profile = Game::Systems::TroopProfileService::instance().get_profile(
      Game::Systems::NationID::RomanRepublic, k_line);
  auto& resources = Game::Systems::PlayerResourceRegistry::instance();
  for (const auto type : Game::Systems::k_all_resource_types) {
    resources.set(1, type, profile.production.resource_costs.get(type) * 2);
  }
  const auto plan = ProductionService::plan_batch(
      world, barracks->get_id(), k_line, ProductionService::k_recruit_batch);
  EXPECT_EQ(plan.count, 2);
  EXPECT_EQ(plan.limit, Game::Systems::ProductionResult::InsufficientResources);
}

TEST_F(BarracksQueueTest, RepeatKeepsTheLineToppedUpUntilTheSecondClick) {
  using Game::Systems::ProductionService;
  EXPECT_TRUE(ProductionService::toggle_repeat(world, barracks->get_id(), k_line));
  ProductionService::top_up_repeats(world);
  EXPECT_EQ(in_line(), 1);
  for (int tick = 0; tick < 10; ++tick) {
    ProductionService::top_up_repeats(world);
  }
  EXPECT_EQ(in_line(), ProductionService::k_repeat_lookahead)
      << "repeat must keep a short line, not drain the stores into ten recruits";

  production->production_queue.clear();
  production->in_progress = false;
  ProductionService::top_up_repeats(world);
  EXPECT_GT(in_line(), 0) << "a line that emptied is refilled while repeat is on";

  EXPECT_FALSE(ProductionService::toggle_repeat(world, barracks->get_id(), k_line));
  production->production_queue.clear();
  production->in_progress = false;
  ProductionService::top_up_repeats(world);
  EXPECT_EQ(in_line(), 0) << "the second Ctrl-click stops the repeat";
}

TEST_F(BarracksQueueTest, RepeatWaitsForTheStoresInsteadOfGivingUp) {
  using Game::Systems::ProductionService;
  stock(0);
  ASSERT_TRUE(ProductionService::toggle_repeat(world, barracks->get_id(), k_line));
  ProductionService::top_up_repeats(world);
  EXPECT_EQ(in_line(), 0);
  EXPECT_TRUE(production->repeat_active);
  stock(1000);
  ProductionService::top_up_repeats(world);
  EXPECT_EQ(in_line(), 1);
}

} // namespace

namespace {

class BuilderQueueTest : public ProductionSystemTest {
protected:
  auto add_builder(Game::Session::SessionContext& session) -> Engine::Core::Entity* {
    auto& world = session.world();
    auto* builder = world.create_entity();
    builder->add_component<Engine::Core::TransformComponent>(2.0F, 0.0F, 2.0F);
    builder->add_component<Engine::Core::MovementComponent>();
    auto* unit = builder->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 10.0F);
    unit->owner_id = 1;
    unit->spawn_type = Game::Units::SpawnType::Builder;
    unit->health = 100;
    builder->add_component<Engine::Core::BuilderProductionComponent>();
    return builder;
  }
};

TEST_F(BuilderQueueTest, ACrewBetweenJobsStartsItsNextQueuedSite) {
  Game::Session::SessionContext session;
  const Game::Session::ScopedSession scope(session);
  auto* builder = add_builder(session);
  auto* work = builder->get_component<Engine::Core::BuilderProductionComponent>();
  work->queued_structure_sites.push_back(
      {.construction_type = "home", .x = 3.0F, .z = 3.0F, .rotation_y = 0.0F});
  work->queued_structure_sites.push_back(
      {.construction_type = "farm", .x = 5.0F, .z = 3.0F, .rotation_y = 0.0F});

  Game::Systems::ProductionSystem system;
  system.update(&session.world(), 0.016F);

  EXPECT_TRUE(work->has_construction_site);
  EXPECT_EQ(work->product_type, "home");
  EXPECT_FLOAT_EQ(work->construction_site_x, 3.0F);
  ASSERT_EQ(work->queued_structure_sites.size(), 1U);
  EXPECT_EQ(work->queued_structure_sites.front().construction_type, "farm");

  system.update(&session.world(), 0.016F);
  EXPECT_EQ(work->product_type, "home")
      << "the second site waits until the first one ends";
}

TEST_F(BuilderQueueTest, AFinishedCrewGoesBackToTheGatherJobItLeft) {
  Game::Session::SessionContext session;
  const Game::Session::ScopedSession scope(session);
  auto* builder = add_builder(session);
  auto* work = builder->get_component<Engine::Core::BuilderProductionComponent>();
  work->resume_auto_gather = true;
  work->resume_auto_gather_priority = "cut_tree";
  work->resume_gather_order = true;
  work->resume_gather_product_type = "collect_stone";
  work->resume_gather_anchor_x = 7.0F;

  Game::Systems::ProductionSystem system;
  system.update(&session.world(), 0.016F);

  EXPECT_TRUE(work->auto_gather);
  EXPECT_EQ(work->auto_gather_priority, "cut_tree");
  EXPECT_TRUE(work->has_gather_order);
  EXPECT_EQ(work->gather_product_type, "collect_stone");
  EXPECT_FLOAT_EQ(work->gather_anchor_x, 7.0F);
  EXPECT_FALSE(work->has_work_to_resume());
}

TEST_F(BuilderQueueTest, AnIdleCrewStartsGatheringWhatItsOwnerHasLeastOf) {
  Game::Session::SessionContext session;
  const Game::Session::ScopedSession scope(session);
  session.set_idle_builders_gather(true);
  auto& resources = Game::Systems::PlayerResourceRegistry::instance();
  resources.set(1, Game::Systems::ResourceType::Wood, 50);
  resources.set(1, Game::Systems::ResourceType::Stone, 5);
  resources.set(1, Game::Systems::ResourceType::Iron, 20);
  auto* builder = add_builder(session);
  auto* work = builder->get_component<Engine::Core::BuilderProductionComponent>();

  Game::Systems::ProductionSystem system;
  for (int second = 0; second < 9; ++second) {
    system.update(&session.world(), 1.0F);
  }
  EXPECT_FALSE(work->auto_gather) << "a crew that has been idle for 9 s is still resting";

  for (int second = 0; second < 2; ++second) {
    system.update(&session.world(), 1.0F);
  }
  EXPECT_TRUE(work->auto_gather);
  EXPECT_EQ(work->auto_gather_priority, "collect_stone");
}

TEST_F(BuilderQueueTest, IdleGatheringIsOffUnlessTheMatchAsksForIt) {
  Game::Session::SessionContext session;
  const Game::Session::ScopedSession scope(session);
  auto* builder = add_builder(session);
  auto* work = builder->get_component<Engine::Core::BuilderProductionComponent>();

  Game::Systems::ProductionSystem system;
  for (int second = 0; second < 30; ++second) {
    system.update(&session.world(), 1.0F);
  }
  EXPECT_FALSE(work->auto_gather)
      << "missions and the tutorial keep their builders where the script put them";
}

} // namespace
