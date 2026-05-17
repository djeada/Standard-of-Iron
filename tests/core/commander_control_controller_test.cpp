#include <cmath>
#include <gtest/gtest.h>

#include "app/core/commander_control_controller.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/command_service.h"
#include "game/systems/movement_system.h"
#include "game/systems/pathfinding.h"
#include "render/entity/registry.h"
#include "render/gl/camera.h"
#include "render/gl/humanoid/animation/animation_inputs.h"

namespace {

class CommanderControlControllerTest : public ::testing::Test {
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

  static auto create_commander(Engine::Core::World& world,
                               float x,
                               float z) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    if (entity == nullptr) {
      return nullptr;
    }

    auto* transform =
        entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    auto* movement = entity->add_component<Engine::Core::MovementComponent>();
    auto* commander = entity->add_component<Engine::Core::CommanderComponent>();
    if (transform == nullptr || unit == nullptr || movement == nullptr ||
        commander == nullptr) {
      return nullptr;
    }

    unit->health = 100;
    unit->max_health = 100;
    unit->owner_id = 1;
    unit->speed = 3.0F;
    unit->spawn_type = Game::Units::SpawnType::Knight;
    return entity;
  }

  static auto
  create_enemy(Engine::Core::World& world, float x, float z) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    if (entity == nullptr) {
      return nullptr;
    }

    auto* transform =
        entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    if (transform == nullptr || unit == nullptr) {
      return nullptr;
    }

    unit->health = 100;
    unit->max_health = 100;
    unit->owner_id = 2;
    unit->speed = 3.0F;
    unit->spawn_type = Game::Units::SpawnType::Knight;
    return entity;
  }
};

TEST_F(CommanderControlControllerTest, JumpForwardBypassesBlockedGroundCells) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  auto const blocked = Game::Systems::CommandService::world_to_grid(0.0F, 1.0F);
  pathfinder->set_obstacle(blocked.x, blocked.y, true);

  CommanderControlController controller;
  controller.input().forward = true;
  controller.request_jump();

  Render::GL::Camera camera;
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.4F));

  EXPECT_GT(transform->position.z, 0.9F);
  auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander_data, nullptr);
  EXPECT_TRUE(commander_data->jump_active);
}

TEST_F(CommanderControlControllerTest,
       JumpLandingSnapsBackWhenNoWalkableLandingExists) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  for (float const z : {1.0F, 2.0F, 3.0F}) {
    auto const blocked = Game::Systems::CommandService::world_to_grid(0.0F, z);
    pathfinder->set_obstacle(blocked.x, blocked.y, true);
  }

  CommanderControlController controller;
  controller.input().forward = true;
  controller.request_jump();

  Render::GL::Camera camera;
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.2F));
  float const first_safe_z = transform->position.z;
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.2F));
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.2F));

  EXPECT_NEAR(transform->position.z, first_safe_z, 0.0001F);
}

TEST_F(CommanderControlControllerTest,
       AirborneCommanderSkipsMovementRollbackAndRecovery) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  auto* movement = commander->get_component<Engine::Core::MovementComponent>();
  auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(transform, nullptr);
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(commander_data, nullptr);

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  auto const blocked = Game::Systems::CommandService::world_to_grid(
      transform->position.x, transform->position.z);
  pathfinder->set_obstacle(blocked.x, blocked.y, true);

  movement->has_target = true;
  movement->target_x = 0.0F;
  movement->target_y = 3.0F;
  movement->goal_x = 0.0F;
  movement->goal_y = 3.0F;
  movement->vx = 1.5F;
  movement->vz = 1.5F;
  commander_data->jump_active = true;

  Game::Systems::MovementSystem movement_system;
  movement_system.update(&world, 0.25F);

  EXPECT_FLOAT_EQ(transform->position.x, 0.0F);
  EXPECT_FLOAT_EQ(transform->position.z, 0.0F);
  EXPECT_TRUE(movement->has_target);
  EXPECT_FLOAT_EQ(movement->vx, 1.5F);
  EXPECT_FLOAT_EQ(movement->vz, 1.5F);
}

TEST_F(CommanderControlControllerTest,
       FpvCommanderMovementAnimationSurvivesMovementSystemUpdate) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
  auto* movement = commander->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(commander_data, nullptr);
  ASSERT_NE(movement, nullptr);
  commander_data->fpv_controlled = true;

  CommanderControlController controller;
  controller.input().forward = true;

  Render::GL::Camera camera;
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.2F));

  world.add_system(std::make_unique<Game::Systems::MovementSystem>());
  world.update(0.2F);

  Render::GL::DrawContext ctx{};
  ctx.entity = commander;
  ctx.animation_time = 0.2F;

  auto anim = Render::GL::sample_anim_state(ctx);

  EXPECT_FALSE(movement->has_target);
  EXPECT_FLOAT_EQ(movement->vx, 0.0F);
  EXPECT_FLOAT_EQ(movement->vz, 0.0F);
  EXPECT_GT(std::abs(commander_data->fpv_motion_vx) +
                std::abs(commander_data->fpv_motion_vz),
            0.05F);
  EXPECT_TRUE(Render::Creature::is_moving_animation(anim.movement_state));
}

TEST_F(CommanderControlControllerTest, LockOnDropsAfterSustainedBuildingOcclusion) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  auto* enemy = create_enemy(world, 0.0F, 6.0F);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(enemy, nullptr);

  CommanderControlController controller;
  controller.cycle_lock_on_target(world, commander->get_id(), 1);
  ASSERT_EQ(controller.locked_target_id(), enemy->get_id());
  auto* targets = commander->get_component<Engine::Core::RpgCommanderTargetComponent>();
  ASSERT_NE(targets, nullptr);
  EXPECT_EQ(targets->explicit_lock_target_id, enemy->get_id());

  Game::Systems::BuildingCollisionRegistry::instance().register_building(
      999U, "barracks", 0.0F, 3.0F, 2);

  Render::GL::Camera camera;
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.2F));
  EXPECT_EQ(controller.locked_target_id(), enemy->get_id());

  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.2F));
  EXPECT_EQ(controller.locked_target_id(), 0U);
}

TEST_F(CommanderControlControllerTest, SoftAimDoesNotBecomeCommanderFocus) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  auto* enemy = create_enemy(world, 0.0F, 6.0F);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(enemy, nullptr);

  CommanderControlController controller;
  controller.set_view_yaw(0.0F);

  Render::GL::Camera camera;
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.016F));

  EXPECT_EQ(controller.locked_target_id(), 0U);
  EXPECT_EQ(controller.focus_target_id(), 0U);
}

TEST_F(CommanderControlControllerTest, RunBackwardBreaksCommanderLockOn) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  auto* enemy = create_enemy(world, 0.0F, 6.0F);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(enemy, nullptr);

  CommanderControlController controller;
  controller.cycle_lock_on_target(world, commander->get_id(), 1);
  ASSERT_EQ(controller.locked_target_id(), enemy->get_id());

  controller.input().backward = true;
  controller.input().run = true;

  Render::GL::Camera camera;
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.016F));

  EXPECT_EQ(controller.locked_target_id(), 0U);
}

TEST_F(CommanderControlControllerTest, LockOnRequiresIntentionalRangeAndCone) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  auto* side_enemy = create_enemy(world, 6.0F, 0.0F);
  auto* far_enemy = create_enemy(world, 0.0F, 14.0F);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(side_enemy, nullptr);
  ASSERT_NE(far_enemy, nullptr);

  CommanderControlController controller;
  controller.set_view_yaw(0.0F);
  controller.cycle_lock_on_target(world, commander->get_id(), 1);

  EXPECT_EQ(controller.locked_target_id(), 0U);
}

TEST_F(CommanderControlControllerTest, CommanderUpdateIgnoresStaleRtsMeleeLockState) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  auto* attack = commander->add_component<Engine::Core::AttackComponent>();
  ASSERT_NE(attack, nullptr);
  attack->in_melee_lock = true;
  attack->melee_lock_target_id = 99U;

  CommanderControlController controller;
  Render::GL::Camera camera;
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.016F));

  EXPECT_TRUE(attack->in_melee_lock);
  EXPECT_EQ(attack->melee_lock_target_id, 99U);
}

TEST_F(CommanderControlControllerTest,
       PrimaryStrikeUsesRpgDamageWithoutRtsAttackTarget) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  auto* enemy = create_enemy(world, 0.0F, 1.2F);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(enemy, nullptr);

  auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander_data, nullptr);
  commander_data->fpv_controlled = true;

  auto* attack = commander->add_component<Engine::Core::AttackComponent>();
  ASSERT_NE(attack, nullptr);
  attack->can_melee = true;
  attack->can_ranged = false;
  attack->melee_damage = 17;
  attack->melee_range = 1.6F;

  auto* enemy_unit = enemy->get_component<Engine::Core::UnitComponent>();
  ASSERT_NE(enemy_unit, nullptr);

  CommanderControlController controller;
  ASSERT_TRUE(controller.primary_action(world, commander->get_id(), 1));

  EXPECT_EQ(enemy_unit->health, 83);
  EXPECT_FALSE(commander->has_component<Engine::Core::AttackTargetComponent>());
  EXPECT_TRUE(commander_data->just_struck_enemy);
  auto* action = commander->get_component<Engine::Core::RpgCommanderActionComponent>();
  ASSERT_NE(action, nullptr);
  EXPECT_EQ(action->phase, Engine::Core::RpgCommanderActionPhase::Strike);
  EXPECT_EQ(action->active_target_id, enemy->get_id());
  EXPECT_EQ(action->last_hit_target_id, enemy->get_id());
  auto* targets = commander->get_component<Engine::Core::RpgCommanderTargetComponent>();
  ASSERT_NE(targets, nullptr);
  EXPECT_EQ(targets->recent_hit_target_id, enemy->get_id());
}

TEST_F(CommanderControlControllerTest, PrimaryActionAlternatesSwordSwaysAcrossClicks) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander_data, nullptr);
  commander_data->fpv_controlled = true;

  auto* attack = commander->add_component<Engine::Core::AttackComponent>();
  ASSERT_NE(attack, nullptr);
  attack->can_melee = true;
  attack->can_ranged = false;
  attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;

  CommanderControlController controller;
  ASSERT_TRUE(controller.primary_action(world, commander->get_id(), 1));

  auto* combat_state = commander->get_component<Engine::Core::CombatStateComponent>();
  ASSERT_NE(combat_state, nullptr);
  EXPECT_EQ(combat_state->attack_variant, 0U);
  EXPECT_FALSE(combat_state->finisher_attack);

  auto* action = commander->get_component<Engine::Core::RpgCommanderActionComponent>();
  ASSERT_NE(action, nullptr);
  EXPECT_EQ(action->melee_attack_style, 0U);
  EXPECT_EQ(action->melee_attack_sequence, 1U);

  combat_state->animation_state = Engine::Core::CombatAnimationState::Idle;
  combat_state->state_time = 0.0F;
  combat_state->state_duration = 0.0F;

  ASSERT_TRUE(controller.primary_action(world, commander->get_id(), 1));

  EXPECT_EQ(combat_state->attack_variant, 1U);
  EXPECT_EQ(action->melee_attack_style, 1U);
  EXPECT_EQ(action->melee_attack_sequence, 0U);
}

TEST_F(CommanderControlControllerTest, PrimaryActionUsesDedicatedFinisherSwordSway) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander_data, nullptr);
  commander_data->fpv_controlled = true;
  commander_data->combo_step = 3;

  auto* attack = commander->add_component<Engine::Core::AttackComponent>();
  ASSERT_NE(attack, nullptr);
  attack->can_melee = true;
  attack->can_ranged = false;
  attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;

  CommanderControlController controller;
  ASSERT_TRUE(controller.primary_action(world, commander->get_id(), 1));

  auto* combat_state = commander->get_component<Engine::Core::CombatStateComponent>();
  ASSERT_NE(combat_state, nullptr);
  EXPECT_EQ(combat_state->attack_variant, 2U);
  EXPECT_TRUE(combat_state->finisher_attack);

  auto* action = commander->get_component<Engine::Core::RpgCommanderActionComponent>();
  ASSERT_NE(action, nullptr);
  EXPECT_EQ(action->melee_attack_style, 2U);
}

TEST_F(CommanderControlControllerTest,
       ComboStepDoesNotResetWhileAttackAnimationIsActive) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander_data, nullptr);
  commander_data->fpv_controlled = true;
  commander_data->combo_step = 1;

  auto* action = commander->add_component<Engine::Core::RpgCommanderActionComponent>();
  ASSERT_NE(action, nullptr);
  action->melee_attack_sequence = 1U;

  auto* combat_state = commander->add_component<Engine::Core::CombatStateComponent>();
  ASSERT_NE(combat_state, nullptr);
  combat_state->animation_state = Engine::Core::CombatAnimationState::Recover;
  combat_state->state_duration = 0.5F;
  combat_state->state_time = 0.2F;

  CommanderControlController controller;
  Render::GL::Camera camera;
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 1.2F));

  EXPECT_EQ(commander_data->combo_step, 1);
  EXPECT_EQ(action->melee_attack_sequence, 1U);

  combat_state->animation_state = Engine::Core::CombatAnimationState::Idle;
  combat_state->state_duration = 0.0F;
  combat_state->state_time = 0.0F;

  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 1.2F));

  EXPECT_EQ(commander_data->combo_step, 0);
  EXPECT_EQ(action->melee_attack_sequence, 0U);
}

TEST_F(CommanderControlControllerTest, CloseCameraModeShortensCommanderViewDistance) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  CommanderControlController controller;
  Render::GL::Camera camera;
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.016F));
  const float chase_distance = camera.get_distance();

  controller.toggle_close_camera_mode(world, commander->get_id(), 1);
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.016F));

  EXPECT_LT(camera.get_distance(), chase_distance);
}

TEST_F(CommanderControlControllerTest, CommanderCameraUsesCloseNearPlane) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  CommanderControlController controller;
  Render::GL::Camera camera;
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.016F));

  EXPECT_NEAR(camera.get_near(), 0.05F, 0.0001F);
}

TEST_F(CommanderControlControllerTest,
       CommanderCameraOcclusionPullsViewCloserToTheCommander) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  Game::Systems::BuildingCollisionRegistry::instance().register_building(
      321U, "barracks", 0.0F, -1.0F, 2);

  CommanderControlController controller;
  Render::GL::Camera camera;
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.016F));

  EXPECT_GT(camera.get_position().z(), -1.6F);
}

TEST_F(CommanderControlControllerTest, VanguardRushClosesDistanceAndStaggersTarget) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  auto* enemy = create_enemy(world, 0.0F, 4.8F);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(enemy, nullptr);

  CommanderControlController controller;
  controller.cycle_lock_on_target(world, commander->get_id(), 1);
  controller.request_vanguard_rush();

  Render::GL::Camera camera;
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.016F));

  auto* commander_transform =
      commander->get_component<Engine::Core::TransformComponent>();
  auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
  auto* enemy_unit = enemy->get_component<Engine::Core::UnitComponent>();
  ASSERT_NE(commander_transform, nullptr);
  ASSERT_NE(commander_data, nullptr);
  ASSERT_NE(enemy_unit, nullptr);

  EXPECT_GT(commander_transform->position.z, 1.0F);
  EXPECT_LT(enemy_unit->health, 100);
  EXPECT_GT(commander_data->vanguard_rush_cooldown_remaining, 0.0F);
  EXPECT_TRUE(enemy->has_component<Engine::Core::StaggerComponent>());
}

TEST_F(CommanderControlControllerTest, SecondWindRestoresPostureAndStamina) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander_data, nullptr);
  commander_data->posture = 72.0F;

  auto* stamina = commander->add_component<Engine::Core::StaminaComponent>();
  ASSERT_NE(stamina, nullptr);
  stamina->stamina = 18.0F;
  stamina->max_stamina = 100.0F;

  CommanderControlController controller;
  controller.request_second_wind();

  Render::GL::Camera camera;
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.016F));

  EXPECT_LT(commander_data->posture, 72.0F);
  EXPECT_GT(stamina->stamina, 18.0F);
  EXPECT_GT(commander_data->second_wind_cooldown_remaining, 0.0F);
}

TEST_F(CommanderControlControllerTest, SecondaryActionActivatesCommanderGuard) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  CommanderControlController controller;
  controller.secondary_action_down();

  Render::GL::Camera camera;
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.016F));

  auto* guard = commander->get_component<Engine::Core::CommanderGuardComponent>();
  ASSERT_NE(guard, nullptr);
  EXPECT_TRUE(guard->active);
}

} // namespace
