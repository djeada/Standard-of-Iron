#include <QVector3D>

#include <chrono>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

#include "core/component.h"
#include "core/system.h"
#include "core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/command_service.h"
#include "game/systems/movement_system.h"
#include "game/systems/pathfinding.h"
#include "game/units/troop_config.h"
#include "render/entity/registry.h"
#include "render/gl/humanoid/animation/animation_inputs.h"
#include "render/gl/humanoid/humanoid_types.h"

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
    for (int attempt = 0; attempt < 80; ++attempt) {
      Game::Systems::CommandService::process_path_results(world);
      bool all_done = true;
      for (auto* movement : movements) {
        if (movement != nullptr && movement->path_pending) {
          all_done = false;
          break;
        }
      }
      if (all_done) {
        return;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    FAIL() << "Timed out waiting for pathfinding results";
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
  movement->has_target = true;
  movement->target_x = 5.0F;
  movement->target_y = 0.0F;
  movement->goal_x = 5.0F;
  movement->goal_y = 0.0F;

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

  movement->has_target = true;
  movement->target_x = 5.0F;
  movement->target_y = 0.0F;
  movement->goal_x = 5.0F;
  movement->goal_y = 0.0F;

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

TEST_F(CommandServiceTest,
       ShortMoveNearObstacleUsesPathRequestInsteadOfUnsafeDirectLine) {
  Engine::Core::World world;
  auto* entity = create_unit(world, -4.0F, 0.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(entity, nullptr);
  auto* movement = entity->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(movement, nullptr);

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_building_obstacles();

  Game::Systems::Point const obstacle =
      Game::Systems::CommandService::world_to_grid(-1.0F, 1.0F);
  pathfinder->set_obstacle(obstacle.x, obstacle.y, true);

  Game::Systems::CommandService::move_unit(
      world, entity->get_id(), QVector3D(2.0F, 0.0F, 0.0F));

  EXPECT_TRUE(movement->path_pending);
  EXPECT_NE(movement->pending_request_id, 0U);
}

TEST_F(CommandServiceTest, MovementSystemQueuesRecoveryMoveForNextTick) {
  Engine::Core::World world;
  auto* entity = create_unit(world, 0.0F, 0.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(entity, nullptr);

  auto* movement = entity->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(movement, nullptr);
  movement->goal_x = 12.0F;
  movement->goal_y = 0.0F;

  world.add_system(std::make_unique<Game::Systems::MovementSystem>());
  world.update(0.1F);

  auto* repath = entity->get_component<Engine::Core::RepathRequestComponent>();
  ASSERT_NE(repath, nullptr);
  EXPECT_FLOAT_EQ(repath->goal_x, 12.0F);
  EXPECT_FLOAT_EQ(repath->goal_y, 0.0F);
  EXPECT_FALSE(movement->path_pending);
  EXPECT_FALSE(movement->has_target);

  world.update(0.1F);

  EXPECT_EQ(entity->get_component<Engine::Core::RepathRequestComponent>(), nullptr);
  EXPECT_TRUE(movement->path_pending || movement->has_target);
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

  EXPECT_TRUE(movement->has_target);
  EXPECT_FLOAT_EQ(movement->target_x, 0.4F);
  EXPECT_FLOAT_EQ(movement->target_y, 0.4F);
}

TEST_F(CommandServiceTest, GroupMoveRejectsLeaderPathWhenFormationCannotFitThroughGap) {
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
  ASSERT_NE(left_movement, nullptr);
  ASSERT_NE(center_movement, nullptr);
  ASSERT_NE(right_movement, nullptr);

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_building_obstacles();

  for (int world_z = -15; world_z <= 15; ++world_z) {
    if (world_z == 0) {
      continue;
    }
    Game::Systems::Point const wall_cell =
        Game::Systems::CommandService::world_to_grid(0.0F, static_cast<float>(world_z));
    pathfinder->set_obstacle(wall_cell.x, wall_cell.y, true);
  }

  Game::Systems::CommandService::MoveOptions options;
  options.allow_direct_fallback = false;
  options.group_move = true;

  Game::Systems::CommandService::move_units(
      world,
      {left->get_id(), center->get_id(), right->get_id()},
      {QVector3D(10.0F, 0.0F, -2.0F),
       QVector3D(10.0F, 0.0F, 0.0F),
       QVector3D(10.0F, 0.0F, 2.0F)},
      options);

  wait_for_path_results(world, {left_movement, center_movement, right_movement});

  EXPECT_FALSE(left_movement->has_target);
  EXPECT_FALSE(center_movement->has_target);
  EXPECT_FALSE(right_movement->has_target);
  EXPECT_TRUE(left_movement->path.empty());
  EXPECT_TRUE(center_movement->path.empty());
  EXPECT_TRUE(right_movement->path.empty());
}

TEST_F(CommandServiceTest, GroupMoveFailureNearBoundaryDoesNotTeleportUnits) {
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
  pathfinder->update_building_obstacles();

  for (int world_z = -7; world_z <= 7; ++world_z) {
    Game::Systems::Point const wall_cell =
        Game::Systems::CommandService::world_to_grid(0.0F, static_cast<float>(world_z));
    pathfinder->set_obstacle(wall_cell.x, wall_cell.y, true);
  }

  Game::Systems::CommandService::MoveOptions options;
  options.allow_direct_fallback = false;
  options.group_move = true;

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

TEST_F(CommandServiceTest, GroupMoveCanRetryMembersIndividuallyAfterGroupFailure) {
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
  ASSERT_NE(left_movement, nullptr);
  ASSERT_NE(center_movement, nullptr);
  ASSERT_NE(right_movement, nullptr);

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_building_obstacles();

  for (int world_z = -15; world_z <= 15; ++world_z) {
    if (world_z >= -2 && world_z <= 2) {
      continue;
    }
    Game::Systems::Point const wall_cell =
        Game::Systems::CommandService::world_to_grid(0.0F, static_cast<float>(world_z));
    pathfinder->set_obstacle(wall_cell.x, wall_cell.y, true);
  }

  Game::Systems::CommandService::MoveOptions options;
  options.allow_direct_fallback = false;
  options.group_move = true;
  options.retry_individual_on_group_failure = true;

  Game::Systems::CommandService::move_units(
      world,
      {left->get_id(), center->get_id(), right->get_id()},
      {QVector3D(10.0F, 0.0F, -2.0F),
       QVector3D(10.0F, 0.0F, 0.0F),
       QVector3D(10.0F, 0.0F, 2.0F)},
      options);

  wait_for_path_results(world, {left_movement, center_movement, right_movement});

  EXPECT_TRUE(left_movement->has_target);
  EXPECT_TRUE(center_movement->has_target);
  EXPECT_TRUE(right_movement->has_target);
  EXPECT_FALSE(left_movement->path.empty());
  EXPECT_FALSE(center_movement->path.empty());
  EXPECT_FALSE(right_movement->path.empty());
}

TEST_F(CommandServiceTest, GroupMoveKeepsOrdersWhenTargetsOnlyHaveDiagonalObstacles) {
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
  ASSERT_NE(left_movement, nullptr);
  ASSERT_NE(center_movement, nullptr);
  ASSERT_NE(right_movement, nullptr);

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_building_obstacles();

  Game::Systems::Point const diagonal_block =
      Game::Systems::CommandService::world_to_grid(11.0F, 1.0F);
  pathfinder->set_obstacle(diagonal_block.x, diagonal_block.y, true);

  Game::Systems::CommandService::MoveOptions options;
  options.allow_direct_fallback = false;
  options.group_move = true;
  options.retry_individual_on_group_failure = true;

  Game::Systems::CommandService::move_units(
      world,
      {left->get_id(), center->get_id(), right->get_id()},
      {QVector3D(10.0F, 0.0F, -2.0F),
       QVector3D(10.0F, 0.0F, 0.0F),
       QVector3D(10.0F, 0.0F, 2.0F)},
      options);

  EXPECT_TRUE(left_movement->path_pending || left_movement->has_target);
  EXPECT_TRUE(center_movement->path_pending || center_movement->has_target);
  EXPECT_TRUE(right_movement->path_pending || right_movement->has_target);
}

TEST_F(CommandServiceTest, LocalRecoveryCanRelaxRadiusToEscapeBoundaryTrap) {
  Game::Systems::CommandService::initialize(16, 16);
  Engine::Core::World world;

  auto* unit = create_unit(world, -7.0F, 0.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(unit, nullptr);

  auto* movement = unit->get_component<Engine::Core::MovementComponent>();
  auto* transform = unit->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(transform, nullptr);

  EXPECT_TRUE(Game::Systems::CommandService::try_queue_local_recovery_move(
      world,
      unit->get_id(),
      QVector3D(transform->position.x, 0.0F, transform->position.z),
      QVector3D(5.0F, 0.0F, 0.0F),
      movement));

  EXPECT_TRUE(movement->has_target);
  EXPECT_GT(movement->target_x, transform->position.x);
  EXPECT_FLOAT_EQ(movement->goal_x, 5.0F);
  EXPECT_FLOAT_EQ(movement->goal_y, 0.0F);
}

TEST_F(CommandServiceTest, GroupBridgeCrossingUsesSharedBridgeCenterlineWaypoints) {
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
  pathfinder->mark_obstacles_dirty();
  pathfinder->update_building_obstacles();

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
  ASSERT_NE(left_movement, nullptr);
  ASSERT_NE(center_movement, nullptr);
  ASSERT_NE(right_movement, nullptr);

  Game::Systems::CommandService::MoveOptions options;
  options.allow_direct_fallback = false;
  options.group_move = true;

  Game::Systems::CommandService::move_units(
      world,
      {left->get_id(), center->get_id(), right->get_id()},
      {QVector3D(8.0F, 0.0F, -2.0F),
       QVector3D(8.0F, 0.0F, 0.0F),
       QVector3D(8.0F, 0.0F, 2.0F)},
      options);

  wait_for_path_results(world, {left_movement, center_movement, right_movement});

  auto const bridge = map_def.bridges.front();
  QVector3D bridge_dir = bridge.end - bridge.start;
  float const bridge_length = bridge_dir.length();
  ASSERT_GT(bridge_length, 0.0F);
  bridge_dir.normalize();

  auto assert_uses_bridge_centerline =
      [bridge, bridge_dir, bridge_length](
          const Engine::Core::MovementComponent* movement) {
        ASSERT_NE(movement, nullptr);
        ASSERT_FALSE(movement->path.empty())
            << "has_target=" << movement->has_target
            << " path_pending=" << movement->path_pending << " target=("
            << movement->target_x << ", " << movement->target_y << ") goal=("
            << movement->goal_x << ", " << movement->goal_y << ")";

        bool found_bridge_waypoint = false;
        bool found_bridge_approach_waypoint = false;
        for (auto const& [x, z] : movement->path) {
          auto const traversal_center =
              Game::Map::TerrainService::instance().get_bridge_traversal_position(x, z);
          if (!traversal_center.has_value()) {
            continue;
          }

          EXPECT_NEAR(x, traversal_center->x(), 1.0e-4F);
          EXPECT_NEAR(z, traversal_center->z(), 1.0e-4F);
          QVector3D const waypoint(x, 0.0F, z);
          float const along =
              QVector3D::dotProduct(waypoint - bridge.start, bridge_dir);
          if (along >= 0.0F && along <= bridge_length) {
            found_bridge_waypoint = true;
          } else {
            found_bridge_approach_waypoint = true;
          }
        }

        EXPECT_TRUE(found_bridge_waypoint);
        EXPECT_TRUE(found_bridge_approach_waypoint);
      };

  assert_uses_bridge_centerline(left_movement);
  assert_uses_bridge_centerline(center_movement);
  assert_uses_bridge_centerline(right_movement);
}

TEST_F(CommandServiceTest, UnitApproachingBridgeSnapsToTraversalCenterBeforeDeck) {
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
  pathfinder->mark_obstacles_dirty();
  pathfinder->update_building_obstacles();

  Engine::Core::World world;
  auto* unit = create_unit(world, -4.0F, 1.25F, Game::Units::SpawnType::Archer);
  ASSERT_NE(unit, nullptr);

  auto* transform = unit->get_component<Engine::Core::TransformComponent>();
  auto* movement = unit->get_component<Engine::Core::MovementComponent>();
  auto* terrain_ctx = unit->add_component<Engine::Core::TerrainContextComponent>();
  ASSERT_NE(transform, nullptr);
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(terrain_ctx, nullptr);

  movement->target_x = -1.5F;
  movement->target_y = 0.0F;
  movement->goal_x = 6.0F;
  movement->goal_y = 0.0F;
  movement->has_target = true;

  Game::Systems::MovementSystem movement_system;
  movement_system.update(&world, 0.1F);

  EXPECT_NEAR(transform->position.z, 0.0F, 1.0e-4F);
  EXPECT_LT(transform->position.x, -3.0F);
  EXPECT_GT(transform->position.x, -4.1F);
  EXPECT_TRUE(
      Game::Map::TerrainService::instance()
          .get_bridge_traversal_position(transform->position.x, transform->position.z)
          .has_value());
}

} // namespace
