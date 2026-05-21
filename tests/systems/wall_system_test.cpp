#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "core/component.h"
#include "core/entity.h"
#include "core/world.h"
#include "game/map/map_transformer.h"
#include "systems/ai_system/ai_strategy.h"
#include "systems/ai_system/ai_types.h"
#include "systems/building_collision_registry.h"
#include "systems/combat_system/damage_processor.h"
#include "systems/command_service.h"
#include "systems/nation_id.h"
#include "systems/pathfinding.h"
#include "systems/player_resource_registry.h"
#include "systems/production_system.h"
#include "systems/wall_network_service.h"
#include "units/factory.h"
#include "units/spawn_type.h"

using namespace Engine::Core;
using namespace Game::Systems;

namespace {

class WallMechanicsTest : public ::testing::Test {
protected:
  void SetUp() override {
    BuildingCollisionRegistry::instance().clear();
    PlayerResourceRegistry::instance().clear();
    CommandService::initialize(8, 8);

    auto registry = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*registry);
    Game::Map::MapTransformer::setFactoryRegistry(std::move(registry));
  }

  void TearDown() override {
    Game::Map::MapTransformer::setFactoryRegistry(nullptr);
    PlayerResourceRegistry::instance().clear();
    BuildingCollisionRegistry::instance().clear();
  }

  auto make_wall(Engine::Core::World& world,
                 float x,
                 float y,
                 float z,
                 int owner_id,
                 int health = 800) -> Entity* {
    auto* wall = world.create_entity();
    wall->add_component<TransformComponent>(x, y, z);
    wall->add_component<RenderableComponent>("mesh", "texture");
    auto* unit = wall->add_component<UnitComponent>(health, 800, 0.0F, 0.0F);
    unit->owner_id = owner_id;
    unit->spawn_type = Game::Units::SpawnType::WallSegment;
    unit->nation_id = Game::Systems::NationID::RomanRepublic;
    wall->add_component<BuildingComponent>();
    auto* wall_segment = wall->add_component<WallSegmentComponent>();
    auto snapped = WallNetworkService::snap_world_position(x, z);
    wall_segment->grid_x = snapped.x;
    wall_segment->grid_z = snapped.z;
    BuildingCollisionRegistry::instance().register_building(
        wall->get_id(), "wall_segment", x, z, owner_id);
    return wall;
  }

  auto make_construction_site(Engine::Core::World& world,
                              float x,
                              float z,
                              int owner_id) -> Entity* {
    auto* site = world.create_entity();
    auto* transform = site->add_component<TransformComponent>(x, 0.0F, z);
    transform->rotation.y = 0.0F;
    auto* renderable = site->add_component<RenderableComponent>("mesh", "texture");
    renderable->visible = false;
    auto* wall = site->add_component<WallSegmentComponent>();
    auto snapped = WallNetworkService::snap_world_position(x, z);
    wall->grid_x = snapped.x;
    wall->grid_z = snapped.z;
    auto* construction = site->add_component<WallConstructionSiteComponent>();
    construction->owner_id = owner_id;
    construction->nation_id = Game::Systems::NationID::RomanRepublic;
    construction->build_time = 1.0F;
    construction->progress = 0.0F;
    return site;
  }

  auto
  make_tower(Engine::Core::World& world, float x, float z, int owner_id) -> Entity* {
    auto* tower = world.create_entity();
    tower->add_component<TransformComponent>(x, 0.0F, z);
    tower->add_component<RenderableComponent>("mesh", "texture");
    auto* unit = tower->add_component<UnitComponent>(2000, 2000, 0.0F, 20.0F);
    unit->owner_id = owner_id;
    unit->spawn_type = Game::Units::SpawnType::DefenseTower;
    unit->nation_id = Game::Systems::NationID::RomanRepublic;
    tower->add_component<BuildingComponent>();
    BuildingCollisionRegistry::instance().register_building(
        tower->get_id(), "defense_tower", x, z, owner_id);
    return tower;
  }
};

} // namespace

TEST_F(WallMechanicsTest, WallSegmentUsesDedicatedFootprint) {
  const auto size = BuildingCollisionRegistry::get_building_size("wall_segment");

  EXPECT_FLOAT_EQ(size.width, 2.0F);
  EXPECT_FLOAT_EQ(size.depth, 2.0F);
}

TEST_F(WallMechanicsTest, CombatDamageRemovesWallSegmentThroughBuildingLifecycle) {
  Engine::Core::World world;

  auto* attacker = world.create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  attacker_unit->spawn_type = Game::Units::SpawnType::Knight;

  auto* wall = make_wall(world, 4.0F, 0.0F, 1.0F, 2, 40);
  auto* renderable = wall->get_component<RenderableComponent>();
  ASSERT_NE(renderable, nullptr);

  EXPECT_TRUE(BuildingCollisionRegistry::instance().is_point_in_building(4.0F, 1.0F));

  Game::Systems::Combat::deal_damage(&world, wall, 40, attacker->get_id());

  EXPECT_TRUE(wall->has_component<PendingRemovalComponent>());
  EXPECT_FALSE(renderable->visible);
  EXPECT_FALSE(BuildingCollisionRegistry::instance().is_point_in_building(4.0F, 1.0F));
}

TEST_F(WallMechanicsTest, BuilderConstructionSpawnsWallSegment) {
  Engine::Core::World world;

  const QVector3D site = CommandService::grid_to_world(Game::Systems::Point{4, 4});

  auto* builder = world.create_entity();
  builder->add_component<TransformComponent>(site.x(), 0.0F, site.z());
  builder->add_component<MovementComponent>();
  auto* unit = builder->add_component<UnitComponent>(100, 100, 1.0F, 10.0F);
  unit->owner_id = 1;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  unit->spawn_type = Game::Units::SpawnType::Builder;

  auto* production = builder->add_component<BuilderProductionComponent>();
  production->in_progress = true;
  production->time_remaining = 0.0F;
  production->product_type = "wall_segment";
  production->has_construction_site = true;
  production->construction_site_x = site.x();
  production->construction_site_z = site.z();
  production->at_construction_site = true;

  Game::Systems::ProductionSystem system;
  system.update(&world, 0.1F);

  Entity* spawned_wall = nullptr;
  for (auto* entity : world.get_entities_with<UnitComponent>()) {
    if (entity == builder) {
      continue;
    }

    auto* spawned_unit = entity->get_component<UnitComponent>();
    if (spawned_unit != nullptr &&
        spawned_unit->spawn_type == Game::Units::SpawnType::WallSegment) {
      spawned_wall = entity;
      break;
    }
  }

  ASSERT_NE(spawned_wall, nullptr);
  EXPECT_TRUE(spawned_wall->has_component<BuildingComponent>());
  EXPECT_TRUE(
      BuildingCollisionRegistry::instance().is_point_in_building(site.x(), site.z()));
  EXPECT_TRUE(production->construction_complete);
  EXPECT_FALSE(production->has_construction_site);
  EXPECT_FALSE(production->in_progress);
}

TEST_F(WallMechanicsTest, AdjacentWallsRefreshConnectionVisuals) {
  Engine::Core::World world;

  auto* left = make_wall(world, 0.0F, 0.0F, 0.0F, 1);
  auto* right = make_wall(world, 2.0F, 0.0F, 0.0F, 1);

  WallNetworkService::refresh_world(world);

  const auto* left_wall = left->get_component<WallSegmentComponent>();
  const auto* right_wall = right->get_component<WallSegmentComponent>();
  const auto* left_renderable = left->get_component<RenderableComponent>();
  const auto* right_renderable = right->get_component<RenderableComponent>();

  ASSERT_NE(left_wall, nullptr);
  ASSERT_NE(right_wall, nullptr);
  ASSERT_NE(left_renderable, nullptr);
  ASSERT_NE(right_renderable, nullptr);

  EXPECT_EQ(left_wall->connection_mask, WallNetworkService::k_connection_east);
  EXPECT_EQ(right_wall->connection_mask, WallNetworkService::k_connection_west);
  EXPECT_NE(left_renderable->renderer_id.find("wall_segment_end"), std::string::npos);
  EXPECT_NE(right_renderable->renderer_id.find("wall_segment_end"), std::string::npos);
}

TEST_F(WallMechanicsTest, IsolatedWallsPreserveChosenOrientationAcrossRefresh) {
  Engine::Core::World world;

  auto* wall = make_wall(world, 0.0F, 0.0F, 0.0F, 1);
  auto* transform = wall->get_component<TransformComponent>();
  ASSERT_NE(transform, nullptr);
  transform->rotation.y = 90.0F;

  WallNetworkService::refresh_world(world);

  EXPECT_FLOAT_EQ(transform->rotation.y, 90.0F);
}

TEST_F(WallMechanicsTest, EnemyWallsDoNotMergeConnectionVisuals) {
  Engine::Core::World world;

  auto* left = make_wall(world, 0.0F, 0.0F, 0.0F, 1);
  auto* right = make_wall(world, 2.0F, 0.0F, 0.0F, 2);

  WallNetworkService::refresh_world(world);

  const auto* left_wall = left->get_component<WallSegmentComponent>();
  const auto* right_wall = right->get_component<WallSegmentComponent>();
  ASSERT_NE(left_wall, nullptr);
  ASSERT_NE(right_wall, nullptr);

  EXPECT_EQ(left_wall->connection_mask, 0);
  EXPECT_EQ(right_wall->connection_mask, 0);
}

TEST_F(WallMechanicsTest, WallConnectionsIncludeFriendlyTowerSockets) {
  Engine::Core::World world;

  auto* wall = make_wall(world, 0.0F, 0.0F, 0.0F, 1);
  make_tower(world, 2.0F, 0.0F, 1);

  WallNetworkService::refresh_world(world);

  const auto* wall_segment = wall->get_component<WallSegmentComponent>();
  ASSERT_NE(wall_segment, nullptr);
  EXPECT_EQ(wall_segment->connection_mask, WallNetworkService::k_connection_east);
}

TEST_F(WallMechanicsTest, ConstructionSitesUseSiteOwnerForFriendlyTowerSockets) {
  Engine::Core::World world;

  auto* site = make_construction_site(world, 0.0F, 0.0F, 1);
  make_tower(world, 2.0F, 0.0F, 1);

  WallNetworkService::refresh_world(world);

  const auto* wall_segment = site->get_component<WallSegmentComponent>();
  ASSERT_NE(wall_segment, nullptr);
  EXPECT_EQ(wall_segment->connection_mask, WallNetworkService::k_connection_east);
}

TEST_F(WallMechanicsTest, TowerSnapSocketFindsNearestFriendlyWallEndpoint) {
  Engine::Core::World world;

  make_wall(world, 0.0F, 0.0F, 0.0F, 1);
  const auto wall_grid = WallNetworkService::snap_world_position(0.0F, 0.0F);
  const auto endpoint_world = CommandService::grid_to_world(Game::Systems::Point{
      wall_grid.x + WallNetworkService::k_segment_spacing, wall_grid.z});

  const auto snapped = WallNetworkService::find_tower_snap_socket(
      world, 1, endpoint_world.x() + 0.1F, endpoint_world.z() + 0.1F);

  ASSERT_TRUE(snapped.has_value());
  EXPECT_EQ(snapped->x, wall_grid.x + WallNetworkService::k_segment_spacing);
  EXPECT_EQ(snapped->z, wall_grid.z);
}

TEST_F(WallMechanicsTest, TowerSnapSocketRejectsOccupiedEndpoint) {
  Engine::Core::World world;

  make_wall(world, 0.0F, 0.0F, 0.0F, 1);
  make_tower(world, 2.0F, 0.0F, 1);
  const auto wall_grid = WallNetworkService::snap_world_position(0.0F, 0.0F);
  const auto endpoint_world = CommandService::grid_to_world(Game::Systems::Point{
      wall_grid.x + WallNetworkService::k_segment_spacing, wall_grid.z});

  const auto snapped = WallNetworkService::find_tower_snap_socket(
      world, 1, endpoint_world.x() + 0.1F, endpoint_world.z() + 0.1F);

  EXPECT_FALSE(snapped.has_value());
}

TEST_F(WallMechanicsTest, BuilderConstructionQueueConsumesMultipleWallSites) {
  Engine::Core::World world;

  auto* first_site = make_construction_site(world, 2.0F, 0.0F, 1);
  auto* second_site = make_construction_site(world, 4.0F, 0.0F, 1);

  auto* builder = world.create_entity();
  builder->add_component<TransformComponent>(2.0F, 0.0F, 0.0F);
  builder->add_component<MovementComponent>();
  auto* unit = builder->add_component<UnitComponent>(100, 100, 1.0F, 10.0F);
  unit->owner_id = 1;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  unit->spawn_type = Game::Units::SpawnType::Builder;

  auto* production = builder->add_component<BuilderProductionComponent>();
  production->product_type = "wall_segment";
  production->build_time = 1.0F;
  production->time_remaining = 0.0F;
  production->has_construction_site = true;
  production->construction_site_entity_id = first_site->get_id();
  production->construction_site_x = 2.0F;
  production->construction_site_z = 0.0F;
  production->at_construction_site = true;
  production->in_progress = true;
  production->queued_construction_site_ids.push_back(second_site->get_id());

  Game::Systems::ProductionSystem system;
  system.update(&world, 0.1F);

  EXPECT_EQ(world.get_entity(first_site->get_id()), nullptr);
  EXPECT_FALSE(production->has_construction_site);
  EXPECT_EQ(production->queued_construction_site_ids.size(), 1U);

  system.update(&world, 0.1F);

  EXPECT_TRUE(production->has_construction_site);
  EXPECT_EQ(production->construction_site_entity_id, second_site->get_id());
  EXPECT_FLOAT_EQ(production->construction_site_x, 4.0F);
}

TEST_F(WallMechanicsTest, InvalidatedWallSiteRefundsWoodAndAdvancesQueue) {
  Engine::Core::World world;

  PlayerResourceRegistry::instance().set(1, ResourceType::Wood, 0);
  PlayerResourceRegistry::instance().set(2, ResourceType::Wood, 0);

  auto* first_site = make_construction_site(world, 2.0F, 0.0F, 1);
  auto* second_site = make_construction_site(world, 4.0F, 0.0F, 1);
  make_wall(world, 2.0F, 0.0F, 0.0F, 3);

  auto* builder = world.create_entity();
  builder->add_component<TransformComponent>(2.0F, 0.0F, 0.0F);
  builder->add_component<MovementComponent>();
  auto* unit = builder->add_component<UnitComponent>(100, 100, 1.0F, 10.0F);
  unit->owner_id = 2;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  unit->spawn_type = Game::Units::SpawnType::Builder;

  auto* production = builder->add_component<BuilderProductionComponent>();
  production->product_type = "wall_segment";
  production->build_time = 1.0F;
  production->time_remaining = 0.0F;
  production->has_construction_site = true;
  production->construction_site_entity_id = first_site->get_id();
  production->construction_site_x = 2.0F;
  production->construction_site_z = 0.0F;
  production->at_construction_site = true;
  production->in_progress = true;
  production->queued_construction_site_ids.push_back(second_site->get_id());

  Game::Systems::ProductionSystem system;
  system.update(&world, 0.1F);

  EXPECT_EQ(world.get_entity(first_site->get_id()), nullptr);
  EXPECT_EQ(PlayerResourceRegistry::instance().get(1, ResourceType::Wood),
            WallNetworkService::k_wall_segment_wood_cost);
  EXPECT_EQ(PlayerResourceRegistry::instance().get(2, ResourceType::Wood), 0);
  EXPECT_TRUE(production->has_construction_site);
  EXPECT_EQ(production->construction_site_entity_id, second_site->get_id());
  EXPECT_FALSE(production->in_progress);
  EXPECT_FALSE(production->construction_complete);
  EXPECT_TRUE(production->queued_construction_site_ids.empty());
}

// ---- Builder production dispatch ----

TEST(WallBuilderProductionTest, WallSegmentSpawnTypeExists) {
  // Verify WallSegment is a distinct spawn type (compile-time check via cast)
  constexpr auto wall_type = Game::Units::SpawnType::WallSegment;
  EXPECT_NE(wall_type, Game::Units::SpawnType::DefenseTower);
  EXPECT_NE(wall_type, Game::Units::SpawnType::Barracks);
  EXPECT_NE(wall_type, Game::Units::SpawnType::Home);
}

TEST(WallBuilderProductionTest, BuilderProductionComponentProductTypeStorable) {
  BuilderProductionComponent comp;
  comp.product_type = "wall_segment";
  EXPECT_EQ(comp.product_type, "wall_segment");
}

// ---- AI strategy wall counts ----

TEST(AIWallStrategyTest, DefensiveStrategyWantsWalls) {
  auto config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Defensive);
  EXPECT_GT(config.desired_wall_segment_count, 0);
}

TEST(AIWallStrategyTest, RusherStrategyWantsNoWalls) {
  auto config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Rusher);
  EXPECT_EQ(config.desired_wall_segment_count, 0);
}

TEST(AIWallStrategyTest, AggressiveStrategyWantsNoWalls) {
  auto config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Aggressive);
  EXPECT_EQ(config.desired_wall_segment_count, 0);
}

TEST(AIWallStrategyTest, BalancedStrategyWantsAtLeastOneWall) {
  auto config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Balanced);
  EXPECT_GE(config.desired_wall_segment_count, 1);
}

TEST(AIWallStrategyTest, HighDefensePersonalityBoostsWallCount) {
  auto config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Balanced);
  const int baseline = config.desired_wall_segment_count;
  Game::Systems::AI::AIStrategyFactory::apply_personality(config, 0.5F, 0.8F, 0.5F);
  EXPECT_GE(config.desired_wall_segment_count, baseline);
}

// ---- AIContext wall_segment_count field ----

TEST(AIWallContextTest, AIContextHasWallSegmentCount) {
  Game::Systems::AI::AIContext ctx;
  ctx.wall_segment_count = 5;
  EXPECT_EQ(ctx.wall_segment_count, 5);
}

TEST(AIWallContextTest, MacroTargetsHasWallSegmentCount) {
  Game::Systems::AI::AIContext::MacroTargets targets;
  targets.wall_segment_count = 3;
  EXPECT_EQ(targets.wall_segment_count, 3);
}
