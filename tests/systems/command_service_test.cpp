#include <QVector3D>

#include <algorithm>
#include <gtest/gtest.h>
#include <limits>
#include <vector>

#include "core/component.h"
#include "core/system.h"
#include "core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/command_service.h"
#include "game/systems/local_avoidance_system.h"
#include "game/systems/movement_system.h"
#include "game/systems/pathfinding.h"
#include "game/units/troop_config.h"
#include "render/entity/registry.h"
#include "render/gl/humanoid/animation/animation_inputs.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "tests/support/movement_test_access.h"

namespace {

class DirectTransformDisplacementSystem : public Engine::Core::System {
public:
  explicit DirectTransformDisplacementSystem(Engine::Core::EntityID entity_id)
      : m_entity_id(entity_id) {}

  void update(Engine::Core::World* world, float) override {
    auto* entity = (world != nullptr) ? world->get_entity(m_entity_id) : nullptr;
    auto* transform = (entity != nullptr)
                          ? entity->get_component<Engine::Core::TransformComponent>()
                          : nullptr;
    if (transform != nullptr) {
      transform->position.x += 0.5F;
    }
  }

private:
  Engine::Core::EntityID m_entity_id;
};

class CommandServiceTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::CommandService::initialize(32, 32);
  }

  void TearDown() override {
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
  }

  static auto create_unit(Engine::Core::World& world,
                          float x,
                          float z,
                          Game::Units::SpawnType spawn_type) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    if (entity == nullptr) {
      return nullptr;
    }
    auto* transform = entity->add_component<Engine::Core::TransformComponent>();
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    auto* movement = entity->add_component<Engine::Core::MovementComponent>();
    if (transform == nullptr || unit == nullptr || movement == nullptr) {
      return nullptr;
    }

    transform->position = {x, 0.0F, z};
    unit->spawn_type = spawn_type;
    unit->speed = 3.0F;
    return entity;
  }

  static void wait_for_path_results(
      Engine::Core::World& world,
      const std::vector<Engine::Core::MovementComponent*>& movements) {
    (void)world;
    (void)movements;
  }

  static void run_movement_for(Engine::Core::World& world,
                               const std::vector<Engine::Core::Entity*>& units,
                               int steps = 180,
                               float delta_time = 0.1F) {
    Game::Systems::MovementSystem movement_system;
    for (int i = 0; i < steps; ++i) {
      movement_system.update(&world, delta_time);
      for (auto* unit : units) {
        ASSERT_NE(unit, nullptr);
        auto* transform = unit->get_component<Engine::Core::TransformComponent>();
        ASSERT_NE(transform, nullptr);
        auto const current_grid = Game::Systems::CommandService::world_to_grid(
            transform->position.x, transform->position.z);
        EXPECT_TRUE(Game::Systems::CommandService::is_grid_walkable(current_grid))
            << "unit=" << unit->get_id() << " world=(" << transform->position.x << ", "
            << transform->position.z << ") grid=(" << current_grid.x << ", "
            << current_grid.y << ")";
      }
    }
  }
};

TEST_F(CommandServiceTest, UnitRadiusUsesSelectionRingFootprint) {
  Engine::Core::World world;
  auto* unit = create_unit(world, 0.0F, 0.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(unit, nullptr);

  float const expected_radius =
      std::max(Game::Units::TroopConfig::instance().get_selection_ring_size(
                   Game::Units::SpawnType::Archer) *
                   0.5F,
               0.5F);

  EXPECT_FLOAT_EQ(Game::Systems::CommandService::get_unit_radius(world, unit->get_id()),
                  expected_radius);
}

TEST_F(CommandServiceTest, MotionSnapshotCapturesDirectTransformDisplacement) {
  Engine::Core::World world;
  auto* unit = create_unit(world, 0.0F, 0.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(unit, nullptr);

  world.add_system(std::make_unique<DirectTransformDisplacementSystem>(unit->get_id()));
  world.update(0.1F);

  auto* motion = unit->get_component<Engine::Core::MotionPresentationComponent>();
  ASSERT_NE(motion, nullptr);
  EXPECT_TRUE(motion->snapshot_valid);
  EXPECT_TRUE(motion->has_locomotion());
  EXPECT_EQ(motion->source, Engine::Core::MotionPresentationSource::ForcedDisplacement);
  EXPECT_GT(motion->speed, 0.0F);
}

TEST_F(CommandServiceTest, InfantryMovementPublishesWalkAnimationStateImmediately) {
  Engine::Core::World world;
  auto* unit = create_unit(world, 0.0F, 0.0F, Game::Units::SpawnType::Spearman);
  ASSERT_NE(unit, nullptr);

  auto* movement = unit->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(movement, nullptr);
  MovementTestAccess::set_has_target(*movement, true);
  MovementTestAccess::set_target_x(*movement, 5.0F);
  MovementTestAccess::set_target_y(*movement, 0.0F);
  MovementTestAccess::set_goal_x(*movement, 5.0F);
  MovementTestAccess::set_goal_y(*movement, 0.0F);

  world.add_system(std::make_unique<Game::Systems::MovementSystem>());
  world.update(0.1F);

  auto* motion = unit->get_component<Engine::Core::MotionPresentationComponent>();
  ASSERT_NE(motion, nullptr);
  EXPECT_TRUE(motion->has_locomotion());
  EXPECT_TRUE(motion->is_walk_state());

  Render::GL::DrawContext ctx{};
  ctx.entity = unit;
  ctx.world = &world;
  ctx.animation_time = 0.1F;
  auto const anim = Render::GL::sample_anim_state(ctx);

  EXPECT_TRUE(Render::Creature::is_moving_animation(anim.movement_state));
  EXPECT_FALSE(Render::Creature::is_running_animation(anim.movement_state));
}

TEST_F(CommandServiceTest, ArrivalStopsNavigationWalkAnimationImmediately) {
  Engine::Core::World world;
  auto* unit = create_unit(world, 0.0F, 0.0F, Game::Units::SpawnType::Spearman);
  ASSERT_NE(unit, nullptr);

  auto* movement = unit->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(movement, nullptr);
  MovementTestAccess::set_has_target(*movement, true);
  MovementTestAccess::set_target_x(*movement, 0.25F);
  MovementTestAccess::set_target_y(*movement, 0.0F);
  MovementTestAccess::set_goal_x(*movement, 5.0F);
  MovementTestAccess::set_goal_y(*movement, 0.0F);
  MovementTestAccess::set_vx(*movement, 1.0F);

  auto* motion = unit->add_component<Engine::Core::MotionPresentationComponent>();
  ASSERT_NE(motion, nullptr);
  motion->set_state(Engine::Core::MotionPresentationState::Walk);
  motion->source = Engine::Core::MotionPresentationSource::Navigation;
  motion->has_navigation_intent = true;
  motion->has_movement_target = true;
  motion->speed = 1.0F;

  world.add_system(std::make_unique<Game::Systems::MovementSystem>());
  world.update(0.1F);

  EXPECT_FALSE(movement->get_has_target());
  EXPECT_FALSE(movement->has_waypoints());
  EXPECT_FALSE(motion->has_locomotion());
  EXPECT_TRUE(motion->is_idle_state());
  EXPECT_EQ(motion->source, Engine::Core::MotionPresentationSource::None);

  Render::GL::DrawContext ctx{};
  ctx.entity = unit;
  ctx.world = &world;
  ctx.animation_time = 0.1F;
  auto const anim = Render::GL::sample_anim_state(ctx);

  EXPECT_FALSE(Render::Creature::is_moving_animation(anim.movement_state));
}

TEST_F(CommandServiceTest, AttackRangeDoesNotCancelNavigationMotionPresentation) {
  Engine::Core::World world;
  auto* unit = create_unit(world, 0.0F, 0.0F, Game::Units::SpawnType::Spearman);
  auto* target = create_unit(world, 0.75F, 0.0F, Game::Units::SpawnType::Knight);
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(target, nullptr);

  auto* movement = unit->get_component<Engine::Core::MovementComponent>();
  auto* attack = unit->add_component<Engine::Core::AttackComponent>();
  auto* attack_target = unit->add_component<Engine::Core::AttackTargetComponent>();
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(attack, nullptr);
  ASSERT_NE(attack_target, nullptr);

  MovementTestAccess::set_has_target(*movement, true);
  MovementTestAccess::set_target_x(*movement, 5.0F);
  MovementTestAccess::set_target_y(*movement, 0.0F);
  MovementTestAccess::set_goal_x(*movement, 5.0F);
  MovementTestAccess::set_goal_y(*movement, 0.0F);

  attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
  attack->melee_range = 1.5F;
  attack_target->target_id = target->get_id();
  attack_target->should_chase = true;

  world.update(0.1F);

  auto* motion = unit->get_component<Engine::Core::MotionPresentationComponent>();
  ASSERT_NE(motion, nullptr);
  EXPECT_TRUE(motion->attack_target_in_range);
  EXPECT_TRUE(motion->has_locomotion());
  EXPECT_TRUE(motion->is_walk_state());
}

TEST_F(CommandServiceTest, MoveOptionsControlFormationModePersistence) {
  Engine::Core::World world;
  auto* unit = create_unit(world, 0.0F, 0.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(unit, nullptr);

  auto* formation = unit->add_component<Engine::Core::FormationModeComponent>();
  ASSERT_NE(formation, nullptr);
  formation->active = true;

  Game::Systems::CommandService::MoveOptions preserve_options;
  preserve_options.preserve_formation_mode = true;
  Game::Systems::CommandService::move_unit(
      world, unit->get_id(), QVector3D(4.0F, 0.0F, 4.0F), preserve_options);
  EXPECT_TRUE(formation->active);

  Game::Systems::CommandService::move_unit(
      world, unit->get_id(), QVector3D(6.0F, 0.0F, 6.0F));
  EXPECT_FALSE(formation->active);
}

TEST_F(CommandServiceTest, ShortMoveNearObstacleKeepsWalkableOrder) {
  Engine::Core::World world;
  auto* entity = create_unit(world, -4.0F, 0.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(entity, nullptr);
  auto* movement = entity->get_component<Engine::Core::MovementComponent>();
  auto* transform = entity->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(transform, nullptr);

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();

  Game::Systems::Point const obstacle =
      Game::Systems::CommandService::world_to_grid(-1.0F, 1.0F);
  pathfinder->set_obstacle(obstacle.x, obstacle.y, true);

  Game::Systems::CommandService::move_unit(
      world, entity->get_id(), QVector3D(2.0F, 0.0F, 0.0F));

  EXPECT_TRUE(movement->get_has_target());
  auto const goal_grid = Game::Systems::CommandService::world_to_grid(
      movement->get_goal_x(), movement->get_goal_y());
  EXPECT_TRUE(Game::Systems::CommandService::is_grid_walkable(goal_grid));

  run_movement_for(world, {entity}, 60);
  EXPECT_GT(transform->position.x, 1.0F);
}

TEST_F(CommandServiceTest, MoveToBlockedDestinationStoresWalkableGoal) {
  Engine::Core::World world;
  auto* entity = create_unit(world, -4.0F, 0.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(entity, nullptr);
  auto* movement = entity->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(movement, nullptr);

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();

  QVector3D const requested_target(2.0F, 0.0F, 0.0F);
  Game::Systems::Point const blocked_goal =
      Game::Systems::CommandService::world_to_grid(requested_target.x(),
                                                   requested_target.z());
  pathfinder->set_obstacle(blocked_goal.x, blocked_goal.y, true);

  Game::Systems::CommandService::move_unit(world, entity->get_id(), requested_target);

  auto const stored_goal = Game::Systems::CommandService::world_to_grid(
      movement->get_goal_x(), movement->get_goal_y());
  EXPECT_TRUE(Game::Systems::CommandService::is_grid_walkable(stored_goal));
  EXPECT_FALSE(stored_goal == blocked_goal);
  EXPECT_TRUE(movement->get_has_target());
}

TEST_F(CommandServiceTest, IdleMovementDoesNotResurrectStaleGoal) {
  Engine::Core::World world;
  auto* entity = create_unit(world, 0.0F, 0.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(entity, nullptr);

  auto* movement = entity->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(movement, nullptr);
  MovementTestAccess::set_goal_x(*movement, 12.0F);
  MovementTestAccess::set_goal_y(*movement, 0.0F);

  world.add_system(std::make_unique<Game::Systems::MovementSystem>());
  world.update(0.1F);

  EXPECT_FALSE(movement->get_has_target());
}

TEST_F(CommandServiceTest, InvalidTileRecoveryAssignsSafeTargetImmediately) {
  Engine::Core::World world;
  auto* entity = create_unit(world, 0.0F, 0.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(entity, nullptr);

  auto* movement = entity->get_component<Engine::Core::MovementComponent>();
  auto* transform = entity->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(transform, nullptr);

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();
  auto const blocked = Game::Systems::CommandService::world_to_grid(
      transform->position.x, transform->position.z);
  pathfinder->set_obstacle(blocked.x, blocked.y, true);

  Game::Systems::MovementSystem movement_system;
  movement_system.update(&world, 0.1F);

  EXPECT_TRUE(movement->get_has_target());
  auto const recovered = Game::Systems::CommandService::world_to_grid(
      movement->get_goal_x(), movement->get_goal_y());
  EXPECT_TRUE(pathfinder->is_walkable(recovered.x, recovered.y));
  EXPECT_FALSE(recovered == blocked);
}

TEST_F(CommandServiceTest, InvalidTileRecoveryKeepsActiveOrder) {
  Engine::Core::World world;
  auto* entity = create_unit(world, 0.0F, 0.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(entity, nullptr);

  auto* movement = entity->get_component<Engine::Core::MovementComponent>();
  auto* transform = entity->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(transform, nullptr);
  MovementTestAccess::set_has_target(*movement, true);
  MovementTestAccess::set_target_x(*movement, 6.0F);
  MovementTestAccess::set_target_y(*movement, 0.0F);
  MovementTestAccess::set_goal_x(*movement, 6.0F);
  MovementTestAccess::set_goal_y(*movement, 0.0F);

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();
  auto const blocked = Game::Systems::CommandService::world_to_grid(
      transform->position.x, transform->position.z);
  pathfinder->set_obstacle(blocked.x, blocked.y, true);

  Game::Systems::MovementSystem movement_system;
  movement_system.update(&world, 0.1F);

  EXPECT_TRUE(movement->get_has_target());
  auto const recovered_goal = Game::Systems::CommandService::world_to_grid(
      movement->get_goal_x(), movement->get_goal_y());
  EXPECT_TRUE(pathfinder->is_walkable(recovered_goal.x, recovered_goal.y));
}

TEST_F(CommandServiceTest, RepeatedInvalidTileRecoveryKeepsOrder) {
  Engine::Core::World world;
  auto* entity = create_unit(world, 0.0F, 0.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(entity, nullptr);

  auto* movement = entity->get_component<Engine::Core::MovementComponent>();
  auto* transform = entity->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(transform, nullptr);
  MovementTestAccess::set_has_target(*movement, true);
  MovementTestAccess::set_target_x(*movement, 6.0F);
  MovementTestAccess::set_target_y(*movement, 0.0F);
  MovementTestAccess::set_goal_x(*movement, 6.0F);
  MovementTestAccess::set_goal_y(*movement, 0.0F);

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();
  auto const blocked = Game::Systems::CommandService::world_to_grid(
      transform->position.x, transform->position.z);
  pathfinder->set_obstacle(blocked.x, blocked.y, true);

  Game::Systems::MovementSystem movement_system;

  for (int i = 0; i < 10; ++i) {
    transform->position.x = 0.0F;
    transform->position.z = 0.0F;
    movement_system.update(&world, 0.1F);
  }

  EXPECT_TRUE(movement->get_has_target());
}

TEST_F(CommandServiceTest, NewMoveOrderAssignsFreshTarget) {
  Engine::Core::World world;
  auto* entity = create_unit(world, 0.0F, 0.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(entity, nullptr);

  auto* movement = entity->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(movement, nullptr);
  MovementTestAccess::set_goal_x(*movement, 1.0F);
  MovementTestAccess::set_goal_y(*movement, 0.0F);

  Game::Systems::CommandService::move_unit(
      world, entity->get_id(), QVector3D(4.0F, 0.0F, 0.0F));

  EXPECT_TRUE(movement->get_has_target());
}

TEST_F(CommandServiceTest, BlockedSegmentKeepsDirectOrderForRecovery) {
  Engine::Core::World world;
  auto* entity = create_unit(world, -4.0F, 0.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(entity, nullptr);

  auto* movement = entity->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(movement, nullptr);

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();

  Game::Systems::Point const obstacle =
      Game::Systems::CommandService::world_to_grid(0.0F, 0.0F);
  pathfinder->set_obstacle(obstacle.x, obstacle.y, true);

  MovementTestAccess::set_has_target(*movement, true);
  MovementTestAccess::set_target_x(*movement, 4.0F);
  MovementTestAccess::set_target_y(*movement, 0.0F);
  MovementTestAccess::set_goal_x(*movement, 4.0F);
  MovementTestAccess::set_goal_y(*movement, 0.0F);
  Game::Systems::MovementSystem movement_system;
  movement_system.update(&world, 0.1F);

  EXPECT_TRUE(movement->get_has_target());
  EXPECT_TRUE(movement->get_path().empty());
}

TEST_F(CommandServiceTest, BlockedSegmentMoveKeepsOrderAndRoutesAroundObstacle) {
  Engine::Core::World world;
  auto* entity = create_unit(world, -4.0F, 0.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(entity, nullptr);

  auto* movement = entity->get_component<Engine::Core::MovementComponent>();
  auto* transform = entity->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(transform, nullptr);

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();

  Game::Systems::Point const obstacle =
      Game::Systems::CommandService::world_to_grid(0.0F, 0.0F);
  pathfinder->set_obstacle(obstacle.x, obstacle.y, true);

  Game::Systems::CommandService::move_unit(
      world, entity->get_id(), QVector3D(4.0F, 0.0F, 0.0F));

  EXPECT_TRUE(movement->get_has_target());
  auto const goal_grid = Game::Systems::CommandService::world_to_grid(
      movement->get_goal_x(), movement->get_goal_y());
  EXPECT_TRUE(Game::Systems::CommandService::is_grid_walkable(goal_grid));

  run_movement_for(world, {entity}, 90);
  EXPECT_GT(transform->position.x, 2.0F);
}

TEST_F(CommandServiceTest, FpvCommanderMoveIgnoresStaleRtsMeleeLockState) {
  Engine::Core::World world;
  auto* commander =
      create_unit(world, 0.0F, 0.0F, Game::Units::SpawnType::RomanFieldCommander);
  ASSERT_NE(commander, nullptr);
  auto* attack = commander->add_component<Engine::Core::AttackComponent>();
  auto* commander_data = commander->add_component<Engine::Core::CommanderComponent>();
  auto* rpg = commander->add_component<Engine::Core::RpgHealthComponent>();
  auto* movement = commander->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(attack, nullptr);
  ASSERT_NE(commander_data, nullptr);
  ASSERT_NE(rpg, nullptr);
  ASSERT_NE(movement, nullptr);

  commander_data->fpv_controlled = true;
  rpg->active = true;
  attack->in_melee_lock = true;
  attack->melee_lock_target_id = 77;

  Game::Systems::CommandService::move_unit(
      world, commander->get_id(), QVector3D(0.4F, 0.0F, 0.4F));

  EXPECT_TRUE(movement->get_has_target());
  EXPECT_FLOAT_EQ(movement->get_target_x(), 0.4F);
  EXPECT_FLOAT_EQ(movement->get_target_y(), 0.4F);
}

TEST_F(CommandServiceTest, MultiUnitMoveRoutesEveryMemberThroughSingleCellGap) {
  Engine::Core::World world;

  auto* left = create_unit(world, -10.0F, -2.0F, Game::Units::SpawnType::Archer);
  auto* center = create_unit(world, -10.0F, 0.0F, Game::Units::SpawnType::Archer);
  auto* right = create_unit(world, -10.0F, 2.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(left, nullptr);
  ASSERT_NE(center, nullptr);
  ASSERT_NE(right, nullptr);

  auto* left_movement = left->get_component<Engine::Core::MovementComponent>();
  auto* center_movement = center->get_component<Engine::Core::MovementComponent>();
  auto* right_movement = right->get_component<Engine::Core::MovementComponent>();
  auto* left_transform = left->get_component<Engine::Core::TransformComponent>();
  auto* center_transform = center->get_component<Engine::Core::TransformComponent>();
  auto* right_transform = right->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(left_movement, nullptr);
  ASSERT_NE(center_movement, nullptr);
  ASSERT_NE(right_movement, nullptr);
  ASSERT_NE(left_transform, nullptr);
  ASSERT_NE(center_transform, nullptr);
  ASSERT_NE(right_transform, nullptr);

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();

  for (int world_z = -15; world_z <= 15; ++world_z) {
    if (world_z == 0) {
      continue;
    }
    Game::Systems::Point const wall_cell =
        Game::Systems::CommandService::world_to_grid(0.0F, static_cast<float>(world_z));
    pathfinder->set_obstacle(wall_cell.x, wall_cell.y, true);
  }

  Game::Systems::CommandService::MoveOptions const options;

  Game::Systems::CommandService::move_units(
      world,
      {left->get_id(), center->get_id(), right->get_id()},
      {QVector3D(10.0F, 0.0F, -2.0F),
       QVector3D(10.0F, 0.0F, 0.0F),
       QVector3D(10.0F, 0.0F, 2.0F)},
      options);

  wait_for_path_results(world, {left_movement, center_movement, right_movement});

  EXPECT_TRUE(left_movement->get_has_target());
  EXPECT_TRUE(center_movement->get_has_target());
  EXPECT_TRUE(right_movement->get_has_target());

  run_movement_for(world, {left, center, right});
  EXPECT_GT(left_transform->position.x, 8.0F);
  EXPECT_GT(center_transform->position.x, 8.0F);
  EXPECT_GT(right_transform->position.x, 8.0F);
}

TEST_F(CommandServiceTest, MultiUnitMoveFailureNearBoundaryDoesNotTeleportUnits) {
  Game::Systems::CommandService::initialize(16, 16);
  Engine::Core::World world;

  auto* left = create_unit(world, -7.0F, -2.0F, Game::Units::SpawnType::Archer);
  auto* center = create_unit(world, -7.0F, 0.0F, Game::Units::SpawnType::Archer);
  auto* right = create_unit(world, -7.0F, 2.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(left, nullptr);
  ASSERT_NE(center, nullptr);
  ASSERT_NE(right, nullptr);

  auto* left_transform = left->get_component<Engine::Core::TransformComponent>();
  auto* center_transform = center->get_component<Engine::Core::TransformComponent>();
  auto* right_transform = right->get_component<Engine::Core::TransformComponent>();
  auto* left_movement = left->get_component<Engine::Core::MovementComponent>();
  auto* center_movement = center->get_component<Engine::Core::MovementComponent>();
  auto* right_movement = right->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(left_transform, nullptr);
  ASSERT_NE(center_transform, nullptr);
  ASSERT_NE(right_transform, nullptr);
  ASSERT_NE(left_movement, nullptr);
  ASSERT_NE(center_movement, nullptr);
  ASSERT_NE(right_movement, nullptr);

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();

  for (int world_z = -7; world_z <= 7; ++world_z) {
    Game::Systems::Point const wall_cell =
        Game::Systems::CommandService::world_to_grid(0.0F, static_cast<float>(world_z));
    pathfinder->set_obstacle(wall_cell.x, wall_cell.y, true);
  }

  Game::Systems::CommandService::MoveOptions const options;

  Game::Systems::CommandService::move_units(
      world,
      {left->get_id(), center->get_id(), right->get_id()},
      {QVector3D(7.0F, 0.0F, -2.0F),
       QVector3D(7.0F, 0.0F, 0.0F),
       QVector3D(7.0F, 0.0F, 2.0F)},
      options);

  wait_for_path_results(world, {left_movement, center_movement, right_movement});

  EXPECT_FLOAT_EQ(left_transform->position.x, -7.0F);
  EXPECT_FLOAT_EQ(left_transform->position.z, -2.0F);
  EXPECT_FLOAT_EQ(center_transform->position.x, -7.0F);
  EXPECT_FLOAT_EQ(center_transform->position.z, 0.0F);
  EXPECT_FLOAT_EQ(right_transform->position.x, -7.0F);
  EXPECT_FLOAT_EQ(right_transform->position.z, 2.0F);
}

TEST_F(CommandServiceTest, MultiUnitMoveCanRouteMembersIndividuallyThroughGap) {
  Engine::Core::World world;

  auto* left = create_unit(world, -10.0F, -2.0F, Game::Units::SpawnType::Archer);
  auto* center = create_unit(world, -10.0F, 0.0F, Game::Units::SpawnType::Archer);
  auto* right = create_unit(world, -10.0F, 2.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(left, nullptr);
  ASSERT_NE(center, nullptr);
  ASSERT_NE(right, nullptr);

  auto* left_movement = left->get_component<Engine::Core::MovementComponent>();
  auto* center_movement = center->get_component<Engine::Core::MovementComponent>();
  auto* right_movement = right->get_component<Engine::Core::MovementComponent>();
  auto* left_transform = left->get_component<Engine::Core::TransformComponent>();
  auto* center_transform = center->get_component<Engine::Core::TransformComponent>();
  auto* right_transform = right->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(left_movement, nullptr);
  ASSERT_NE(center_movement, nullptr);
  ASSERT_NE(right_movement, nullptr);
  ASSERT_NE(left_transform, nullptr);
  ASSERT_NE(center_transform, nullptr);
  ASSERT_NE(right_transform, nullptr);

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();

  for (int world_z = -15; world_z <= 15; ++world_z) {
    if (world_z >= -2 && world_z <= 2) {
      continue;
    }
    Game::Systems::Point const wall_cell =
        Game::Systems::CommandService::world_to_grid(0.0F, static_cast<float>(world_z));
    pathfinder->set_obstacle(wall_cell.x, wall_cell.y, true);
  }

  Game::Systems::CommandService::MoveOptions const options;

  Game::Systems::CommandService::move_units(
      world,
      {left->get_id(), center->get_id(), right->get_id()},
      {QVector3D(10.0F, 0.0F, -2.0F),
       QVector3D(10.0F, 0.0F, 0.0F),
       QVector3D(10.0F, 0.0F, 2.0F)},
      options);

  wait_for_path_results(world, {left_movement, center_movement, right_movement});

  EXPECT_TRUE(left_movement->get_has_target());
  EXPECT_TRUE(center_movement->get_has_target());
  EXPECT_TRUE(right_movement->get_has_target());

  run_movement_for(world, {left, center, right});
  EXPECT_GT(left_transform->position.x, 8.0F);
  EXPECT_GT(center_transform->position.x, 8.0F);
  EXPECT_GT(right_transform->position.x, 8.0F);
}

TEST_F(CommandServiceTest,
       MultiUnitMoveKeepsOrdersWhenTargetsOnlyHaveDiagonalObstacles) {
  Engine::Core::World world;

  auto* left = create_unit(world, -10.0F, -2.0F, Game::Units::SpawnType::Archer);
  auto* center = create_unit(world, -10.0F, 0.0F, Game::Units::SpawnType::Archer);
  auto* right = create_unit(world, -10.0F, 2.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(left, nullptr);
  ASSERT_NE(center, nullptr);
  ASSERT_NE(right, nullptr);

  auto* left_movement = left->get_component<Engine::Core::MovementComponent>();
  auto* center_movement = center->get_component<Engine::Core::MovementComponent>();
  auto* right_movement = right->get_component<Engine::Core::MovementComponent>();
  auto* left_transform = left->get_component<Engine::Core::TransformComponent>();
  auto* center_transform = center->get_component<Engine::Core::TransformComponent>();
  auto* right_transform = right->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(left_movement, nullptr);
  ASSERT_NE(center_movement, nullptr);
  ASSERT_NE(right_movement, nullptr);
  ASSERT_NE(left_transform, nullptr);
  ASSERT_NE(center_transform, nullptr);
  ASSERT_NE(right_transform, nullptr);

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();

  Game::Systems::Point const diagonal_block =
      Game::Systems::CommandService::world_to_grid(11.0F, 1.0F);
  pathfinder->set_obstacle(diagonal_block.x, diagonal_block.y, true);

  Game::Systems::CommandService::MoveOptions const options;

  Game::Systems::CommandService::move_units(
      world,
      {left->get_id(), center->get_id(), right->get_id()},
      {QVector3D(10.0F, 0.0F, -2.0F),
       QVector3D(10.0F, 0.0F, 0.0F),
       QVector3D(10.0F, 0.0F, 2.0F)},
      options);

  EXPECT_TRUE(left_movement->get_has_target());
  EXPECT_TRUE(center_movement->get_has_target());
  EXPECT_TRUE(right_movement->get_has_target());
}

TEST_F(CommandServiceTest, PlannedMoveExpandsSharedDestinationIntoFormationSlots) {
  Engine::Core::World world;

  auto* left = create_unit(world, -10.0F, -2.0F, Game::Units::SpawnType::Archer);
  auto* center = create_unit(world, -10.0F, 0.0F, Game::Units::SpawnType::Archer);
  auto* right = create_unit(world, -10.0F, 2.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(left, nullptr);
  ASSERT_NE(center, nullptr);
  ASSERT_NE(right, nullptr);

  auto* left_movement = left->get_component<Engine::Core::MovementComponent>();
  auto* center_movement = center->get_component<Engine::Core::MovementComponent>();
  auto* right_movement = right->get_component<Engine::Core::MovementComponent>();
  auto* left_transform = left->get_component<Engine::Core::TransformComponent>();
  auto* center_transform = center->get_component<Engine::Core::TransformComponent>();
  auto* right_transform = right->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(left_movement, nullptr);
  ASSERT_NE(center_movement, nullptr);
  ASSERT_NE(right_movement, nullptr);
  ASSERT_NE(left_transform, nullptr);
  ASSERT_NE(center_transform, nullptr);
  ASSERT_NE(right_transform, nullptr);

  QVector3D const shared_target(10.0F, 0.0F, 0.0F);
  std::vector<Engine::Core::EntityID> const selected_units = {
      left->get_id(), center->get_id(), right->get_id()};
  auto const plan = Game::Systems::CommandService::plan_ground_move(
      world, selected_units, shared_target);
  Game::Systems::CommandService::issue_ground_move(world, selected_units, plan);

  auto const distinct_goal_count = [=]() {
    std::vector<std::pair<float, float>> goals = {
        {left_movement->get_goal_x(), left_movement->get_goal_y()},
        {center_movement->get_goal_x(), center_movement->get_goal_y()},
        {right_movement->get_goal_x(), right_movement->get_goal_y()}};
    std::sort(goals.begin(), goals.end());
    goals.erase(std::unique(goals.begin(), goals.end()), goals.end());
    return goals.size();
  }();

  EXPECT_EQ(distinct_goal_count, 3U);
}

TEST_F(CommandServiceTest, PlannedMoveUsesLargeUnitFootprintForSharedDestination) {
  Engine::Core::World world;

  std::vector<Engine::Core::Entity*> units = {
      create_unit(world, -10.0F, -2.0F, Game::Units::SpawnType::Elephant),
      create_unit(world, -10.0F, 0.0F, Game::Units::SpawnType::Spearman),
      create_unit(world, -10.0F, 2.0F, Game::Units::SpawnType::Elephant),
      create_unit(world, -12.0F, -1.0F, Game::Units::SpawnType::Spearman)};
  for (auto* unit : units) {
    ASSERT_NE(unit, nullptr);
  }

  QVector3D const shared_target(10.0F, 0.0F, 0.0F);
  std::vector<Engine::Core::EntityID> const selected_units = {
      units[0]->get_id(), units[1]->get_id(), units[2]->get_id(), units[3]->get_id()};
  auto const plan = Game::Systems::CommandService::plan_ground_move(
      world, selected_units, shared_target);
  Game::Systems::CommandService::issue_ground_move(world, selected_units, plan);

  float min_distance_sq = std::numeric_limits<float>::max();
  for (std::size_t i = 0; i < units.size(); ++i) {
    auto* lhs = units[i]->get_component<Engine::Core::MovementComponent>();
    ASSERT_NE(lhs, nullptr);
    for (std::size_t j = i + 1; j < units.size(); ++j) {
      auto* rhs = units[j]->get_component<Engine::Core::MovementComponent>();
      ASSERT_NE(rhs, nullptr);
      float const dx = lhs->get_goal_x() - rhs->get_goal_x();
      float const dz = lhs->get_goal_y() - rhs->get_goal_y();
      min_distance_sq = std::min(min_distance_sq, dx * dx + dz * dz);
    }
  }

  EXPECT_GT(min_distance_sq, 3.0F);
}

TEST_F(CommandServiceTest, AttackTargetAssignsDistinctApproachGoals) {
  Engine::Core::World world;

  auto* target = create_unit(world, 10.0F, 0.0F, Game::Units::SpawnType::Knight);
  ASSERT_NE(target, nullptr);
  auto* target_unit = target->get_component<Engine::Core::UnitComponent>();
  ASSERT_NE(target_unit, nullptr);
  target_unit->owner_id = 2;

  std::vector<Engine::Core::Entity*> attackers = {
      create_unit(world, 0.0F, -1.0F, Game::Units::SpawnType::Spearman),
      create_unit(world, 0.0F, 0.0F, Game::Units::SpawnType::Spearman),
      create_unit(world, 0.0F, 1.0F, Game::Units::SpawnType::Spearman)};
  for (auto* attacker : attackers) {
    ASSERT_NE(attacker, nullptr);
    auto* unit = attacker->get_component<Engine::Core::UnitComponent>();
    ASSERT_NE(unit, nullptr);
    unit->owner_id = 1;
    auto* attack = attacker->add_component<Engine::Core::AttackComponent>();
    ASSERT_NE(attack, nullptr);
    attack->can_melee = true;
    attack->can_ranged = false;
    attack->preferred_mode = Engine::Core::AttackComponent::CombatMode::Melee;
  }

  Game::Systems::CommandService::attack_target(
      world,
      {attackers[0]->get_id(), attackers[1]->get_id(), attackers[2]->get_id()},
      target->get_id(),
      true);

  std::vector<std::pair<float, float>> goals;
  for (auto* attacker : attackers) {
    auto* movement = attacker->get_component<Engine::Core::MovementComponent>();
    auto* attack_target =
        attacker->get_component<Engine::Core::AttackTargetComponent>();
    ASSERT_NE(movement, nullptr);
    ASSERT_NE(attack_target, nullptr);
    EXPECT_TRUE(movement->get_has_target());
    EXPECT_EQ(attack_target->target_id, target->get_id());
    EXPECT_TRUE(attack_target->should_chase);
    goals.emplace_back(movement->get_goal_x(), movement->get_goal_y());
  }

  std::sort(goals.begin(), goals.end());
  goals.erase(std::unique(goals.begin(), goals.end()), goals.end());
  EXPECT_EQ(goals.size(), attackers.size());
}

TEST_F(CommandServiceTest, AttackTargetBreaksStaleMeleeLockOnRetarget) {
  Engine::Core::World world;

  auto* old_target = create_unit(world, 1.0F, 0.0F, Game::Units::SpawnType::Knight);
  auto* new_target = create_unit(world, 12.0F, 0.0F, Game::Units::SpawnType::Knight);
  auto* attacker = create_unit(world, 0.0F, 0.0F, Game::Units::SpawnType::Spearman);
  ASSERT_NE(old_target, nullptr);
  ASSERT_NE(new_target, nullptr);
  ASSERT_NE(attacker, nullptr);

  old_target->get_component<Engine::Core::UnitComponent>()->owner_id = 2;
  new_target->get_component<Engine::Core::UnitComponent>()->owner_id = 2;
  attacker->get_component<Engine::Core::UnitComponent>()->owner_id = 1;

  auto* attack = attacker->add_component<Engine::Core::AttackComponent>();
  ASSERT_NE(attack, nullptr);
  attack->can_melee = true;
  attack->can_ranged = false;
  attack->preferred_mode = Engine::Core::AttackComponent::CombatMode::Melee;
  attack->in_melee_lock = true;
  attack->melee_lock_target_id = old_target->get_id();

  Game::Systems::CommandService::attack_target(
      world, {attacker->get_id()}, new_target->get_id(), true);

  EXPECT_FALSE(attack->in_melee_lock);
  EXPECT_EQ(attack->melee_lock_target_id, 0U);

  auto* attack_target = attacker->get_component<Engine::Core::AttackTargetComponent>();
  ASSERT_NE(attack_target, nullptr);
  EXPECT_EQ(attack_target->target_id, new_target->get_id());
  EXPECT_TRUE(attack_target->is_player_command);
}

TEST_F(CommandServiceTest, InvalidPositionAssignsNearbyWalkableRecoveryMove) {
  Game::Systems::CommandService::initialize(16, 16);
  Engine::Core::World world;

  auto* unit = create_unit(world, -7.0F, 0.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(unit, nullptr);

  auto* movement = unit->get_component<Engine::Core::MovementComponent>();
  auto* transform = unit->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(transform, nullptr);
  MovementTestAccess::set_goal_x(*movement, 5.0F);
  MovementTestAccess::set_goal_y(*movement, 0.0F);

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();
  auto const current_grid = Game::Systems::CommandService::world_to_grid(
      transform->position.x, transform->position.z);
  pathfinder->set_obstacle(current_grid.x, current_grid.y, true);

  Game::Systems::MovementSystem movement_system;
  movement_system.update(&world, 0.3F);

  EXPECT_TRUE(movement->get_has_target());
  EXPECT_GT(movement->get_target_x(), transform->position.x);
  auto const target_grid = Game::Systems::CommandService::world_to_grid(
      movement->get_target_x(), movement->get_target_y());
  auto const goal_grid = Game::Systems::CommandService::world_to_grid(
      movement->get_goal_x(), movement->get_goal_y());
  EXPECT_TRUE(Game::Systems::CommandService::is_grid_walkable(target_grid));
  EXPECT_TRUE(Game::Systems::CommandService::is_grid_walkable(goal_grid));
}

TEST_F(CommandServiceTest, PersistentInvalidPositionRetargetsToNearbyRecoveryCell) {
  Game::Systems::CommandService::initialize(16, 16);
  Engine::Core::World world;

  auto* unit = create_unit(world, 0.0F, 0.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(unit, nullptr);

  auto* movement = unit->get_component<Engine::Core::MovementComponent>();
  auto* transform = unit->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(transform, nullptr);
  MovementTestAccess::set_has_target(*movement, true);
  MovementTestAccess::set_target_x(*movement, 6.0F);
  MovementTestAccess::set_target_y(*movement, 0.0F);
  MovementTestAccess::set_goal_x(*movement, 6.0F);
  MovementTestAccess::set_goal_y(*movement, 0.0F);

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();

  for (int world_x = 0; world_x <= 3; ++world_x) {
    Game::Systems::Point const blocked =
        Game::Systems::CommandService::world_to_grid(static_cast<float>(world_x), 0.0F);
    pathfinder->set_obstacle(blocked.x, blocked.y, true);
  }

  world.add_system(std::make_unique<Game::Systems::MovementSystem>());

  for (int i = 0; i < 8; ++i) {
    world.update(0.1F);
  }

  Game::Systems::Point const current_grid =
      Game::Systems::CommandService::world_to_grid(transform->position.x,
                                                   transform->position.z);
  EXPECT_TRUE(pathfinder->is_walkable(current_grid.x, current_grid.y));
  auto const goal_grid = Game::Systems::CommandService::world_to_grid(
      movement->get_goal_x(), movement->get_goal_y());
  EXPECT_TRUE(pathfinder->is_walkable(goal_grid.x, goal_grid.y));
}

TEST_F(CommandServiceTest, LocalRecoveryDoesNotSpliceBlockedGoalBackIntoPath) {
  Game::Systems::CommandService::initialize(16, 16);
  Engine::Core::World world;

  auto* unit = create_unit(world, 0.0F, 0.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(unit, nullptr);

  auto* movement = unit->get_component<Engine::Core::MovementComponent>();
  auto* transform = unit->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(transform, nullptr);

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();

  Game::Systems::Point const current_grid =
      Game::Systems::CommandService::world_to_grid(transform->position.x,
                                                   transform->position.z);
  QVector3D const requested_goal(6.0F, 0.0F, 0.0F);
  Game::Systems::Point const blocked_goal =
      Game::Systems::CommandService::world_to_grid(requested_goal.x(),
                                                   requested_goal.z());
  pathfinder->set_obstacle(current_grid.x, current_grid.y, true);
  pathfinder->set_obstacle(blocked_goal.x, blocked_goal.y, true);
  MovementTestAccess::set_goal_x(*movement, requested_goal.x());
  MovementTestAccess::set_goal_y(*movement, requested_goal.z());

  Game::Systems::MovementSystem movement_system;
  movement_system.update(&world, 0.3F);

  auto const stored_goal = Game::Systems::CommandService::world_to_grid(
      movement->get_goal_x(), movement->get_goal_y());
  EXPECT_TRUE(pathfinder->is_walkable(stored_goal.x, stored_goal.y));
  EXPECT_FALSE(stored_goal == blocked_goal);
  for (auto const& [x, z] : movement->get_path()) {
    auto const waypoint_grid = Game::Systems::CommandService::world_to_grid(x, z);
    EXPECT_TRUE(pathfinder->is_walkable(waypoint_grid.x, waypoint_grid.y));
  }
}

TEST_F(CommandServiceTest, MultiUnitBridgeCrossingUsesWalkableBridgeCells) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 21;
  map_def.grid.height = 21;
  map_def.grid.tile_size = 1.0F;
  map_def.rivers.push_back(
      {QVector3D(0.0F, 0.0F, -10.0F), QVector3D(0.0F, 0.0F, 10.0F), 2.0F});
  map_def.bridges.push_back(
      {QVector3D(-2.0F, 0.0F, 0.0F), QVector3D(2.0F, 0.0F, 0.0F), 2.0F, 0.6F});
  Game::Map::TerrainService::instance().initialize(map_def);
  Game::Systems::CommandService::initialize(map_def.grid.width, map_def.grid.height);

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->mark_navigation_grid_dirty();
  pathfinder->update_navigation_grid();

  Engine::Core::World world;
  auto* left = create_unit(world, -8.0F, -2.0F, Game::Units::SpawnType::Archer);
  auto* center = create_unit(world, -8.0F, 0.0F, Game::Units::SpawnType::Archer);
  auto* right = create_unit(world, -8.0F, 2.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(left, nullptr);
  ASSERT_NE(center, nullptr);
  ASSERT_NE(right, nullptr);

  auto* left_movement = left->get_component<Engine::Core::MovementComponent>();
  auto* center_movement = center->get_component<Engine::Core::MovementComponent>();
  auto* right_movement = right->get_component<Engine::Core::MovementComponent>();
  auto* left_transform = left->get_component<Engine::Core::TransformComponent>();
  auto* center_transform = center->get_component<Engine::Core::TransformComponent>();
  auto* right_transform = right->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(left_movement, nullptr);
  ASSERT_NE(center_movement, nullptr);
  ASSERT_NE(right_movement, nullptr);
  ASSERT_NE(left_transform, nullptr);
  ASSERT_NE(center_transform, nullptr);
  ASSERT_NE(right_transform, nullptr);

  Game::Systems::CommandService::MoveOptions const options;

  Game::Systems::CommandService::move_units(
      world,
      {left->get_id(), center->get_id(), right->get_id()},
      {QVector3D(8.0F, 0.0F, -2.0F),
       QVector3D(8.0F, 0.0F, 0.0F),
       QVector3D(8.0F, 0.0F, 2.0F)},
      options);

  wait_for_path_results(world, {left_movement, center_movement, right_movement});

  std::vector<Engine::Core::Entity*> const units = {left, center, right};
  std::vector<bool> crossed_bridge(units.size(), false);
  Game::Systems::MovementSystem movement_system;
  auto const* height_map = Game::Map::TerrainService::instance().get_height_map();
  ASSERT_NE(height_map, nullptr);

  for (int step = 0; step < 220; ++step) {
    movement_system.update(&world, 0.1F);
    for (std::size_t idx = 0; idx < units.size(); ++idx) {
      auto* transform = units[idx]->get_component<Engine::Core::TransformComponent>();
      ASSERT_NE(transform, nullptr);
      auto const current_grid = Game::Systems::CommandService::world_to_grid(
          transform->position.x, transform->position.z);
      EXPECT_TRUE(Game::Systems::CommandService::is_grid_walkable(current_grid))
          << "unit=" << units[idx]->get_id() << " world=(" << transform->position.x
          << ", " << transform->position.z << ") grid=(" << current_grid.x << ", "
          << current_grid.y << ")";
      crossed_bridge[idx] = crossed_bridge[idx] ||
                            height_map->isBridgeCell(current_grid.x, current_grid.y);
    }
  }

  EXPECT_GT(left_transform->position.x, 6.0F);
  EXPECT_GT(center_transform->position.x, 6.0F);
  EXPECT_GT(right_transform->position.x, 6.0F);
  EXPECT_TRUE(crossed_bridge[0]);
  EXPECT_TRUE(crossed_bridge[1]);
  EXPECT_TRUE(crossed_bridge[2]);
}

TEST_F(CommandServiceTest, MultiUnitMoveKeepsAllUnitsOrderedWhenOnePlannedSlotIsBad) {
  Engine::Core::World world;
  auto* first = create_unit(world, -4.0F, -1.0F, Game::Units::SpawnType::Archer);
  auto* second = create_unit(world, -4.0F, 0.0F, Game::Units::SpawnType::Archer);
  auto* third = create_unit(world, -4.0F, 1.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  ASSERT_NE(third, nullptr);

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();

  Game::Systems::Point const blocked =
      Game::Systems::CommandService::world_to_grid(4.0F, 0.0F);
  for (int dz = -1; dz <= 1; ++dz) {
    pathfinder->set_obstacle(blocked.x, blocked.y + dz, true);
  }

  Game::Systems::CommandService::MoveOptions const options;

  Game::Systems::CommandService::move_units(
      world,
      {first->get_id(), second->get_id(), third->get_id()},
      {QVector3D(4.0F, 0.0F, -1.0F),
       QVector3D(4.0F, 0.0F, 0.0F),
       QVector3D(4.0F, 0.0F, 1.0F)},
      options);

  auto* first_movement = first->get_component<Engine::Core::MovementComponent>();
  auto* second_movement = second->get_component<Engine::Core::MovementComponent>();
  auto* third_movement = third->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(first_movement, nullptr);
  ASSERT_NE(second_movement, nullptr);
  ASSERT_NE(third_movement, nullptr);

  EXPECT_TRUE(first_movement->get_has_target());
  EXPECT_TRUE(second_movement->get_has_target());
  EXPECT_TRUE(third_movement->get_has_target());
}

TEST_F(CommandServiceTest, RuntimeLocalAvoidanceKeepsBridgeEntrantsOnWalkableCells) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 21;
  map_def.grid.height = 21;
  map_def.grid.tile_size = 1.0F;
  map_def.rivers.push_back(
      {QVector3D(0.0F, 0.0F, -10.0F), QVector3D(0.0F, 0.0F, 10.0F), 2.0F});
  map_def.bridges.push_back(
      {QVector3D(-2.0F, 0.0F, 0.0F), QVector3D(2.0F, 0.0F, 0.0F), 2.0F, 0.6F});
  Game::Map::TerrainService::instance().initialize(map_def);
  Game::Systems::CommandService::initialize(map_def.grid.width, map_def.grid.height);

  Engine::Core::World world;
  std::vector<Engine::Core::Entity*> units;
  for (int i = 0; i < 4; ++i) {
    auto* unit = create_unit(
        world, -3.0F, static_cast<float>(i) * 0.15F, Game::Units::SpawnType::Archer);
    ASSERT_NE(unit, nullptr);
    auto* movement = unit->get_component<Engine::Core::MovementComponent>();
    ASSERT_NE(movement, nullptr);
    MovementTestAccess::set_has_target(*movement, true);
    MovementTestAccess::set_target_x(*movement, 1.5F);
    MovementTestAccess::set_target_y(*movement, 0.0F);
    MovementTestAccess::set_goal_x(*movement, 4.0F);
    MovementTestAccess::set_goal_y(*movement, 0.0F);
    units.push_back(unit);
  }

  Game::Systems::MovementSystem movement_system;
  Game::Systems::LocalAvoidanceSystem avoidance_system;
  for (int frame = 0; frame < 20; ++frame) {
    movement_system.update(&world, 0.1F);
    avoidance_system.update(&world, 0.1F);
  }

  for (auto* unit : units) {
    auto* transform = unit->get_component<Engine::Core::TransformComponent>();
    auto* movement = unit->get_component<Engine::Core::MovementComponent>();
    ASSERT_NE(transform, nullptr);
    ASSERT_NE(movement, nullptr);
    auto const grid = Game::Systems::CommandService::world_to_grid(
        transform->position.x, transform->position.z);
    EXPECT_TRUE(Game::Systems::CommandService::is_grid_walkable(grid));
  }
}

TEST_F(CommandServiceTest, UnitApproachingBridgeMovesOnWalkableCells) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 21;
  map_def.grid.height = 21;
  map_def.grid.tile_size = 1.0F;
  map_def.rivers.push_back(
      {QVector3D(0.0F, 0.0F, -10.0F), QVector3D(0.0F, 0.0F, 10.0F), 2.0F});
  map_def.bridges.push_back(
      {QVector3D(-2.0F, 0.0F, 0.0F), QVector3D(2.0F, 0.0F, 0.0F), 2.0F, 0.6F});
  Game::Map::TerrainService::instance().initialize(map_def);
  Game::Systems::CommandService::initialize(map_def.grid.width, map_def.grid.height);

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->mark_navigation_grid_dirty();
  pathfinder->update_navigation_grid();

  Engine::Core::World world;
  auto* unit = create_unit(world, -4.0F, 1.25F, Game::Units::SpawnType::Archer);
  ASSERT_NE(unit, nullptr);

  auto* transform = unit->get_component<Engine::Core::TransformComponent>();
  auto* movement = unit->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(transform, nullptr);
  ASSERT_NE(movement, nullptr);

  MovementTestAccess::set_target_x(*movement, -1.5F);
  MovementTestAccess::set_target_y(*movement, 0.0F);
  MovementTestAccess::set_goal_x(*movement, 6.0F);
  MovementTestAccess::set_goal_y(*movement, 0.0F);
  MovementTestAccess::set_has_target(*movement, true);

  Game::Systems::MovementSystem movement_system;
  movement_system.update(&world, 0.1F);

  EXPECT_LT(transform->position.x, -3.0F);
  EXPECT_GT(transform->position.x, -4.1F);
  auto const grid = Game::Systems::CommandService::world_to_grid(transform->position.x,
                                                                 transform->position.z);
  EXPECT_TRUE(Game::Systems::CommandService::is_grid_walkable(grid));
}

} // namespace
