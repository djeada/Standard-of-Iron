#include <QVector3D>

#include <cmath>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <vector>

#include "core/component_gameplay.h"
#include "core/world.h"
#include "game/formation/army_formation_registry.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/movement/command_service.h"
#include "game/systems/movement/movement_system.h"
#include "game/systems/navigation/nav_grid.h"

namespace {

using Engine::Core::MovementComponent;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Game::Systems::CommandService;
using Game::Systems::MoveOrderKind;

class MovementOrdersTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Formation::ArmyFormationRegistry::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::NavGrid::initialize(48, 48);
  }

  void TearDown() override {
    Game::Formation::ArmyFormationRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
  }

  static auto spawn(Engine::Core::World& world,
                    float x,
                    float z,
                    float speed = 3.0F) -> Engine::Core::EntityID {
    auto* entity = world.create_entity();
    auto* transform = entity->add_component<TransformComponent>();
    auto* unit = entity->add_component<UnitComponent>();
    entity->add_component<MovementComponent>();
    transform->position = {x, 0.0F, z};
    unit->spawn_type = Game::Units::SpawnType::Archer;
    unit->speed = speed;
    unit->health = 100;
    unit->max_health = 100;
    return entity->get_id();
  }

  static auto movement_of(Engine::Core::World& world,
                          Engine::Core::EntityID id) -> MovementComponent& {
    return *world.try_get<MovementComponent>(id);
  }

  static auto goal_distance(const MovementComponent& movement,
                            const QVector3D& target) -> float {
    return std::hypot(movement.get_goal_x() - target.x(),
                      movement.get_goal_y() - target.z());
  }
};

TEST_F(MovementOrdersTest, ASingleMoveGivesTheUnitATargetAndANewOrder) {
  Engine::Core::World world;
  auto const id = spawn(world, 0.0F, 0.0F);
  auto const& movement = movement_of(world, id);
  ASSERT_FALSE(movement.get_has_target());

  CommandService::move_unit(world, id, {8.0F, 0.0F, 4.0F});

  EXPECT_TRUE(movement.get_has_target());
  EXPECT_GT(movement.get_order_sequence(), 0U);
  EXPECT_LT(goal_distance(movement, {8.0F, 0.0F, 4.0F}), 1.0F);
  EXPECT_TRUE(movement.get_has_requested_goal());
}

TEST_F(MovementOrdersTest, ASecondMoveSupersedesTheFirst) {
  Engine::Core::World world;
  auto const id = spawn(world, 0.0F, 0.0F);
  auto const& movement = movement_of(world, id);

  CommandService::move_unit(world, id, {8.0F, 0.0F, 0.0F});
  auto const first_order = movement.get_order_sequence();
  CommandService::move_unit(world, id, {0.0F, 0.0F, 9.0F});

  EXPECT_GT(movement.get_order_sequence(), first_order);
  EXPECT_LT(goal_distance(movement, {0.0F, 0.0F, 9.0F}), 1.0F);
}

TEST_F(MovementOrdersTest, ChaseOrdersArriveExactlyAndRetargetWithTheirIssuer) {
  Engine::Core::World world;
  auto const chase = spawn(world, 0.0F, 0.0F);
  auto const walk = spawn(world, 2.0F, 0.0F);

  CommandService::MoveOptions chase_options;
  chase_options.kind = MoveOrderKind::AttackChase;
  CommandService::move_unit(world, chase, {10.0F, 0.0F, 0.0F}, chase_options);
  CommandService::move_unit(world, walk, {10.0F, 0.0F, 2.0F});

  EXPECT_TRUE(movement_of(world, chase).get_precise_arrival());
  EXPECT_TRUE(movement_of(world, chase).get_issuer_retargets());
  EXPECT_FALSE(movement_of(world, walk).get_precise_arrival());
  EXPECT_FALSE(movement_of(world, walk).get_issuer_retargets());
}

TEST_F(MovementOrdersTest, AGroupMoveSendsEveryMemberToItsOwnTarget) {
  Engine::Core::World world;
  std::vector<Engine::Core::EntityID> units;
  std::vector<QVector3D> targets;
  for (int index = 0; index < 4; ++index) {
    units.push_back(spawn(world, static_cast<float>(index) * 1.5F, 0.0F));
    targets.emplace_back(12.0F + static_cast<float>(index) * 1.5F, 0.0F, 6.0F);
  }

  CommandService::move_units(world, units, targets);

  for (std::size_t index = 0; index < units.size(); ++index) {
    auto const& movement = movement_of(world, units[index]);
    EXPECT_TRUE(movement.get_has_target()) << "member " << index;
    EXPECT_LT(goal_distance(movement, targets[index]), 1.0F) << "member " << index;
  }
}

TEST_F(MovementOrdersTest, AGroupMoveMarchesAtTheSlowestMembersPace) {
  Engine::Core::World world;
  auto const slow = spawn(world, 0.0F, 0.0F, 1.2F);
  auto const fast = spawn(world, 2.0F, 0.0F, 3.4F);

  CommandService::move_units(
      world,
      {slow, fast},
      {QVector3D(10.0F, 0.0F, 0.0F), QVector3D(12.0F, 0.0F, 0.0F)});

  EXPECT_FLOAT_EQ(movement_of(world, slow).get_declared_group_pace(), 1.2F);
  EXPECT_FLOAT_EQ(movement_of(world, fast).get_declared_group_pace(), 1.2F);
}

TEST_F(MovementOrdersTest, AMismatchedGroupOrderIsIgnored) {
  Engine::Core::World world;
  auto const a = spawn(world, 0.0F, 0.0F);
  auto const b = spawn(world, 2.0F, 0.0F);

  CommandService::move_units(world, {a, b}, {QVector3D(10.0F, 0.0F, 0.0F)});

  EXPECT_FALSE(movement_of(world, a).get_has_target());
  EXPECT_FALSE(movement_of(world, b).get_has_target());
  EXPECT_EQ(movement_of(world, a).get_order_sequence(), 0U);
}

TEST_F(MovementOrdersTest, AnEmptyGroupOrderDoesNothing) {
  Engine::Core::World world;
  auto const a = spawn(world, 0.0F, 0.0F);

  CommandService::move_units(world, std::vector<Engine::Core::EntityID>{}, {});

  EXPECT_FALSE(movement_of(world, a).get_has_target());
}

TEST_F(MovementOrdersTest, IntentsCarryTheirFacingToTheUnit) {
  Engine::Core::World world;
  auto const a = spawn(world, 0.0F, 0.0F);
  auto const b = spawn(world, 2.0F, 0.0F);

  std::vector<CommandService::MoveIntent> intents;
  intents.push_back({a, QVector3D(10.0F, 0.0F, 5.0F), 90.0F});
  intents.push_back({b, QVector3D(12.0F, 0.0F, 5.0F), std::nullopt});
  CommandService::move_units(world, intents);

  auto const* facing = world.try_get<TransformComponent>(a);
  ASSERT_NE(facing, nullptr);
  EXPECT_TRUE(facing->has_desired_yaw);
  EXPECT_FLOAT_EQ(facing->desired_yaw, 90.0F);
  EXPECT_FALSE(world.try_get<TransformComponent>(b)->has_desired_yaw);
  EXPECT_TRUE(movement_of(world, a).get_has_target());
  EXPECT_TRUE(movement_of(world, b).get_has_target());
}

TEST_F(MovementOrdersTest, FormationSlotFollowersMarkTheirMovement) {
  Engine::Core::World world;
  auto const id = spawn(world, 0.0F, 0.0F);

  CommandService::MoveOptions options;
  options.follow_formation_slots = true;
  std::vector<CommandService::MoveIntent> intents;
  intents.push_back({id, QVector3D(6.0F, 0.0F, 3.0F), 45.0F});
  CommandService::move_units(world, intents, options);

  auto const& movement = movement_of(world, id);
  EXPECT_TRUE(movement.get_following_formation_slot());
  EXPECT_TRUE(movement.get_issuer_retargets());
  EXPECT_TRUE(movement.get_precise_arrival());
  EXPECT_TRUE(movement.get_has_target());
  EXPECT_LT(goal_distance(movement, {6.0F, 0.0F, 3.0F}), 0.5F);
  EXPECT_TRUE(world.try_get<TransformComponent>(id)->has_desired_yaw);
}

TEST_F(MovementOrdersTest, ARegisteredMovementSystemDoesNotChangeTheRouting) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::MovementSystem>());
  std::vector<Engine::Core::EntityID> units;
  std::vector<QVector3D> targets;
  for (int index = 0; index < 3; ++index) {
    units.push_back(spawn(world, static_cast<float>(index) * 1.5F, 0.0F));
    targets.emplace_back(14.0F, 0.0F, static_cast<float>(index) * 1.5F);
  }

  CommandService::move_units(world, units, targets);

  for (std::size_t index = 0; index < units.size(); ++index) {
    EXPECT_TRUE(movement_of(world, units[index]).get_has_target());
    EXPECT_LT(goal_distance(movement_of(world, units[index]), targets[index]), 1.0F);
  }
}

} // namespace
