#include <QVector3D>

#include <cmath>
#include <gtest/gtest.h>

#include "core/component.h"
#include "core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/systems/builder_product_types.h"
#include "game/systems/gather_loop_system.h"
#include "game/systems/nav_grid.h"
#include "game/systems/order_service.h"
#include "game/units/spawn_type.h"

namespace {

class GatherLoopSystemTest : public ::testing::Test {
protected:
  void SetUp() override { lay_out_trees(3); }

  static void lay_out_trees(int tree_count) {
    Game::Map::MapDefinition map_def;
    map_def.grid.width = 64;
    map_def.grid.height = 64;
    map_def.grid.tile_size = 1.0F;
    for (int index = 0; index < tree_count; ++index) {
      map_def.world_props.push_back({.type = Game::Map::WorldProp::Type::PineTree,
                                     .x = 32.0F + (static_cast<float>(index) * 3.0F),
                                     .z = 32.0F});
    }
    Game::Map::TerrainService::instance().initialize(map_def);
    Game::Systems::NavGrid::initialize(map_def.grid.width, map_def.grid.height);
  }

  static auto add_woodcutter(Engine::Core::World& world,
                             float x,
                             float z) -> Engine::Core::Entity* {
    auto* worker = world.create_entity();
    auto* transform = worker->add_component<Engine::Core::TransformComponent>();
    transform->position = {x, 0.0F, z};
    auto* unit = worker->add_component<Engine::Core::UnitComponent>();
    unit->spawn_type = Game::Units::SpawnType::Builder;
    unit->owner_id = 1;
    unit->health = 100;
    unit->max_health = 100;
    unit->speed = 3.0F;
    worker->add_component<Engine::Core::MovementComponent>();

    auto* builder = worker->add_component<Engine::Core::BuilderProductionComponent>();
    builder->build_time = 6.0F;
    builder->has_gather_order = true;
    builder->gather_product_type =
        std::string(Game::Systems::k_builder_product_cut_tree);
    builder->gather_anchor_x = 0.0F;
    builder->gather_anchor_z = 0.0F;
    return worker;
  }

  static void think(Engine::Core::World& world) {
    Game::Systems::GatherLoopSystem system;

    system.update(&world, Game::Systems::GatherLoopSystem::k_think_interval + 0.1F);
  }
};

TEST_F(GatherLoopSystemTest, AFreeWorkerTakesTheNextTreeInTheStandByItself) {
  Engine::Core::World world;
  auto* worker = add_woodcutter(world, 6.0F, 6.0F);

  think(world);

  const auto* builder =
      worker->get_component<Engine::Core::BuilderProductionComponent>();
  ASSERT_NE(builder, nullptr);
  EXPECT_TRUE(builder->has_task_target);
  EXPECT_TRUE(builder->task_target_reserved);
  EXPECT_TRUE(builder->has_construction_site);
  EXPECT_EQ(builder->product_type,
            std::string(Game::Systems::k_builder_product_cut_tree));
  EXPECT_FLOAT_EQ(builder->time_remaining, builder->build_time);
  EXPECT_TRUE(Game::Map::TerrainService::instance().is_world_prop_reserved(
      builder->task_target_id));
}

TEST_F(GatherLoopSystemTest, TwoWorkersNeverClaimTheSameTree) {
  Engine::Core::World world;
  auto* first = add_woodcutter(world, 6.0F, 6.0F);
  auto* second = add_woodcutter(world, -6.0F, 6.0F);

  think(world);
  think(world);

  const auto* first_builder =
      first->get_component<Engine::Core::BuilderProductionComponent>();
  const auto* second_builder =
      second->get_component<Engine::Core::BuilderProductionComponent>();
  ASSERT_NE(first_builder, nullptr);
  ASSERT_NE(second_builder, nullptr);
  EXPECT_TRUE(first_builder->has_task_target);
  EXPECT_TRUE(second_builder->has_task_target);
  EXPECT_NE(first_builder->task_target_id, second_builder->task_target_id);
}

TEST_F(GatherLoopSystemTest, AWorkerStillCarryingALoadIsLeftToWalkItHome) {
  Engine::Core::World world;
  auto* worker = add_woodcutter(world, 6.0F, 6.0F);
  auto* carry = worker->add_component<Engine::Core::ResourceCarryComponent>();
  carry->amounts.add(Game::Systems::ResourceType::Wood, 20);

  think(world);

  const auto* builder =
      worker->get_component<Engine::Core::BuilderProductionComponent>();
  ASSERT_NE(builder, nullptr);
  EXPECT_FALSE(builder->has_task_target);
  EXPECT_TRUE(builder->has_gather_order);
}

TEST_F(GatherLoopSystemTest, AManualMoveOrderEndsTheStandingOrder) {
  Engine::Core::World world;
  auto* worker = add_woodcutter(world, 6.0F, 6.0F);

  Game::Systems::OrderService::prepare_for_move(
      worker, Game::Systems::MoveOrderKind::PlayerMove, false);
  think(world);

  const auto* builder =
      worker->get_component<Engine::Core::BuilderProductionComponent>();
  ASSERT_NE(builder, nullptr);
  EXPECT_FALSE(builder->has_gather_order);
  EXPECT_FALSE(builder->has_task_target);
}

TEST_F(GatherLoopSystemTest, AHaulHomeIsNotMistakenForAManualMove) {
  Engine::Core::World world;
  auto* worker = add_woodcutter(world, 6.0F, 6.0F);

  Game::Systems::OrderService::prepare_for_move(
      worker, Game::Systems::MoveOrderKind::ScriptedMove, false);

  const auto* builder =
      worker->get_component<Engine::Core::BuilderProductionComponent>();
  ASSERT_NE(builder, nullptr);
  EXPECT_TRUE(builder->has_gather_order);
}

TEST_F(GatherLoopSystemTest, AnExhaustedRoundIsRetiredInsteadOfRescannedForever) {
  Engine::Core::World world;
  auto* worker = add_woodcutter(world, 6.0F, 6.0F);
  auto* anchored = worker->get_component<Engine::Core::BuilderProductionComponent>();
  ASSERT_NE(anchored, nullptr);

  anchored->gather_anchor_x = 400.0F;
  anchored->gather_anchor_z = 400.0F;

  think(world);

  const auto* builder =
      worker->get_component<Engine::Core::BuilderProductionComponent>();
  ASSERT_NE(builder, nullptr);
  EXPECT_FALSE(builder->has_gather_order);
  EXPECT_FALSE(builder->has_task_target);
}

TEST_F(GatherLoopSystemTest, TheWorkSpotSitsOnTheWorkerSideOfTheTree) {

  struct Approach {
    const char* name;
    float x;
    float z;
  };
  const Approach approaches[] = {{"north", 0.0F, -8.0F},
                                 {"south", 0.0F, 8.0F},
                                 {"west", -8.0F, 0.0F},
                                 {"east", 8.0F, 0.0F}};

  for (const auto& approach : approaches) {
    lay_out_trees(1);
    const QVector3D tree =
        Game::Map::TerrainService::instance().world_prop_world_position(
            Game::Map::TerrainService::instance().world_props().front());
    float const worker_x = tree.x() + approach.x;
    float const worker_z = tree.z() + approach.z;

    Engine::Core::World world;
    auto* worker = add_woodcutter(world, worker_x, worker_z);

    think(world);

    const auto* builder =
        worker->get_component<Engine::Core::BuilderProductionComponent>();
    ASSERT_NE(builder, nullptr) << approach.name;
    ASSERT_TRUE(builder->has_construction_site) << approach.name;

    float const to_tree = std::hypot(builder->construction_site_x - tree.x(),
                                     builder->construction_site_z - tree.z());
    EXPECT_LT(to_tree, 2.0F)
        << approach.name << ": the work spot must hug the tree, not sit off to a side";

    float const spot_dx = builder->construction_site_x - tree.x();
    float const spot_dz = builder->construction_site_z - tree.z();
    float const facing = (spot_dx * approach.x) + (spot_dz * approach.z);
    EXPECT_GE(facing, 0.0F) << approach.name
                            << ": the crew must never walk round to the far side"
                            << " tree=(" << tree.x() << "," << tree.z() << ")"
                            << " worker=(" << worker_x << "," << worker_z << ")"
                            << " spot=(" << builder->construction_site_x << ","
                            << builder->construction_site_z << ")";
  }
}

TEST_F(GatherLoopSystemTest, AWorkerSentToBuildIsNotPulledOffTheBuildingSite) {
  Engine::Core::World world;
  auto* worker = add_woodcutter(world, 6.0F, 6.0F);
  auto* builder = worker->get_component<Engine::Core::BuilderProductionComponent>();
  ASSERT_NE(builder, nullptr);
  builder->product_type = "home";
  builder->has_construction_site = true;
  builder->construction_site_x = 8.0F;
  builder->construction_site_z = 8.0F;

  think(world);

  EXPECT_EQ(builder->product_type, std::string("home"));
  EXPECT_FLOAT_EQ(builder->construction_site_x, 8.0F);
  EXPECT_FALSE(builder->has_task_target);
}

TEST_F(GatherLoopSystemTest, AWorkerBorrowedForABuildGoesBackToTheTreeLine) {
  Engine::Core::World world;
  auto* worker = add_woodcutter(world, 6.0F, 6.0F);
  auto* builder = worker->get_component<Engine::Core::BuilderProductionComponent>();
  ASSERT_NE(builder, nullptr);
  builder->product_type = "home";
  builder->has_construction_site = true;

  think(world);
  ASSERT_TRUE(builder->has_gather_order);

  builder->product_type.clear();
  builder->has_construction_site = false;
  builder->construction_complete = true;
  think(world);

  EXPECT_TRUE(builder->has_task_target);
  EXPECT_EQ(builder->product_type,
            std::string(Game::Systems::k_builder_product_cut_tree));
}

} // namespace
