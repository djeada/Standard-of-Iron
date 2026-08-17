#include <QJsonObject>
#include <QVector3D>

#include <gtest/gtest.h>
#include <string>

#include "core/component.h"
#include "core/world.h"
#include "game/command/command.h"
#include "game/command/command_dispatcher.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/systems/builder_product_types.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/gather_loop_system.h"
#include "game/systems/nav_grid.h"
#include "game/systems/order_service.h"
#include "game/systems/owner_registry.h"
#include "game/systems/pathfinding.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/resource_delivery_system.h"
#include "game/systems/resource_types.h"
#include "game/systems/unit_activity.h"
#include "game/units/spawn_type.h"
#include "save/serialization.h"

namespace {

using Game::Map::WorldProp;

class AutoGatherTest : public ::testing::Test {
protected:
  void SetUp() override { reset_shared_state(); }

  void TearDown() override { reset_shared_state(); }

  static void reset_shared_state() {
    Game::Map::TerrainService::instance().clear();
    Game::Map::VisibilityService::instance().reset();
    Game::Systems::OwnerRegistry::instance().clear();
    Game::Systems::PlayerResourceRegistry::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
  }

  struct Node {
    WorldProp::Type type{WorldProp::Type::PineTree};
    float x{0.0F};
    float z{0.0F};
  };

  static constexpr int k_grid_extent = 24;

  static void lay_out(const std::vector<Node>& nodes) {
    lay_out_on(k_grid_extent, nodes);
  }

  static void lay_out_on(int extent, const std::vector<Node>& nodes) {
    const float origin = (static_cast<float>(extent) * 0.5F) - 0.5F;
    Game::Map::MapDefinition map_def;
    map_def.grid.width = extent;
    map_def.grid.height = extent;
    map_def.grid.tile_size = 1.0F;
    map_def.biome.procedural_trees_enabled = false;
    map_def.biome.procedural_boulders_enabled = false;
    map_def.biome.procedural_iron_ore_enabled = false;
    for (const auto& node : nodes) {

      map_def.world_props.push_back(
          {.type = node.type, .x = node.x + origin, .z = node.z + origin});
    }
    Game::Map::TerrainService::instance().initialize(map_def);
    Game::Map::VisibilityService::instance().initialize(
        map_def.grid.width, map_def.grid.height, map_def.grid.tile_size);
    Game::Map::VisibilityService::instance().reveal_all();
    Game::Systems::NavGrid::initialize(map_def.grid.width, map_def.grid.height);
    if (auto* pathfinder = Game::Systems::NavGrid::get_pathfinder()) {
      pathfinder->update_navigation_grid();
    }
  }

  static auto
  add_builder(Engine::Core::World& world, float x, float z) -> Engine::Core::Entity* {
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
    return worker;
  }

  static void order_auto_gather(Engine::Core::World& world,
                                Engine::Core::Entity* worker,
                                const std::string& priority = {}) {
    Game::Command::Command command;
    command.owner_id = 1;
    command.payload = Game::Command::SetAutoGather{
        .units = {worker->get_id()}, .active = true, .priority_product_type = priority};
    Game::Command::dispatch(world, command);
  }

  static void think(Engine::Core::World& world) {
    Game::Systems::GatherLoopSystem system;
    system.update(&world, Game::Systems::GatherLoopSystem::k_think_interval + 0.1F);
  }

  static auto builder_of(Engine::Core::Entity* worker)
      -> Engine::Core::BuilderProductionComponent* {
    return worker->get_component<Engine::Core::BuilderProductionComponent>();
  }

  static void finish_the_load(Engine::Core::Entity* worker) {
    auto* builder = builder_of(worker);
    EXPECT_TRUE(Game::Map::TerrainService::instance().harvest_world_prop(
        builder->task_target_id));
    builder->has_task_target = false;
    builder->task_target_id = 0;
    builder->task_target_reserved = false;
    builder->has_construction_site = false;
    builder->at_construction_site = false;
    builder->in_progress = false;
    builder->product_type.clear();
  }
};

TEST_F(AutoGatherTest, ABuilderOnAutoGatherClaimsTheNearestResourceByItself) {
  lay_out({{.type = WorldProp::Type::PineTree, .x = 9.0F, .z = 0.0F},
           {.type = WorldProp::Type::Boulder, .x = 3.0F, .z = 0.0F}});
  Engine::Core::World world;
  auto* worker = add_builder(world, 0.0F, 0.0F);
  order_auto_gather(world, worker);

  think(world);

  const auto* builder = builder_of(worker);
  ASSERT_NE(builder, nullptr);
  EXPECT_TRUE(builder->auto_gather);
  EXPECT_TRUE(builder->has_task_target);
  EXPECT_TRUE(builder->task_target_reserved);
  EXPECT_TRUE(builder->has_construction_site);

  EXPECT_EQ(builder->product_type,
            std::string(Game::Systems::k_builder_product_collect_stone));
  EXPECT_TRUE(Game::Map::TerrainService::instance().is_world_prop_reserved(
      builder->task_target_id));
}

TEST_F(AutoGatherTest, AnExhaustedNodeSendsTheBuilderOnToTheNextOne) {
  lay_out({{.type = WorldProp::Type::PineTree, .x = 3.0F, .z = 0.0F},
           {.type = WorldProp::Type::PineTree, .x = 7.0F, .z = 0.0F}});
  Engine::Core::World world;
  auto* worker = add_builder(world, 0.0F, 0.0F);
  order_auto_gather(world, worker);

  think(world);
  auto* builder = builder_of(worker);
  ASSERT_TRUE(builder->has_task_target);
  auto const first_target = builder->task_target_id;

  finish_the_load(worker);
  think(world);

  EXPECT_TRUE(builder->auto_gather);
  EXPECT_TRUE(builder->has_task_target);
  EXPECT_NE(builder->task_target_id, first_target);
}

TEST_F(AutoGatherTest, WhenEverythingIsGoneTheOrderStandsAndTheWorkerReportsIt) {
  lay_out({{.type = WorldProp::Type::PineTree, .x = 3.0F, .z = 0.0F}});
  Engine::Core::World world;
  auto* worker = add_builder(world, 0.0F, 0.0F);
  order_auto_gather(world, worker);

  think(world);
  finish_the_load(worker);
  think(world);

  const auto* builder = builder_of(worker);
  EXPECT_TRUE(builder->auto_gather);
  EXPECT_FALSE(builder->has_task_target);
  EXPECT_TRUE(builder->has_active_fault());

  auto const activity = Game::Systems::classify_unit_activity(*worker);
  EXPECT_EQ(activity.kind, Game::Systems::ActivityKind::AutoGather);
  EXPECT_EQ(activity.state, Game::Systems::ActivityState::Unavailable);
}

TEST_F(AutoGatherTest, AResourceWalledOffFromTheWorkerIsPassedOver) {
  lay_out({{.type = WorldProp::Type::PineTree, .x = 3.0F, .z = 0.0F},
           {.type = WorldProp::Type::PineTree, .x = -6.0F, .z = 0.0F}});
  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);

  (void)pathfinder->find_path({0, 0}, {1, 1});

  auto const walled_in = Game::Systems::NavGrid::world_to_grid(3.0F, 0.0F);
  for (int dx = -2; dx <= 2; ++dx) {
    for (int dz = -2; dz <= 2; ++dz) {
      if (std::abs(dx) != 2 && std::abs(dz) != 2) {
        continue;
      }
      pathfinder->set_obstacle(walled_in.x + dx, walled_in.y + dz, true);
    }
  }

  Engine::Core::World world;
  auto* worker = add_builder(world, 0.0F, 0.0F);
  order_auto_gather(world, worker);

  think(world);

  const auto* builder = builder_of(worker);
  ASSERT_TRUE(builder->has_task_target);
  EXPECT_NEAR(builder->task_target_x, -6.0F, 1.0F)
      << "the builder should skip the resource it cannot walk to";
}

TEST_F(AutoGatherTest, TwoBuildersSpreadOverTwoNodesInsteadOfOne) {
  lay_out({{.type = WorldProp::Type::PineTree, .x = 3.0F, .z = 0.0F},
           {.type = WorldProp::Type::PineTree, .x = 3.5F, .z = 1.0F}});
  Engine::Core::World world;
  auto* first = add_builder(world, 0.0F, 0.0F);
  auto* second = add_builder(world, 0.5F, 0.0F);
  order_auto_gather(world, first);
  order_auto_gather(world, second);

  think(world);

  const auto* first_builder = builder_of(first);
  const auto* second_builder = builder_of(second);
  ASSERT_TRUE(first_builder->has_task_target);
  ASSERT_TRUE(second_builder->has_task_target);
  EXPECT_NE(first_builder->task_target_id, second_builder->task_target_id);
}

TEST_F(AutoGatherTest, EquallyDistantNodesDoNotMakeTheWorkerDither) {
  lay_out({{.type = WorldProp::Type::PineTree, .x = -4.0F, .z = 0.0F},
           {.type = WorldProp::Type::PineTree, .x = 3.0F, .z = 0.0F}});
  Engine::Core::World world;
  auto* worker = add_builder(world, 0.0F, 0.0F);
  order_auto_gather(world, worker);

  think(world);
  auto* builder = builder_of(worker);
  ASSERT_TRUE(builder->has_task_target);
  auto const chosen = builder->task_target_id;

  think(world);
  think(world);

  EXPECT_EQ(builder->task_target_id, chosen);
  EXPECT_TRUE(builder->task_target_reserved);
}

TEST_F(AutoGatherTest, APlayerMoveOrderCancelsTheStandingOrder) {
  lay_out({{.type = WorldProp::Type::PineTree, .x = 3.0F, .z = 0.0F}});
  Engine::Core::World world;
  auto* worker = add_builder(world, 0.0F, 0.0F);
  order_auto_gather(world, worker);
  think(world);
  ASSERT_TRUE(builder_of(worker)->auto_gather);

  Game::Systems::OrderService::prepare_for_move(
      worker, Game::Systems::MoveOrderKind::PlayerMove, false);

  EXPECT_FALSE(builder_of(worker)->auto_gather);
}

TEST_F(AutoGatherTest, AStopOrderCancelsTheStandingOrder) {
  lay_out({{.type = WorldProp::Type::PineTree, .x = 3.0F, .z = 0.0F}});
  Engine::Core::World world;
  auto* worker = add_builder(world, 0.0F, 0.0F);
  order_auto_gather(world, worker);

  Game::Systems::OrderService::apply_stop(worker);

  EXPECT_FALSE(builder_of(worker)->auto_gather);
}

TEST_F(AutoGatherTest, TheOrderCanBeToggledOffWithoutTouchingTheRest) {
  lay_out({{.type = WorldProp::Type::PineTree, .x = 3.0F, .z = 0.0F}});
  Engine::Core::World world;
  auto* worker = add_builder(world, 0.0F, 0.0F);
  order_auto_gather(world, worker);
  ASSERT_TRUE(builder_of(worker)->auto_gather);

  Game::Command::Command command;
  command.owner_id = 1;
  command.payload =
      Game::Command::SetAutoGather{.units = {worker->get_id()}, .active = false};
  Game::Command::dispatch(world, command);

  EXPECT_FALSE(builder_of(worker)->auto_gather);
}

TEST_F(AutoGatherTest, APreferredResourceWinsOverACloserOne) {
  lay_out({{.type = WorldProp::Type::PineTree, .x = 2.0F, .z = 0.0F},
           {.type = WorldProp::Type::IronOre, .x = 7.0F, .z = 0.0F}});
  Engine::Core::World world;
  auto* worker = add_builder(world, 0.0F, 0.0F);
  order_auto_gather(
      world, worker, std::string(Game::Systems::k_builder_product_collect_iron_ore));

  think(world);

  const auto* builder = builder_of(worker);
  EXPECT_EQ(builder->product_type,
            std::string(Game::Systems::k_builder_product_collect_iron_ore));
}

TEST_F(AutoGatherTest, APreferredResourceThatRanOutFallsBackToWhatIsLeft) {
  lay_out({{.type = WorldProp::Type::PineTree, .x = 2.0F, .z = 0.0F}});
  Engine::Core::World world;
  auto* worker = add_builder(world, 0.0F, 0.0F);
  order_auto_gather(
      world, worker, std::string(Game::Systems::k_builder_product_collect_iron_ore));

  think(world);

  const auto* builder = builder_of(worker);
  ASSERT_TRUE(builder->has_task_target);
  EXPECT_EQ(builder->product_type,
            std::string(Game::Systems::k_builder_product_cut_tree));
}

TEST_F(AutoGatherTest, ResourcesUnderFogAreLeftAlone) {
  lay_out({{.type = WorldProp::Type::PineTree, .x = 3.0F, .z = 0.0F}});
  Game::Systems::OwnerRegistry::instance().set_local_player_id(1);
  Game::Map::VisibilityService::instance().initialize(
      k_grid_extent, k_grid_extent, 1.0F);

  Engine::Core::World world;
  auto* worker = add_builder(world, 0.0F, 0.0F);
  order_auto_gather(world, worker);

  think(world);

  EXPECT_FALSE(builder_of(worker)->has_task_target);

  Game::Map::VisibilityService::instance().reveal_all();
  think(world);

  EXPECT_TRUE(builder_of(worker)->has_task_target);
}

TEST_F(AutoGatherTest, WithNoDropOffLeftTheWorkerBanksTheLoadAndCarriesOn) {
  lay_out({{.type = WorldProp::Type::PineTree, .x = 3.0F, .z = 0.0F},
           {.type = WorldProp::Type::PineTree, .x = 7.0F, .z = 0.0F}});
  Engine::Core::World world;
  auto* worker = add_builder(world, 0.0F, 0.0F);
  order_auto_gather(world, worker);

  think(world);
  finish_the_load(worker);

  auto* carry = worker->add_component<Engine::Core::ResourceCarryComponent>();
  carry->amounts.add(Game::Systems::ResourceType::Wood, 20);

  int const banked_before = Game::Systems::PlayerResourceRegistry::instance().get(
      1, Game::Systems::ResourceType::Wood);

  Game::Systems::ResourceDeliverySystem delivery;
  delivery.update(&world, 0.1F);

  EXPECT_FALSE(worker->has_component<Engine::Core::ResourceCarryComponent>())
      << "a load with nowhere to go must not pin the worker forever";
  EXPECT_GT(Game::Systems::PlayerResourceRegistry::instance().get(
                1, Game::Systems::ResourceType::Wood),
            banked_before);

  think(world);
  EXPECT_TRUE(builder_of(worker)->has_task_target);
}

TEST_F(AutoGatherTest, TheStandingOrderSurvivesSaveAndLoad) {
  lay_out({{.type = WorldProp::Type::PineTree, .x = 3.0F, .z = 0.0F}});
  Engine::Core::World world;
  auto* worker = add_builder(world, 0.0F, 0.0F);
  order_auto_gather(
      world, worker, std::string(Game::Systems::k_builder_product_collect_stone));

  QJsonObject const saved = Engine::Core::Serialization::serialize_entity(worker);

  Engine::Core::World restored_world;
  auto* restored = restored_world.create_entity();
  Engine::Core::Serialization::deserialize_entity(restored, saved);

  const auto* builder =
      restored->get_component<Engine::Core::BuilderProductionComponent>();
  ASSERT_NE(builder, nullptr);
  EXPECT_TRUE(builder->auto_gather);
  EXPECT_EQ(builder->auto_gather_priority,
            std::string(Game::Systems::k_builder_product_collect_stone));
}

TEST_F(AutoGatherTest, TheSelectedUnitPanelNamesTheStandingOrder) {
  lay_out({{.type = WorldProp::Type::PineTree, .x = 3.0F, .z = 0.0F}});
  Engine::Core::World world;
  auto* worker = add_builder(world, 0.0F, 0.0F);
  order_auto_gather(world, worker);

  auto const waiting = Game::Systems::classify_unit_activity(*worker);
  EXPECT_EQ(waiting.kind, Game::Systems::ActivityKind::AutoGather);
  EXPECT_EQ(waiting.state, Game::Systems::ActivityState::Queued);
  EXPECT_EQ(Game::Systems::activity_kind_id(waiting.kind), "auto_gather");

  think(world);

  auto const working = Game::Systems::classify_unit_activity(*worker);
  EXPECT_EQ(working.kind, Game::Systems::ActivityKind::ChopWood)
      << "once it is on a node the panel should name the work, not the standing order";
}

TEST_F(AutoGatherTest, AHaulHomeIsStillReportedAsADelivery) {
  lay_out({{.type = WorldProp::Type::PineTree, .x = 3.0F, .z = 0.0F}});
  Engine::Core::World world;
  auto* worker = add_builder(world, 0.0F, 0.0F);
  order_auto_gather(world, worker);

  auto* carry = worker->add_component<Engine::Core::ResourceCarryComponent>();
  carry->amounts.add(Game::Systems::ResourceType::Wood, 20);

  auto const activity = Game::Systems::classify_unit_activity(*worker);
  EXPECT_EQ(activity.kind, Game::Systems::ActivityKind::Deliver);
}

TEST_F(AutoGatherTest, TheNearbyPatchIsWorkedBeforeAnythingAcrossTheMap) {
  lay_out_on(160,
             {{.type = WorldProp::Type::PineTree, .x = 6.0F, .z = 0.0F},
              {.type = WorldProp::Type::PineTree, .x = 60.0F, .z = 0.0F}});
  Engine::Core::World world;
  auto* worker = add_builder(world, 0.0F, 0.0F);
  order_auto_gather(world, worker);

  think(world);

  const auto* builder = builder_of(worker);
  ASSERT_TRUE(builder->has_task_target);
  EXPECT_NEAR(builder->task_target_x, 6.0F, 1.5F)
      << "the worker walked past the patch beside it";
}

TEST_F(AutoGatherTest, AStrippedNeighbourhoodSendsTheWorkerFurtherOut) {
  const float beyond = Game::Systems::GatherLoopSystem::k_search_radius + 20.0F;
  lay_out_on(160, {{.type = WorldProp::Type::PineTree, .x = beyond, .z = 0.0F}});
  Engine::Core::World world;
  auto* worker = add_builder(world, 0.0F, 0.0F);
  order_auto_gather(world, worker);

  think(world);

  const auto* builder = builder_of(worker);
  ASSERT_TRUE(builder->has_task_target)
      << "with nothing close by the worker should still go and find the far node";
  EXPECT_NEAR(builder->task_target_x, beyond, 1.5F);
}

TEST_F(AutoGatherTest, ACarriedLoadIsWalkedHomeBeforeTheNextNodeIsTaken) {
  lay_out({{.type = WorldProp::Type::PineTree, .x = 3.0F, .z = 0.0F}});
  Engine::Core::World world;
  auto* worker = add_builder(world, 0.0F, 0.0F);
  order_auto_gather(world, worker);

  auto* carry = worker->add_component<Engine::Core::ResourceCarryComponent>();
  carry->amounts.add(Game::Systems::ResourceType::Wood, 20);

  think(world);

  const auto* builder = builder_of(worker);
  EXPECT_FALSE(builder->has_task_target);
  EXPECT_TRUE(builder->auto_gather);
}

} // namespace
