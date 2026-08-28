#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <vector>

#include "app/commander/commander_control_controller.h"
#include "app/commander/commander_mode_coordinator.h"
#include "game/audio/audio_cues.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/combat_actions/combat_action_definition.h"
#include "game/systems/combat_actions/combat_action_events.h"
#include "game/systems/movement_pipeline.h"
#include "game/systems/nav_grid.h"
#include "game/systems/pathfinding.h"
#include "game/systems/rpg_combat_system/rpg_targeting.h"
#include "game/systems/terrain_alignment_system.h"
#include "render/entity/registry.h"
#include "render/gl/humanoid/animation/animation_inputs.h"
#include "scene/camera.h"
#include "tests/support/movement_test_access.h"

namespace {

class CommanderControlControllerTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::NavGrid::initialize(32, 32);
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

  [[nodiscard]] static auto press_primary(CommanderControlController& controller,
                                          Engine::Core::World& world,
                                          Engine::Core::EntityID commander_id) -> bool {
    controller.primary_action_down();
    controller.primary_action_up();
    return controller.update_simulation(world, commander_id, 1, 0.016F);
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

TEST_F(CommanderControlControllerTest,
       CommanderWalksUpToTheDrawnFacadeNotTheNavFootprint) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, -5.0F);
  ASSERT_NE(commander, nullptr);
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);

  auto& registry = Game::Systems::BuildingCollisionRegistry::instance();
  registry.register_building(9001U, "home", 0.0F, 0.0F, 2, 0.0F);

  auto const nav = Game::Systems::BuildingCollisionRegistry::get_building_size("home");
  auto const body = Game::Systems::BuildingCollisionRegistry::get_building_body("home");
  ASSERT_LT(body.depth, nav.depth)
      << "this test only means something while the nav footprint is the larger one";

  CommanderControlController controller;
  Render::GL::Camera camera;
  constexpr float k_dt = 1.0F / 60.0F;
  for (int frame = 0; frame < 240; ++frame) {
    controller.input().forward = true;
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
  }

  float const reached = std::abs(transform->position.z);
  float const facade = (body.depth * 0.5F) - body.offset_z;
  EXPECT_LT(reached, facade + 0.45F)
      << "the commander stopped " << reached << " m out; the drawn facade is at "
      << facade << " m";
  EXPECT_GT(reached, facade) << "the commander walked inside the drawn building";
}

TEST_F(CommanderControlControllerTest, JumpForwardBypassesBlockedGroundCells) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);

  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  auto const blocked = Game::Systems::NavGrid::world_to_grid(0.0F, 1.0F);
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

TEST_F(CommanderControlControllerTest, WalkingAsksForAFootstepOnEveryStride) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  auto& registry = Game::Audio::CueRegistry::instance();
  registry.clear();

  CommanderControlController controller;
  controller.input().forward = true;

  Render::GL::Camera camera;
  for (int frame = 0; frame < 90; ++frame) {
    controller.input().forward = true;
    static_cast<void>(
        controller.update(world, commander->get_id(), 1, camera, 1.0F / 30.0F));
  }

  const std::vector<std::string> asked = registry.silent_cues();
  EXPECT_NE(std::find(asked.begin(), asked.end(), Game::Audio::Cue::k_move_footstep),
            asked.end())
      << "three seconds of walking never asked for a footstep";

  registry.clear();
}

TEST_F(CommanderControlControllerTest, ScriptedDodgeUsesRequestedWorldDirection) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);
  auto* rpg = commander->add_component<Engine::Core::RpgHealthComponent>();
  ASSERT_NE(rpg, nullptr);
  rpg->active = true;

  CommanderControlController controller;
  controller.set_view_yaw(0.0F);
  controller.request_dodge(QVector3D(0.0F, 0.0F, -1.0F));

  Render::GL::Camera camera;
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.10F));

  EXPECT_NEAR(transform->position.x, 0.0F, 0.0001F);
  EXPECT_LT(transform->position.z, -0.60F);
  EXPECT_TRUE(controller.is_dodge_rolling());

  EXPECT_GT(rpg->dodge_grace_remaining, 0.0F);
  EXPECT_LT(rpg->dodge_dir_z, -0.5F);
}

TEST_F(CommanderControlControllerTest,
       JumpLandingInsideAnObstacleRecoversOntoWalkableGround) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);

  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  for (float const z : {1.0F, 2.0F, 3.0F}) {
    auto const blocked = Game::Systems::NavGrid::world_to_grid(0.0F, z);
    pathfinder->set_obstacle(blocked.x, blocked.y, true);
  }

  CommanderControlController controller;
  controller.input().forward = true;
  controller.request_jump();

  Render::GL::Camera camera;
  auto& session = Game::Session::session_for(world);
  for (int frame = 0; frame < 8; ++frame) {
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.2F));
  }

  EXPECT_TRUE(App::Core::CommanderMotor::is_walkable_at(
      session, transform->position.x, transform->position.z))
      << "landed at " << transform->position.x << ", " << transform->position.z;

  float const settled_x = transform->position.x;
  float const settled_z = transform->position.z;
  controller.input().forward = false;
  controller.input().backward = true;
  for (int frame = 0; frame < 4; ++frame) {
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.1F));
  }
  EXPECT_GT(
      std::hypot(transform->position.x - settled_x, transform->position.z - settled_z),
      0.05F)
      << "commander froze after landing";
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

  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  auto const blocked = Game::Systems::NavGrid::world_to_grid(transform->position.x,
                                                             transform->position.z);
  pathfinder->set_obstacle(blocked.x, blocked.y, true);

  MovementTestAccess::set_has_target(*movement, true);
  MovementTestAccess::set_target_x(*movement, 0.0F);
  MovementTestAccess::set_target_y(*movement, 3.0F);
  MovementTestAccess::set_goal_x(*movement, 0.0F);
  MovementTestAccess::set_goal_y(*movement, 3.0F);
  MovementTestAccess::set_vx(*movement, 1.5F);
  MovementTestAccess::set_vz(*movement, 1.5F);
  commander_data->jump_active = true;

  Game::Systems::MovementPipeline movement_system;
  movement_system.update(&world, 0.25F);

  EXPECT_FLOAT_EQ(transform->position.x, 0.0F);
  EXPECT_FLOAT_EQ(transform->position.z, 0.0F);
  EXPECT_TRUE(movement->get_has_target());
  EXPECT_FLOAT_EQ(movement->get_vx(), 1.5F);
  EXPECT_FLOAT_EQ(movement->get_vz(), 1.5F);
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

  world.add_system(std::make_unique<Game::Systems::MovementPipeline>());
  world.update(0.2F);

  Render::GL::DrawContext ctx{};
  ctx.entity = commander;
  ctx.animation_time = 0.2F;

  auto anim = Render::GL::sample_anim_state(ctx);

  EXPECT_FALSE(movement->get_has_target());
  EXPECT_FLOAT_EQ(movement->get_vx(), 0.0F);
  EXPECT_FLOAT_EQ(movement->get_vz(), 0.0F);
  EXPECT_GT(std::abs(commander_data->fpv_motion_vx) +
                std::abs(commander_data->fpv_motion_vz),
            0.05F);
  EXPECT_TRUE(Render::Creature::is_moving_animation(anim.movement_state));
}

TEST_F(CommanderControlControllerTest,
       FpvCommanderMovementAnimationSurvivesSustainedWalking) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander_data, nullptr);
  commander_data->fpv_controlled = true;

  CommanderControlController controller;
  controller.input().forward = true;

  Render::GL::Camera camera;
  world.add_system(std::make_unique<Game::Systems::MovementPipeline>());

  constexpr float k_step = 0.05F;
  float elapsed = 0.0F;
  for (int frame = 0; frame < 40; ++frame) {
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_step));
    world.update(k_step);
    elapsed += k_step;

    Render::GL::DrawContext ctx{};
    ctx.entity = commander;
    ctx.animation_time = elapsed;
    auto anim = Render::GL::sample_anim_state(ctx);
    ASSERT_TRUE(Render::Creature::is_moving_animation(anim.movement_state))
        << "locomotion went stale after " << elapsed << "s of held input";
  }

  controller.input().forward = false;
  for (int frame = 0; frame < 20; ++frame) {
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_step));
    world.update(k_step);
    elapsed += k_step;
  }

  Render::GL::DrawContext idle_ctx{};
  idle_ctx.entity = commander;
  idle_ctx.animation_time = elapsed;
  auto const idle_anim = Render::GL::sample_anim_state(idle_ctx);
  EXPECT_FALSE(Render::Creature::is_moving_animation(idle_anim.movement_state));
}

TEST_F(CommanderControlControllerTest,
       FpvCommanderAttackAnimationPrefersCombatActionIdOverLegacyStyle) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander_data, nullptr);
  commander_data->fpv_controlled = true;

  auto* attack = commander->add_component<Engine::Core::AttackComponent>();
  ASSERT_NE(attack, nullptr);
  attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;

  auto* combat_state = commander->add_component<Engine::Core::CombatStateComponent>();
  ASSERT_NE(combat_state, nullptr);
  combat_state->animation_state = Engine::Core::CombatAnimationState::Strike;
  combat_state->state_time = 0.1F;
  combat_state->state_duration = 0.4F;
  combat_state->attack_family = Engine::Core::CombatAttackFamily::Sword;

  auto* action = commander->add_component<Engine::Core::RpgCommanderActionComponent>();
  ASSERT_NE(action, nullptr);
  action->phase = Engine::Core::RpgCommanderActionPhase::Strike;
  action->combat_action_id = static_cast<std::uint8_t>(
      Game::Systems::CombatActions::CombatActionId::RpgSwordThrust);

  Render::GL::DrawContext ctx{};
  ctx.entity = commander;
  ctx.animation_time = 0.25F;

  auto const anim = Render::GL::sample_anim_state(ctx);

  EXPECT_TRUE(anim.has_sword_attack_animation);
  EXPECT_EQ(anim.sword_attack_animation, Animation::SwordAttackAnimation::RpgThrust);
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

TEST_F(CommanderControlControllerTest, AimCandidateTracksExactInRangeFormationSoldier) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  auto* enemy = create_enemy(world, 0.0F, 1.8F);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(enemy, nullptr);
  auto* enemy_unit = enemy->get_component<Engine::Core::UnitComponent>();
  ASSERT_NE(enemy_unit, nullptr);
  enemy_unit->render_individuals_per_unit_override = 6;

  CommanderControlController controller;
  controller.set_view_yaw(0.0F);
  Render::GL::Camera camera;
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.016F));

  auto const* targets =
      commander->get_component<Engine::Core::RpgCommanderTargetComponent>();
  ASSERT_NE(targets, nullptr);
  EXPECT_TRUE(targets->aim_candidate_in_range);
  EXPECT_EQ(targets->aim_candidate_id, enemy->get_id());
  EXPECT_NE(targets->aim_candidate_soldier_slot,
            Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot);
  auto const exact_target = Game::Systems::RpgCombat::resolve_soldier_target(
      *enemy, targets->aim_candidate_soldier_slot);
  ASSERT_TRUE(exact_target.has_value());
  EXPECT_TRUE(Game::Systems::RpgCombat::target_in_melee_envelope(
      *commander, *exact_target, 2.05F));

  auto* enemy_transform = enemy->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(enemy_transform, nullptr);
  enemy_transform->position.z = 10.0F;
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.016F));

  EXPECT_FALSE(targets->aim_candidate_in_range);
  EXPECT_EQ(targets->aim_candidate_id, 0U);
  EXPECT_EQ(targets->aim_candidate_soldier_slot,
            Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot);
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
  attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
  attack->melee_damage = 17;
  attack->melee_range = 1.6F;

  auto* enemy_unit = enemy->get_component<Engine::Core::UnitComponent>();
  ASSERT_NE(enemy_unit, nullptr);

  CommanderControlController controller;
  ASSERT_TRUE(press_primary(controller, world, commander->get_id()));

  EXPECT_EQ(enemy_unit->health, 100);
  EXPECT_FALSE(commander->has_component<Engine::Core::AttackTargetComponent>());
  EXPECT_FALSE(commander_data->just_struck_enemy);
  auto* action = commander->get_component<Engine::Core::RpgCommanderActionComponent>();
  ASSERT_NE(action, nullptr);
  EXPECT_EQ(action->phase, Engine::Core::RpgCommanderActionPhase::Strike);
  EXPECT_EQ(action->active_target_id, enemy->get_id());
  EXPECT_EQ(action->last_hit_target_id, 0U);
  auto* targets = commander->get_component<Engine::Core::RpgCommanderTargetComponent>();
  ASSERT_NE(targets, nullptr);
  EXPECT_EQ(targets->explicit_lock_target_id, 0U)
      << "a strike aims at what is in front of the commander; it does not lock on";
}

TEST_F(CommanderControlControllerTest,
       PrimaryActionGrowsTheNextSwingOutOfWhereTheLastOneFinished) {
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
  Render::GL::Camera camera;
  ASSERT_TRUE(press_primary(controller, world, commander->get_id()));

  auto* combat_state = commander->get_component<Engine::Core::CombatStateComponent>();
  ASSERT_NE(combat_state, nullptr);
  EXPECT_EQ(combat_state->attack_variant, 0U);
  EXPECT_FALSE(combat_state->finisher_attack);

  auto* action = commander->get_component<Engine::Core::RpgCommanderActionComponent>();
  ASSERT_NE(action, nullptr);
  EXPECT_EQ(action->combat_action_id,
            static_cast<std::uint8_t>(
                Game::Systems::CombatActions::CombatActionId::RpgSwordSlashLeft));
  EXPECT_EQ(action->melee_attack_sequence, 1U);

  auto const first_swing = combat_state->intent;
  EXPECT_LT(first_swing.strike_dir_x, 0.0F);
  EXPECT_LT(first_swing.strike_dir_y, 0.0F);

  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.016F));
  combat_state->animation_state = Engine::Core::CombatAnimationState::Idle;
  combat_state->state_time = 0.0F;
  combat_state->state_duration = 0.0F;
  action->action_running = false;
  action->action_completed = true;
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.016F));

  ASSERT_TRUE(press_primary(controller, world, commander->get_id()));

  EXPECT_EQ(combat_state->attack_variant, 0U);
  EXPECT_GT(combat_state->intent.strike_dir_x, 0.0F);
  EXPECT_GT(combat_state->intent.strike_dir_y, 0.0F);
  EXPECT_EQ(action->combat_action_id,
            static_cast<std::uint8_t>(
                Game::Systems::CombatActions::CombatActionId::RpgSwordSlashRight));
  EXPECT_EQ(action->melee_attack_sequence, 2U);
}

TEST_F(CommanderControlControllerTest,
       RpgSwordActionDefinitionsCoverCurrentCommanderSwordStyles) {
  using Game::Systems::CombatActions::CombatActionId;
  using Game::Systems::CombatActions::find_combat_action_definition;
  using Game::Systems::CombatActions::WeaponFamily;

  auto const* left = find_combat_action_definition(CombatActionId::RpgSwordSlashLeft);
  auto const* right = find_combat_action_definition(CombatActionId::RpgSwordSlashRight);
  auto const* overhead =
      find_combat_action_definition(CombatActionId::RpgSwordOverhead);
  auto const* thrust = find_combat_action_definition(CombatActionId::RpgSwordThrust);
  auto const* finisher =
      find_combat_action_definition(CombatActionId::RpgSwordFinisher);

  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);
  ASSERT_NE(overhead, nullptr);
  ASSERT_NE(thrust, nullptr);
  ASSERT_NE(finisher, nullptr);

  EXPECT_EQ(left->weapon_family, WeaponFamily::Sword);
  EXPECT_EQ(left->sword_clip, Animation::SwordAttackAnimation::RpgSlashLeft);
  EXPECT_EQ(right->sword_clip, Animation::SwordAttackAnimation::RpgSlashRight);
  EXPECT_EQ(finisher->sword_clip, Animation::SwordAttackAnimation::RpgFinisher);
  EXPECT_EQ(overhead->sword_clip, Animation::SwordAttackAnimation::RpgOverhead);
  EXPECT_EQ(thrust->sword_clip, Animation::SwordAttackAnimation::RpgThrust);
  EXPECT_EQ(finisher->attack_direction, Engine::Core::AttackDirection::HeavyOverhead);
  EXPECT_FALSE(left->events.empty());
  EXPECT_FALSE(finisher->events.empty());
}

TEST_F(CommanderControlControllerTest, RpgSpearActionDefinitionsUseSpearFamily) {
  using Game::Systems::CombatActions::CombatActionId;
  using Game::Systems::CombatActions::find_combat_action_definition;
  using Game::Systems::CombatActions::WeaponFamily;

  auto const* thrust = find_combat_action_definition(CombatActionId::RpgSpearThrust);
  auto const* sweep = find_combat_action_definition(CombatActionId::RpgSpearSweep);

  ASSERT_NE(thrust, nullptr);
  ASSERT_NE(sweep, nullptr);
  EXPECT_EQ(thrust->weapon_family, WeaponFamily::Spear);
  EXPECT_EQ(thrust->attack_family, Engine::Core::CombatAttackFamily::Spear);
  EXPECT_EQ(thrust->attack_direction, Engine::Core::AttackDirection::Thrust);
  EXPECT_GT(thrust->hit_shape.reach, sweep->hit_shape.reach);
  EXPECT_LT(thrust->hit_shape.radius, sweep->hit_shape.radius);
  EXPECT_EQ(sweep->max_targets, 6);
  EXPECT_TRUE(thrust->commander_only);
  EXPECT_TRUE(sweep->commander_only);
  EXPECT_FALSE(thrust->events.empty());
  EXPECT_FALSE(sweep->events.empty());
}

TEST_F(CommanderControlControllerTest, RpgBowActionDefinitionUsesProjectileRelease) {
  using Game::Systems::CombatActions::CombatActionEventType;
  using Game::Systems::CombatActions::CombatActionId;
  using Game::Systems::CombatActions::find_combat_action_definition;
  using Game::Systems::CombatActions::WeaponFamily;

  auto const* bow = find_combat_action_definition(CombatActionId::RpgBowShot);
  ASSERT_NE(bow, nullptr);

  EXPECT_EQ(bow->weapon_family, WeaponFamily::Bow);
  EXPECT_EQ(bow->attack_family, Engine::Core::CombatAttackFamily::Bow);
  EXPECT_TRUE(bow->requires_projectile_release);
  EXPECT_FALSE(bow->events.empty());
  EXPECT_NE(std::find_if(bow->events.begin(),
                         bow->events.end(),
                         [](auto const& event) {
                           return event.type ==
                                  CombatActionEventType::ProjectileRelease;
                         }),
            bow->events.end());
}

TEST_F(CommanderControlControllerTest, MountedActionDefinitionsUseMountedProfiles) {
  using Game::Systems::CombatActions::CombatActionId;
  using Game::Systems::CombatActions::find_combat_action_definition;
  using Game::Systems::CombatActions::WeaponFamily;

  auto const* sword = find_combat_action_definition(CombatActionId::MountedSwordSlash);
  auto const* spear = find_combat_action_definition(CombatActionId::MountedSpearThrust);
  auto const* infantry_sword =
      find_combat_action_definition(CombatActionId::RpgSwordSlashRight);
  auto const* infantry_spear =
      find_combat_action_definition(CombatActionId::RpgSpearThrust);

  ASSERT_NE(sword, nullptr);
  ASSERT_NE(spear, nullptr);
  ASSERT_NE(infantry_sword, nullptr);
  ASSERT_NE(infantry_spear, nullptr);

  EXPECT_EQ(sword->weapon_family, WeaponFamily::Sword);
  EXPECT_EQ(sword->attack_family, Engine::Core::CombatAttackFamily::Sword);
  EXPECT_EQ(spear->weapon_family, WeaponFamily::Spear);
  EXPECT_EQ(spear->attack_family, Engine::Core::CombatAttackFamily::Spear);
  EXPECT_GT(sword->hit_shape.reach, infantry_sword->hit_shape.reach);
  EXPECT_GT(spear->hit_shape.reach, infantry_spear->hit_shape.reach);
  EXPECT_EQ(sword->max_targets, 2);
  EXPECT_EQ(spear->max_targets, 2);
  EXPECT_FALSE(sword->events.empty());
  EXPECT_FALSE(spear->events.empty());
}

TEST_F(CommanderControlControllerTest, PrimaryActionUsesSpearActionForSpearCommander) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  auto* unit = commander->get_component<Engine::Core::UnitComponent>();
  auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(commander_data, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Spearman;
  commander_data->fpv_controlled = true;

  auto* attack = commander->add_component<Engine::Core::AttackComponent>();
  ASSERT_NE(attack, nullptr);
  attack->can_melee = true;
  attack->can_ranged = false;
  attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
  attack->melee_range = 2.6F;

  CommanderControlController controller;
  ASSERT_TRUE(press_primary(controller, world, commander->get_id()));

  auto* combat_state = commander->get_component<Engine::Core::CombatStateComponent>();
  auto* action = commander->get_component<Engine::Core::RpgCommanderActionComponent>();
  ASSERT_NE(combat_state, nullptr);
  ASSERT_NE(action, nullptr);

  EXPECT_EQ(combat_state->attack_family, Engine::Core::CombatAttackFamily::Spear);
  EXPECT_EQ(combat_state->attack_direction(), Engine::Core::AttackDirection::Thrust);
  EXPECT_EQ(action->combat_action_id,
            static_cast<std::uint8_t>(
                Game::Systems::CombatActions::CombatActionId::RpgSpearThrust));
}

TEST_F(CommanderControlControllerTest,
       PrimaryActionUsesMountedSwordActionForMountedCommander) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  auto* unit = commander->get_component<Engine::Core::UnitComponent>();
  auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(commander_data, nullptr);
  unit->spawn_type = Game::Units::SpawnType::MountedKnight;
  commander_data->fpv_controlled = true;

  auto* attack = commander->add_component<Engine::Core::AttackComponent>();
  ASSERT_NE(attack, nullptr);
  attack->can_melee = true;
  attack->can_ranged = false;
  attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
  attack->melee_range = 2.2F;

  CommanderControlController controller;
  ASSERT_TRUE(press_primary(controller, world, commander->get_id()));

  auto* combat_state = commander->get_component<Engine::Core::CombatStateComponent>();
  auto* action = commander->get_component<Engine::Core::RpgCommanderActionComponent>();
  ASSERT_NE(combat_state, nullptr);
  ASSERT_NE(action, nullptr);

  EXPECT_EQ(combat_state->attack_family, Engine::Core::CombatAttackFamily::Sword);

  EXPECT_EQ(combat_state->attack_direction(), Engine::Core::AttackDirection::LeftSlash);
  EXPECT_EQ(action->combat_action_id,
            static_cast<std::uint8_t>(
                Game::Systems::CombatActions::CombatActionId::MountedSwordSlash));
}

TEST_F(CommanderControlControllerTest,
       PrimaryActionUsesMountedSpearActionForHorseSpearmanCommander) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  auto* unit = commander->get_component<Engine::Core::UnitComponent>();
  auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(commander_data, nullptr);
  unit->spawn_type = Game::Units::SpawnType::HorseSpearman;
  commander_data->fpv_controlled = true;

  auto* attack = commander->add_component<Engine::Core::AttackComponent>();
  ASSERT_NE(attack, nullptr);
  attack->can_melee = true;
  attack->can_ranged = false;
  attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
  attack->melee_range = 3.0F;

  CommanderControlController controller;
  ASSERT_TRUE(press_primary(controller, world, commander->get_id()));

  auto* combat_state = commander->get_component<Engine::Core::CombatStateComponent>();
  auto* action = commander->get_component<Engine::Core::RpgCommanderActionComponent>();
  ASSERT_NE(combat_state, nullptr);
  ASSERT_NE(action, nullptr);

  EXPECT_EQ(combat_state->attack_family, Engine::Core::CombatAttackFamily::Spear);
  EXPECT_EQ(combat_state->attack_direction(), Engine::Core::AttackDirection::Thrust);
  EXPECT_EQ(action->combat_action_id,
            static_cast<std::uint8_t>(
                Game::Systems::CombatActions::CombatActionId::MountedSpearThrust));
}

TEST_F(CommanderControlControllerTest, PrimaryActionUsesBowActionForRangedCommander) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  auto* enemy = create_enemy(world, 0.0F, 2.0F);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(enemy, nullptr);

  auto* unit = commander->get_component<Engine::Core::UnitComponent>();
  auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(commander_data, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Archer;
  commander_data->fpv_controlled = true;

  auto* attack = commander->add_component<Engine::Core::AttackComponent>();
  ASSERT_NE(attack, nullptr);
  attack->can_melee = true;
  attack->can_ranged = true;
  attack->preferred_mode = Engine::Core::AttackComponent::CombatMode::Ranged;
  attack->current_mode = Engine::Core::AttackComponent::CombatMode::Ranged;
  attack->range = 8.0F;
  attack->damage = 12;

  CommanderControlController controller;
  ASSERT_TRUE(press_primary(controller, world, commander->get_id()));

  auto* combat_state = commander->get_component<Engine::Core::CombatStateComponent>();
  auto* action = commander->get_component<Engine::Core::RpgCommanderActionComponent>();
  ASSERT_NE(combat_state, nullptr);
  ASSERT_NE(action, nullptr);

  EXPECT_EQ(attack->current_mode, Engine::Core::AttackComponent::CombatMode::Ranged);
  EXPECT_EQ(combat_state->attack_family, Engine::Core::CombatAttackFamily::Bow);
  EXPECT_EQ(action->active_target_id, 0U)
      << "a drawn bow aims down the view; it does not acquire a melee target";
  EXPECT_EQ(action->combat_action_id,
            static_cast<std::uint8_t>(
                Game::Systems::CombatActions::CombatActionId::RpgBowShot));
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
  ASSERT_TRUE(press_primary(controller, world, commander->get_id()));

  auto* combat_state = commander->get_component<Engine::Core::CombatStateComponent>();
  ASSERT_NE(combat_state, nullptr);
  EXPECT_EQ(combat_state->attack_variant, 0U);
  EXPECT_TRUE(combat_state->finisher_attack);

  auto* action = commander->get_component<Engine::Core::RpgCommanderActionComponent>();
  ASSERT_NE(action, nullptr);
  EXPECT_EQ(action->combat_action_id,
            static_cast<std::uint8_t>(
                Game::Systems::CombatActions::CombatActionId::RpgSwordFinisher));
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
  action->action_running = true;

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
  action->action_running = false;
  action->action_completed = true;

  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 1.2F));

  EXPECT_EQ(commander_data->combo_step, 0);

  EXPECT_EQ(action->melee_attack_sequence, 1U);
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

TEST_F(CommanderControlControllerTest, MountedVanguardInputRequestsAuthoredCharge) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  auto* unit = commander->get_component<Engine::Core::UnitComponent>();
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::MountedKnight;
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);

  CommanderControlController controller;
  controller.request_vanguard_rush();
  Render::GL::Camera camera;
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.016F));

  auto* charge = commander->get_component<Engine::Core::MountedChargeComponent>();
  ASSERT_NE(charge, nullptr);
  EXPECT_TRUE(charge->intent_requested);
  EXPECT_EQ(charge->intent_source, Engine::Core::MountedChargeIntentSource::Player);
  EXPECT_EQ(charge->state, Engine::Core::MountedChargeState::Ready);
  EXPECT_FLOAT_EQ(transform->position.z, 0.0F);
  EXPECT_GT(commander->get_component<Engine::Core::MovementComponent>()->get_vz(),
            7.0F);
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

TEST_F(CommanderControlControllerTest, CommanderRpgPoolStaysInPlayableBand) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  auto* unit = commander->get_component<Engine::Core::UnitComponent>();
  ASSERT_NE(unit, nullptr);
  unit->max_health = 4200;
  unit->health = 4200;

  App::Core::CommanderModeCoordinator coordinator;
  CommanderControlController controller;
  Render::GL::Camera camera;
  auto const effects =
      coordinator.enter_commander_control_mode({.world = &world,
                                                .commander = commander,
                                                .commander_camera = &camera,
                                                .commander_control = &controller,
                                                .local_owner_id = 1});
  ASSERT_TRUE(effects.entered);

  auto* rpg = commander->get_component<Engine::Core::RpgHealthComponent>();
  ASSERT_NE(rpg, nullptr);
  EXPECT_TRUE(rpg->active);

  ASSERT_GT(rpg->incoming_damage_scale, 0.0F);
  auto const* commander_unit = commander->get_component<Engine::Core::UnitComponent>();
  ASSERT_NE(commander_unit, nullptr);
  float const effective_pool =
      static_cast<float>(commander_unit->max_health) / rpg->incoming_damage_scale;
  EXPECT_GE(effective_pool, 130.0F);
  EXPECT_LE(effective_pool, 220.0F);
}

TEST_F(CommanderControlControllerTest, ChaseLensStaysLevelWhileWalkingOverTerrain) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 64;
  map_def.grid.height = 64;
  map_def.grid.tile_size = 1.0F;
  Game::Map::TerrainService::instance().initialize(map_def);

  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, -12.0F);
  ASSERT_NE(commander, nullptr);
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);

  CommanderControlController controller;
  controller.set_view_yaw(0.0F);
  controller.input().forward = true;

  Game::Systems::TerrainAlignmentSystem terrain_alignment;
  Render::GL::Camera camera;
  std::vector<float> eye_y;
  std::vector<float> ground_y;
  constexpr int k_frames = 240;
  constexpr float k_dt = 1.0F / 60.0F;
  for (int frame = 0; frame < k_frames; ++frame) {
    terrain_alignment.update(&world, k_dt);
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
    eye_y.push_back(camera.get_position().y());
    ground_y.push_back(transform->position.y);
  }

  ASSERT_GT(transform->position.z, -11.0F) << "commander never walked";

  constexpr std::size_t k_settle = 30U;
  float max_eye_step = 0.0F;
  for (std::size_t i = k_settle; i < eye_y.size(); ++i) {
    max_eye_step = std::max(max_eye_step, std::abs(eye_y[i] - eye_y[i - 1]));
  }
  EXPECT_LT(max_eye_step, 0.006F);

  auto const eye_span = *std::max_element(eye_y.begin() + k_settle, eye_y.end()) -
                        *std::min_element(eye_y.begin() + k_settle, eye_y.end());
  auto const ground_span =
      *std::max_element(ground_y.begin() + k_settle, ground_y.end()) -
      *std::min_element(ground_y.begin() + k_settle, ground_y.end());

  EXPECT_LT(eye_span - ground_span, 0.05F);
}

TEST_F(CommanderControlControllerTest, ChaseLensFiltersGroundReliefButNotJumps) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);

  constexpr float k_dt = 1.0F / 60.0F;
  constexpr float k_rise = 0.5F;
  constexpr float k_jump_peak_height = 0.34F;

  CommanderControlController controller;
  controller.set_view_yaw(0.0F);
  Render::GL::Camera camera;
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
  const float resting_eye_y = camera.get_position().y();

  transform->position.y += k_rise;
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
  const float ground_first_frame = camera.get_position().y() - resting_eye_y;
  EXPECT_LT(ground_first_frame, k_rise * 0.25F);

  for (int frame = 0; frame < 120; ++frame) {
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
  }
  EXPECT_NEAR(camera.get_position().y() - resting_eye_y, k_rise, 0.02F);
  const float raised_eye_y = camera.get_position().y();

  controller.request_jump();
  float peak_jump_eye_y = raised_eye_y;
  for (int frame = 0; frame < 24; ++frame) {
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
    peak_jump_eye_y = std::max(peak_jump_eye_y, camera.get_position().y());
  }
  EXPECT_GT(peak_jump_eye_y - raised_eye_y, k_jump_peak_height * 0.5F);
}

TEST_F(CommanderControlControllerTest, ACommittedStrikeDoesNotFollowRawCameraYaw) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  auto* enemy = create_enemy(world, 0.0F, 1.6F);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(enemy, nullptr);

  auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander_data, nullptr);
  commander_data->fpv_controlled = true;
  auto* attack = commander->add_component<Engine::Core::AttackComponent>();
  ASSERT_NE(attack, nullptr);
  attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);

  CommanderControlController controller;
  controller.set_view_yaw(0.0F);
  constexpr float k_dt = 1.0F / 60.0F;

  ASSERT_TRUE(press_primary(controller, world, commander->get_id()));
  auto* action = commander->get_component<Engine::Core::RpgCommanderActionComponent>();
  ASSERT_NE(action, nullptr);
  ASSERT_TRUE(action->action_running);

  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      static_cast<Game::Systems::CombatActions::CombatActionId>(
          action->combat_action_id));
  ASSERT_NE(definition, nullptr);

  action->normalized_action_time =
      Game::Systems::CombatActions::action_event_normalized_time(
          *definition,
          Game::Systems::CombatActions::CombatActionEventType::WeaponTraceStart,
          0.4F) +
      0.05F;
  ASSERT_FLOAT_EQ(Game::Systems::CombatActions::melee_interruption_at(
                      *definition, action->normalized_action_time)
                      .redirect_authority,
                  0.35F);

  float const committed_yaw = transform->rotation.y;
  controller.set_view_yaw(150.0F);
  ASSERT_TRUE(controller.update_simulation(world, commander->get_id(), 1, k_dt));

  float const turned = std::abs(controller.view_yaw() - transform->rotation.y);
  EXPECT_GT(turned, 100.0F)
      << "a committed strike must not snap the body 150 degrees onto the camera";
  EXPECT_LT(std::abs(transform->rotation.y - committed_yaw), 10.0F)
      << "the body may steer inside its authored authority, not beyond it";
}

TEST_F(CommanderControlControllerTest, IsolatedSwingsDoNotReplayTheSameClip) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  auto* enemy = create_enemy(world, 0.0F, 1.6F);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(enemy, nullptr);

  auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander_data, nullptr);
  commander_data->fpv_controlled = true;
  auto* attack = commander->add_component<Engine::Core::AttackComponent>();
  ASSERT_NE(attack, nullptr);
  attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;

  CommanderControlController controller;
  controller.set_view_yaw(0.0F);
  Render::GL::Camera camera;

  std::vector<std::uint8_t> swing_clips;
  for (int swing = 0; swing < 3; ++swing) {
    controller.primary_action_down();
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.016F));
    controller.primary_action_up();

    auto* action =
        commander->get_component<Engine::Core::RpgCommanderActionComponent>();
    ASSERT_NE(action, nullptr);
    swing_clips.push_back(action->combat_action_id);

    action->action_running = false;
    action->action_completed = true;
    if (auto* combat_state =
            commander->get_component<Engine::Core::CombatStateComponent>()) {
      combat_state->animation_state = Engine::Core::CombatAnimationState::Idle;
      combat_state->state_duration = 0.0F;
      combat_state->state_time = 0.0F;
    }
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 1.5F));
  }

  ASSERT_EQ(swing_clips.size(), 3U);
  EXPECT_NE(swing_clips[0], swing_clips[1]);
  EXPECT_NE(swing_clips[1], swing_clips[2]);
}

TEST_F(CommanderControlControllerTest,
       HoldingPrimaryChainsAtRecoveryAndReleaseStopsTheCombo) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(create_enemy(world, 0.0F, 1.6F), nullptr);

  auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander_data, nullptr);
  commander_data->fpv_controlled = true;
  auto* attack = commander->add_component<Engine::Core::AttackComponent>();
  ASSERT_NE(attack, nullptr);
  attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;

  CommanderControlController controller;
  controller.set_view_yaw(0.0F);
  Render::GL::Camera camera;
  controller.primary_action_down();
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.016F));

  auto* action = commander->get_component<Engine::Core::RpgCommanderActionComponent>();
  auto* intents = commander->get_component<Engine::Core::CombatIntentQueueComponent>();
  ASSERT_NE(action, nullptr);
  ASSERT_NE(intents, nullptr);
  auto const first_id = static_cast<Game::Systems::CombatActions::CombatActionId>(
      action->combat_action_id);
  auto const* first =
      Game::Systems::CombatActions::find_combat_action_definition(first_id);
  ASSERT_NE(first, nullptr);
  ASSERT_EQ(intents->accepted_intents, 1U);

  float const recovery = Game::Systems::CombatActions::action_event_normalized_time(
      *first,
      Game::Systems::CombatActions::CombatActionEventType::RecoveryStart,
      0.75F);
  (void)Game::Systems::CombatActions::advance_combat_action_events(
      *action, action->action_duration * (recovery + 0.01F), *first);
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.016F));

  EXPECT_EQ(intents->accepted_intents, 2U);
  auto const second_id = static_cast<Game::Systems::CombatActions::CombatActionId>(
      action->combat_action_id);
  EXPECT_EQ(second_id,
            Game::Systems::CombatActions::resolve_commander_action(
                first_id,
                Engine::Core::CommanderCombatIntentType::Light,
                Game::Systems::CombatActions::WeaponFamily::Sword,
                false,
                false));

  controller.primary_action_up();
  auto const* second =
      Game::Systems::CombatActions::find_combat_action_definition(second_id);
  ASSERT_NE(second, nullptr);
  float const second_recovery =
      Game::Systems::CombatActions::action_event_normalized_time(
          *second,
          Game::Systems::CombatActions::CombatActionEventType::RecoveryStart,
          0.75F);
  (void)Game::Systems::CombatActions::advance_combat_action_events(
      *action, action->action_duration * (second_recovery + 0.01F), *second);
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.016F));
  EXPECT_EQ(intents->accepted_intents, 2U);
}

TEST_F(CommanderControlControllerTest, MiddleMouseHeavyIsLongerAndHitsHarderThanLight) {
  using Game::Systems::CombatActions::CombatActionId;
  auto const* light = Game::Systems::CombatActions::find_combat_action_definition(
      CombatActionId::RpgSwordSlashLeft);
  auto const* heavy = Game::Systems::CombatActions::find_combat_action_definition(
      CombatActionId::RpgSwordOverhead);
  ASSERT_NE(light, nullptr);
  ASSERT_NE(heavy, nullptr);
  EXPECT_GT(heavy->duration_seconds, light->duration_seconds);
  EXPECT_GT(heavy->damage.base_multiplier, light->damage.base_multiplier);
  EXPECT_GT(heavy->damage.posture_damage, light->damage.posture_damage);
}

TEST_F(CommanderControlControllerTest, StrikeCarriesTheCommanderIntoATargetOutOfReach) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);

  auto* enemy = create_enemy(world, 0.0F, 2.6F);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(enemy, nullptr);

  auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  auto* enemy_unit = enemy->get_component<Engine::Core::UnitComponent>();
  ASSERT_NE(commander_data, nullptr);
  ASSERT_NE(transform, nullptr);
  ASSERT_NE(enemy_unit, nullptr);
  commander_data->fpv_controlled = true;
  enemy_unit->render_individuals_per_unit_override = 1;
  auto* attack = commander->add_component<Engine::Core::AttackComponent>();
  ASSERT_NE(attack, nullptr);
  attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;

  CommanderControlController controller;
  controller.set_view_yaw(0.0F);
  Render::GL::Camera camera;

  controller.primary_action_down();
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.016F));
  controller.primary_action_up();

  auto* action = commander->get_component<Engine::Core::RpgCommanderActionComponent>();
  ASSERT_NE(action, nullptr);
  ASSERT_TRUE(action->action_running);

  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      static_cast<Game::Systems::CombatActions::CombatActionId>(
          action->combat_action_id));
  ASSERT_NE(definition, nullptr);

  const float start_z = transform->position.z;
  constexpr float k_dt = 0.016F;
  for (int frame = 0; frame < 40; ++frame) {

    (void)Game::Systems::CombatActions::advance_combat_action_events(
        *action, k_dt, *definition);
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
  }
  const float travelled = transform->position.z - start_z;

  EXPECT_GT(travelled, 0.25F);

  EXPECT_LT(transform->position.z, 2.2F);
}

} // namespace

TEST_F(CommanderControlControllerTest, FrameIntentMirrorsHeldCommanderInput) {
  CommanderControlController controller;
  controller.set_view_yaw(40.0F);
  controller.set_view_pitch(-5.0F);

  const auto idle = controller.sample_frame_intent(nullptr);
  EXPECT_FALSE(idle.has_move());
  EXPECT_FALSE(idle.guard);
  EXPECT_FALSE(idle.attack_held);
  EXPECT_FLOAT_EQ(idle.view_yaw, 40.0F);
  EXPECT_FLOAT_EQ(idle.view_pitch, -5.0F);

  controller.key_down(Qt::Key_W);
  controller.key_down(Qt::Key_D);
  controller.key_down(Qt::Key_Shift);
  controller.primary_action_down();
  controller.secondary_action_down();
  controller.request_dodge();
  controller.request_jump();

  const auto held = controller.sample_frame_intent(nullptr);
  EXPECT_FLOAT_EQ(held.move.x(), 1.0F);
  EXPECT_FLOAT_EQ(held.move.y(), 1.0F);
  EXPECT_TRUE(held.run);
  EXPECT_TRUE(held.attack_held);
  EXPECT_TRUE(held.guard);
  EXPECT_TRUE(held.dodge_pressed);
  EXPECT_TRUE(held.jump_pressed);
  EXPECT_EQ(held.frame_index, idle.frame_index + 1);
}

TEST_F(CommanderControlControllerTest, FrameIntentReportsMouseLookDeltaForTheFrame) {
  CommanderControlController controller;
  controller.set_view_yaw(0.0F);
  controller.set_view_pitch(0.0F);

  static_cast<void>(controller.sample_frame_intent(nullptr));
  controller.mouse_move(20.0, -10.0);
  const auto moved = controller.sample_frame_intent(nullptr);

  EXPECT_TRUE(moved.has_look_delta());
  EXPECT_GT(moved.look_delta.x(), 0.0F);
  EXPECT_GT(moved.look_delta.y(), 0.0F);
  EXPECT_FLOAT_EQ(moved.view_yaw, controller.view_yaw());
  EXPECT_FLOAT_EQ(moved.view_pitch, controller.view_pitch());

  const auto settled = controller.sample_frame_intent(nullptr);
  EXPECT_FALSE(settled.has_look_delta());
}

TEST_F(CommanderControlControllerTest, FrameIntentWrapsAroundTheYawSeam) {
  CommanderControlController controller;
  controller.set_view_yaw(359.0F);
  static_cast<void>(controller.sample_frame_intent(nullptr));

  controller.mouse_move(20.0, 0.0);
  const auto wrapped = controller.sample_frame_intent(nullptr);
  EXPECT_GT(wrapped.look_delta.x(), 0.0F);
  EXPECT_LT(wrapped.look_delta.x(), 90.0F);
}
