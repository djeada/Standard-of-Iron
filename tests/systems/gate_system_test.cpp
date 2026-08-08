#include <algorithm>
#include <gtest/gtest.h>
#include <memory>

#include "core/component.h"
#include "core/entity.h"
#include "core/serialization.h"
#include "core/world.h"
#include "map/terrain_service.h"
#include "systems/building_collision_registry.h"
#include "systems/command_service.h"
#include "systems/gate_service.h"
#include "systems/gate_system.h"
#include "systems/owner_registry.h"
#include "systems/pathfinding.h"
#include "systems/wall_network_service.h"
#include "units/spawn_type.h"
#include "units/troop_config.h"
#include "units/troop_type.h"

using namespace Engine::Core;
using namespace Game::Systems;

namespace {

constexpr int k_gate_owner = 1;
constexpr int k_ally_owner = 3;
constexpr int k_enemy_owner = 2;

class GateSystemTest : public ::testing::Test {
protected:
  void SetUp() override {
    BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    CommandService::initialize(32, 32);

    auto& owners = OwnerRegistry::instance();
    owners.clear();
    owners.register_owner_with_id(k_gate_owner, OwnerType::Player, "Defender");
    owners.register_owner_with_id(k_enemy_owner, OwnerType::AI, "Raider");
    owners.register_owner_with_id(k_ally_owner, OwnerType::AI, "Ally");
    owners.set_owner_team(k_gate_owner, 1);
    owners.set_owner_team(k_enemy_owner, 2);
    owners.set_owner_team(k_ally_owner, 1);
  }

  void TearDown() override {
    GateService::clear_blockers();
    OwnerRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    BuildingCollisionRegistry::instance().clear();
  }

  auto make_wall(World& world, float x, float z, int owner_id) -> Entity* {
    auto* entity = world.create_entity();
    entity->add_component<TransformComponent>(x, 0.0F, z);
    entity->add_component<RenderableComponent>("mesh", "texture");
    auto* unit = entity->add_component<UnitComponent>(800, 800, 0.0F, 0.0F);
    unit->owner_id = owner_id;
    unit->spawn_type = Game::Units::SpawnType::WallSegment;
    entity->add_component<BuildingComponent>();
    auto* wall = entity->add_component<WallSegmentComponent>();
    const auto snapped = WallNetworkService::snap_world_position(x, z);
    wall->grid_x = snapped.x;
    wall->grid_z = snapped.z;
    BuildingCollisionRegistry::instance().register_building(
        entity->get_id(), "wall_segment", x, z, owner_id);
    return entity;
  }

  auto make_gate(World& world, float x, float z, int owner_id) -> Entity* {
    auto* entity = world.create_entity();
    entity->add_component<TransformComponent>(x, 0.0F, z);
    entity->add_component<RenderableComponent>("mesh", "texture");
    auto* unit = entity->add_component<UnitComponent>(700, 700, 0.0F, 0.0F);
    unit->owner_id = owner_id;
    unit->spawn_type = Game::Units::SpawnType::WallGate;
    entity->add_component<BuildingComponent>();
    auto* wall = entity->add_component<WallSegmentComponent>();
    const auto snapped = WallNetworkService::snap_world_position(x, z);
    wall->grid_x = snapped.x;
    wall->grid_z = snapped.z;
    entity->add_component<GateComponent>();
    const auto extent = GateService::structure_extent(0.0F);
    BuildingCollisionRegistry::instance().register_building(
        entity->get_id(),
        "wall_gate",
        x,
        z,
        owner_id,
        {.width = extent.half_x * 2.0F, .depth = extent.half_z * 2.0F});
    return entity;
  }

  static auto make_troop(World& world, float x, float z, int owner_id) -> Entity* {
    auto* entity = world.create_entity();
    entity->add_component<TransformComponent>(x, 0.0F, z);
    auto* unit = entity->add_component<UnitComponent>(100, 100, 3.0F, 10.0F);
    unit->owner_id = owner_id;
    unit->spawn_type = Game::Units::SpawnType::Spearman;
    return entity;
  }

  static void tick(World& world, float seconds, float step = 0.05F) {
    GateSystem system;
    for (float elapsed = 0.0F; elapsed < seconds; elapsed += step) {
      system.update(&world, step);
    }
  }
};

void expect_passage_on_gate(const NavigationPassage& passage,
                            const Entity& gate_entity) {
  const auto* transform = gate_entity.get_component<TransformComponent>();
  ASSERT_NE(transform, nullptr);
  EXPECT_FLOAT_EQ(passage.center_x, transform->position.x);
  EXPECT_FLOAT_EQ(passage.center_z, transform->position.z);
}

} // namespace

TEST_F(GateSystemTest, GateFootprintMatchesWallSegment) {
  const auto gate = BuildingCollisionRegistry::get_building_size("wall_gate");
  const auto wall = BuildingCollisionRegistry::get_building_size("wall_segment");

  EXPECT_FLOAT_EQ(gate.width, wall.width);
  EXPECT_FLOAT_EQ(gate.depth, wall.depth);
}

TEST_F(GateSystemTest, GatehouseIsSolidAcrossThreeCellsAndOpensOnlyInTheMiddle) {
  World world;
  auto* gate = make_gate(world, 0.0F, 0.0F, k_gate_owner);

  const auto* footprint =
      BuildingCollisionRegistry::instance().find_building(gate->get_id());

  ASSERT_NE(footprint, nullptr);

  EXPECT_TRUE(footprint->blocks_navigation);
  EXPECT_FLOAT_EQ(footprint->width, GateComponent::k_structure_half_span * 2.0F);
  EXPECT_FLOAT_EQ(footprint->depth, GateComponent::k_cross_half_extent * 2.0F);

  WallNetworkService::refresh_world(world);
  const auto& passages = BuildingCollisionRegistry::instance().navigation_passages();
  ASSERT_EQ(passages.size(), 1U);
  EXPECT_FLOAT_EQ(passages.front().width, GateComponent::k_passage_half_width * 2.0F);
  EXPECT_LT(passages.front().width, footprint->width);
}

TEST_F(GateSystemTest, GateOpeningIsWideEnoughForTheLargestUnit) {

  const float elephant_width =
      Game::Units::TroopConfig::instance().get_selection_ring_size(
          Game::Units::TroopType::Elephant);
  EXPECT_GT(GateComponent::k_passage_half_width * 2.0F, elephant_width * 1.5F);
}

TEST_F(GateSystemTest, OpensForOwnerTroopInRange) {
  World world;
  make_gate(world, 0.0F, 0.0F, k_gate_owner);
  make_troop(world, 0.0F, -3.0F, k_gate_owner);

  tick(world, 2.0F);

  const auto* gate =
      world.get_entities_with<GateComponent>().front()->get_component<GateComponent>();
  EXPECT_TRUE(gate->is_passable());
  EXPECT_EQ(gate->state, GateComponent::State::Open);
}

TEST_F(GateSystemTest, OpensForAlliedTroopInRange) {
  World world;
  auto* gate_entity = make_gate(world, 0.0F, 0.0F, k_gate_owner);
  make_troop(world, 0.0F, -3.0F, k_ally_owner);

  tick(world, 2.0F);

  EXPECT_TRUE(gate_entity->get_component<GateComponent>()->is_passable());
}

TEST_F(GateSystemTest, StaysShutForEnemyTroopInRange) {
  World world;
  auto* gate_entity = make_gate(world, 0.0F, 0.0F, k_gate_owner);
  make_troop(world, 0.0F, -2.0F, k_enemy_owner);

  tick(world, 3.0F);

  const auto* gate = gate_entity->get_component<GateComponent>();
  EXPECT_FLOAT_EQ(gate->open_amount, 0.0F);
  EXPECT_EQ(gate->state, GateComponent::State::Closed);
  EXPECT_FALSE(gate->is_passable());
}

TEST_F(GateSystemTest, ClosesOnceTheApproachClears) {
  World world;
  auto* gate_entity = make_gate(world, 0.0F, 0.0F, k_gate_owner);
  auto* troop = make_troop(world, 0.0F, -3.0F, k_gate_owner);

  tick(world, 2.0F);
  ASSERT_TRUE(gate_entity->get_component<GateComponent>()->is_passable());

  troop->get_component<TransformComponent>()->position.z = -20.0F;
  tick(world, 4.0F);

  EXPECT_FLOAT_EQ(gate_entity->get_component<GateComponent>()->open_amount, 0.0F);
}

TEST_F(GateSystemTest, DoesNotCloseWhileAServedTroopStandsInThePassage) {
  World world;
  auto* gate_entity = make_gate(world, 0.0F, 0.0F, k_gate_owner);
  auto* troop = make_troop(world, 0.0F, -3.0F, k_gate_owner);

  tick(world, 2.0F);
  troop->get_component<TransformComponent>()->position.z = 0.0F;
  gate_entity->get_component<GateComponent>()->manual_mode =
      GateComponent::ManualMode::ForcedClosed;
  tick(world, 3.0F);

  EXPECT_TRUE(gate_entity->get_component<GateComponent>()->is_passable());
}

TEST_F(GateSystemTest, ManualHoldOpenNeedsNoTroopNearby) {
  World world;
  auto* gate_entity = make_gate(world, 0.0F, 0.0F, k_gate_owner);
  gate_entity->get_component<GateComponent>()->manual_mode =
      GateComponent::ManualMode::ForcedOpen;

  tick(world, 2.0F);

  EXPECT_TRUE(gate_entity->get_component<GateComponent>()->is_passable());
}

TEST_F(GateSystemTest, ManualHoldShutOverridesAFriendlyApproach) {
  World world;
  auto* gate_entity = make_gate(world, 0.0F, 0.0F, k_gate_owner);
  make_troop(world, 0.0F, -3.0F, k_gate_owner);
  gate_entity->get_component<GateComponent>()->manual_mode =
      GateComponent::ManualMode::ForcedClosed;

  tick(world, 3.0F);

  EXPECT_FLOAT_EQ(gate_entity->get_component<GateComponent>()->open_amount, 0.0F);
}

TEST_F(GateSystemTest, ClosedGateBlocksAMoveIntoThePassage) {
  World world;
  make_gate(world, 0.0F, 0.0F, k_gate_owner);

  tick(world, 0.1F);

  EXPECT_TRUE(GateService::blocks_move(QVector3D(0.0F, 0.0F, -2.0F),
                                       QVector3D(0.0F, 0.0F, -0.5F)));
}

TEST_F(GateSystemTest, OpenGateStopsBlockingMoves) {
  World world;
  make_gate(world, 0.0F, 0.0F, k_gate_owner);
  make_troop(world, 0.0F, -3.0F, k_gate_owner);

  tick(world, 2.0F);

  EXPECT_FALSE(GateService::blocks_move(QVector3D(0.0F, 0.0F, -2.0F),
                                        QVector3D(0.0F, 0.0F, -0.5F)));
}

TEST_F(GateSystemTest, ABodyAlreadyInThePassageIsNeverTrapped) {
  World world;
  make_gate(world, 0.0F, 0.0F, k_gate_owner);

  tick(world, 0.1F);

  EXPECT_FALSE(GateService::blocks_move(QVector3D(0.0F, 0.0F, 0.0F),
                                        QVector3D(0.0F, 0.0F, 0.5F)));
}

TEST_F(GateSystemTest, DestroyedGateStopsBeingABarrier) {
  World world;
  auto* gate_entity = make_gate(world, 0.0F, 0.0F, k_gate_owner);

  tick(world, 0.1F);
  ASSERT_FALSE(GateService::blockers().empty());

  gate_entity->get_component<UnitComponent>()->health = 0;
  tick(world, 0.1F);

  EXPECT_TRUE(GateService::blockers().empty());
}

TEST_F(GateSystemTest, LiveGateOpensAPassageInTheNavigationGrid) {
  World world;
  make_wall(world, -2.0F, 0.0F, k_gate_owner);
  make_wall(world, 2.0F, 0.0F, k_gate_owner);
  auto* gate_entity = make_gate(world, 0.0F, 0.0F, k_gate_owner);

  WallNetworkService::refresh_world(world);

  const auto& passages = BuildingCollisionRegistry::instance().navigation_passages();
  ASSERT_EQ(passages.size(), 1U);
  expect_passage_on_gate(passages.front(), *gate_entity);
}

TEST_F(GateSystemTest, DestroyedGateLeavesAPassableBreach) {
  World world;
  make_wall(world, -2.0F, 0.0F, k_gate_owner);
  make_wall(world, 2.0F, 0.0F, k_gate_owner);
  auto* gate_entity = make_gate(world, 0.0F, 0.0F, k_gate_owner);

  WallNetworkService::refresh_world(world);
  ASSERT_EQ(BuildingCollisionRegistry::instance().navigation_passages().size(), 1U);
  const auto* wall = gate_entity->get_component<WallSegmentComponent>();
  const auto cell = Point{wall->grid_x, wall->grid_z};

  BuildingCollisionRegistry::instance().unregister_building(gate_entity->get_id());
  world.destroy_entity(gate_entity->get_id());
  WallNetworkService::refresh_world(world);

  const auto& passages = BuildingCollisionRegistry::instance().navigation_passages();
  ASSERT_EQ(passages.size(), 1U);
  const auto expected = CommandService::grid_to_world(cell);
  EXPECT_FLOAT_EQ(passages.front().center_x, expected.x());
  EXPECT_FLOAT_EQ(passages.front().center_z, expected.z());
}

TEST_F(GateSystemTest, IntactWallRunPublishesNoPassage) {
  World world;
  make_wall(world, -2.0F, 0.0F, k_gate_owner);
  make_wall(world, 0.0F, 0.0F, k_gate_owner);
  make_wall(world, 2.0F, 0.0F, k_gate_owner);

  WallNetworkService::refresh_world(world);

  EXPECT_TRUE(BuildingCollisionRegistry::instance().navigation_passages().empty());
}

TEST_F(GateSystemTest, GateKeepsTheWallLineOrientation) {
  World world;
  make_wall(world, -2.0F, 0.0F, k_gate_owner);
  make_wall(world, 2.0F, 0.0F, k_gate_owner);
  auto* gate_entity = make_gate(world, 0.0F, 0.0F, k_gate_owner);

  WallNetworkService::refresh_world(world);

  const auto* transform = gate_entity->get_component<TransformComponent>();
  const auto* renderable = gate_entity->get_component<RenderableComponent>();
  EXPECT_FLOAT_EQ(transform->rotation.y, 0.0F);
  EXPECT_NE(renderable->renderer_id.find("wall_gate"), std::string::npos);
}

TEST_F(GateSystemTest, GateAcrossAVerticalRunTurnsToFaceIt) {
  World world;
  make_wall(world, 0.0F, -2.0F, k_gate_owner);
  make_wall(world, 0.0F, 2.0F, k_gate_owner);
  auto* gate_entity = make_gate(world, 0.0F, 0.0F, k_gate_owner);

  WallNetworkService::refresh_world(world);

  EXPECT_FLOAT_EQ(gate_entity->get_component<TransformComponent>()->rotation.y, 90.0F);
}

TEST_F(GateSystemTest, ManualModeSurvivesASaveRoundTrip) {
  World world;
  auto* gate_entity = make_gate(world, 0.0F, 0.0F, k_gate_owner);
  auto* gate = gate_entity->get_component<GateComponent>();
  gate->manual_mode = GateComponent::ManualMode::ForcedOpen;
  gate->open_amount = 0.5F;
  gate->state = GateComponent::State::Opening;

  const auto json = Engine::Core::Serialization::serialize_entity(gate_entity);

  World restored;
  auto* clone = restored.create_entity();
  Engine::Core::Serialization::deserialize_entity(clone, json);

  const auto* restored_gate = clone->get_component<GateComponent>();
  ASSERT_NE(restored_gate, nullptr);
  EXPECT_EQ(restored_gate->manual_mode, GateComponent::ManualMode::ForcedOpen);
  EXPECT_FLOAT_EQ(restored_gate->open_amount, 0.5F);
  EXPECT_EQ(restored_gate->state, GateComponent::State::Opening);
}

TEST_F(GateSystemTest, RenderSnapshotCarriesTheLeafPosition) {
  World world;
  auto* gate_entity = make_gate(world, 0.0F, 0.0F, k_gate_owner);
  make_troop(world, 0.0F, -3.0F, k_gate_owner);

  tick(world, 2.0F);
  const float open_amount = gate_entity->get_component<GateComponent>()->open_amount;
  ASSERT_GT(open_amount, GateComponent::k_passable_open_amount);

  world.request_render_snapshots();
  world.update(0.016F);
  const auto snapshot = world.acquire_render_snapshot();
  ASSERT_NE(snapshot, nullptr);

  auto* mirrored = snapshot->get_entity(gate_entity->get_id());
  ASSERT_NE(mirrored, nullptr);
  const auto* mirrored_gate = mirrored->get_component<GateComponent>();
  ASSERT_NE(mirrored_gate, nullptr);
  EXPECT_FLOAT_EQ(mirrored_gate->open_amount, open_amount);
}
