#include <cmath>
#include <gtest/gtest.h>
#include <vector>

#include "app/commander/commander_control_controller.h"
#include "game/core/component_commander.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/ford_system.h"
#include "game/systems/navigation/nav_grid.h"
#include "game/systems/navigation/pathfinding.h"
#include "game/systems/navigation/terrain_alignment_system.h"
#include "scene/camera.h"

namespace {

struct MotorRun {
  QVector3D final_position;
  float settled_speed{0.0F};
  float peak_speed{0.0F};
  int zero_speed_pulses{0};
  int longest_zero_run{0};
};

class CommanderMotorTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::NavGrid::initialize(96, 96);
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
    entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    entity->add_component<Engine::Core::MovementComponent>();
    auto* commander = entity->add_component<Engine::Core::CommanderComponent>();
    if (unit == nullptr || commander == nullptr) {
      return nullptr;
    }
    unit->health = 100;
    unit->max_health = 100;
    unit->owner_id = 1;
    unit->speed = 3.0F;
    unit->spawn_type = Game::Units::SpawnType::Swordsman;
    commander->fpv_controlled = true;
    return entity;
  }

  static auto drive(float tick_seconds,
                    float seconds,
                    int forward,
                    int right,
                    float view_yaw = 0.0F) -> MotorRun {
    Engine::Core::World world;
    auto* commander = create_commander(world, 0.0F, 0.0F);
    EXPECT_NE(commander, nullptr);
    if (commander == nullptr) {
      return {};
    }
    auto* transform = commander->get_component<Engine::Core::TransformComponent>();

    CommanderControlController controller;
    controller.set_view_yaw(view_yaw);
    controller.set_presentation_trace_enabled(true);
    Render::GL::Camera camera;
    const auto commander_id = commander->get_id();

    auto& input = controller.input();
    input.forward = forward > 0;
    input.backward = forward < 0;
    input.right = right > 0;
    input.left = right < 0;

    MotorRun run;
    std::vector<float> speeds;
    int zero_run = 0;
    const auto ticks = static_cast<int>(std::lround(seconds / tick_seconds));
    for (int tick = 0; tick < ticks; ++tick) {
      QVector3D const before(
          transform->position.x, transform->position.y, transform->position.z);
      EXPECT_TRUE(controller.update(world, commander_id, 1, camera, tick_seconds));
      QVector3D const after(
          transform->position.x, transform->position.y, transform->position.z);
      float const speed = tick_seconds > 0.0F ? std::hypot(after.x() - before.x(),
                                                           after.z() - before.z()) /
                                                    tick_seconds
                                              : 0.0F;
      speeds.push_back(speed);
      run.peak_speed = std::max(run.peak_speed, speed);
      if (speed < 0.05F) {
        ++zero_run;
      } else {
        if (zero_run > 0) {
          ++run.zero_speed_pulses;
          run.longest_zero_run = std::max(run.longest_zero_run, zero_run);
        }
        zero_run = 0;
      }
    }

    run.final_position =
        QVector3D(transform->position.x, transform->position.y, transform->position.z);
    float total = 0.0F;
    int counted = 0;
    for (std::size_t index = speeds.size() / 2; index < speeds.size(); ++index) {
      total += speeds[index];
      ++counted;
    }
    run.settled_speed = counted > 0 ? total / static_cast<float>(counted) : 0.0F;
    return run;
  }
};

TEST_F(CommanderMotorTest, DiagonalTravelMatchesStraightTravel) {
  const auto straight = drive(1.0F / 60.0F, 3.0F, 1, 0);
  const auto diagonal = drive(1.0F / 60.0F, 3.0F, 1, 1);

  ASSERT_GT(straight.settled_speed, 0.5F);
  float const error = std::abs(diagonal.settled_speed - straight.settled_speed) /
                      straight.settled_speed;
  EXPECT_LE(error, 0.01F) << "diagonal settled at " << diagonal.settled_speed
                          << " m/s against " << straight.settled_speed
                          << " m/s straight ahead";
}

TEST_F(CommanderMotorTest, SettledSpeedMatchesTheConfiguredSpeed) {
  const auto run = drive(1.0F / 60.0F, 3.0F, 1, 0);

  constexpr float k_configured = 3.0F * 1.25F;
  float const error = std::abs(run.settled_speed - k_configured) / k_configured;
  EXPECT_LE(error, 0.02F) << "settled at " << run.settled_speed << " m/s against a "
                          << k_configured << " m/s contract";
}

TEST_F(CommanderMotorTest, TheSamePathIsWalkedAtEverySimulationRate) {
  const auto reference = drive(1.0F / 60.0F, 3.0F, 1, 0);

  for (float const hz : {30.0F, 120.0F}) {
    const auto run = drive(1.0F / hz, 3.0F, 1, 0);
    EXPECT_LE((run.final_position - reference.final_position).length(), 0.03F)
        << hz << " Hz ended at " << run.final_position.z() << " against "
        << reference.final_position.z() << " at 60 Hz";
  }
}

TEST_F(CommanderMotorTest, AWallDoesNotResetTheWholeMotor) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);

  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  for (int offset = -8; offset <= 8; ++offset) {
    auto const blocked =
        Game::Systems::NavGrid::world_to_grid(static_cast<float>(offset), 3.0F);
    pathfinder->set_obstacle(blocked.x, blocked.y, true);
  }

  CommanderControlController controller;
  controller.set_presentation_trace_enabled(true);
  Render::GL::Camera camera;
  constexpr float k_dt = 1.0F / 60.0F;

  controller.input().forward = true;
  controller.input().right = true;

  int sliding_frames = 0;
  float travelled_x = 0.0F;
  for (int frame = 0; frame < 240; ++frame) {
    float const before_x = transform->position.x;
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
    auto const& motor = controller.presentation_trace().motor;
    if (motor.blocked || motor.slid) {
      sliding_frames += motor.slid ? 1 : 0;
      travelled_x += std::abs(transform->position.x - before_x);
    }
  }

  EXPECT_GT(sliding_frames, 30)
      << "with forward blocked and strafe free the commander must keep sliding "
         "along the wall, not stop dead";
  EXPECT_GT(travelled_x, 1.0F) << "the commander covered only " << travelled_x
                               << " m sideways while pressed against the wall";
}

TEST_F(CommanderMotorTest, WalkingStraightIntoABoulderGlancesAroundIt) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);

  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  auto const boulder = Game::Systems::NavGrid::world_to_grid(0.0F, 2.0F);
  pathfinder->set_obstacle(boulder.x, boulder.y, true);

  CommanderControlController controller;
  Render::GL::Camera camera;
  controller.input().forward = true;
  for (int frame = 0; frame < 180; ++frame) {
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 1.0F / 60.0F));
  }

  EXPECT_GT(transform->position.z, 3.0F)
      << "walking straight at a one-cell boulder stopped the commander dead at z = "
      << transform->position.z << "; he must glance off it";
}

TEST_F(CommanderMotorTest, NoDisplacementTunnelsThroughAThinWall) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);

  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  for (int offset = -12; offset <= 12; ++offset) {
    auto const blocked =
        Game::Systems::NavGrid::world_to_grid(static_cast<float>(offset), 2.0F);
    pathfinder->set_obstacle(blocked.x, blocked.y, true);
  }

  CommanderControlController controller;
  Render::GL::Camera camera;

  controller.input().forward = true;
  controller.request_dodge();

  constexpr float k_long_frame = 0.25F;
  for (int frame = 0; frame < 8; ++frame) {
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_long_frame));
    EXPECT_LT(transform->position.z, 2.0F)
        << "a long frame carried the commander through the wall at z = 2 to "
        << transform->position.z;
  }
}

TEST_F(CommanderMotorTest, ADodgeDoesNotTunnelThroughAThinWall) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);

  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  for (int offset = -12; offset <= 12; ++offset) {
    auto const blocked =
        Game::Systems::NavGrid::world_to_grid(static_cast<float>(offset), 2.0F);
    pathfinder->set_obstacle(blocked.x, blocked.y, true);
  }

  CommanderControlController controller;
  Render::GL::Camera camera;
  constexpr float k_dt = 1.0F / 60.0F;

  controller.input().forward = true;
  for (int frame = 0; frame < 30; ++frame) {
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
  }

  controller.request_dodge();
  for (int frame = 0; frame < 60; ++frame) {
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
    ASSERT_LT(transform->position.z, 2.0F)
        << "the dodge roll crossed the wall at z = 2 to " << transform->position.z;
  }

  transform->position.z = 1.2F;
  controller.request_dodge();
  for (int frame = 0; frame < 6; ++frame) {
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.25F));
    ASSERT_LT(transform->position.z, 2.0F)
        << "a dodge inside one long frame crossed the wall at z = 2 to "
        << transform->position.z;
  }
}

TEST_F(CommanderMotorTest, LookingAroundWhileIdleDoesNotSpinTheBody) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);

  CommanderControlController controller;
  Render::GL::Camera camera;
  constexpr float k_dt = 1.0F / 60.0F;

  controller.set_view_yaw(0.0F);
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
  float const resting_yaw = transform->rotation.y;

  for (int frame = 0; frame < 10; ++frame) {
    controller.mouse_move(10.0, 0.0);
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
  }
  EXPECT_NEAR(transform->rotation.y, resting_yaw, 0.01F)
      << "a glance of " << controller.view_yaw()
      << " degrees moved the body; the commander should stand still and look";

  for (int frame = 0; frame < 20; ++frame) {
    controller.mouse_move(20.0, 0.0);
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
  }

  EXPECT_GT(std::abs(controller.view_yaw() - resting_yaw), 80.0F);
  EXPECT_NEAR(transform->rotation.y, resting_yaw, 0.01F)
      << "free camera orbit must not rotate the idle body, even after a wide sweep";

  for (int frame = 0; frame < 60; ++frame) {
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
  }
  EXPECT_NEAR(transform->rotation.y, resting_yaw, 0.01F)
      << "idle facing must remain planted after the camera stops";
}

TEST_F(CommanderMotorTest, AnAttackFacesTheViewImmediately) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);

  CommanderControlController controller;
  Render::GL::Camera camera;
  constexpr float k_dt = 1.0F / 60.0F;

  controller.set_view_yaw(0.0F);
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));

  controller.set_view_yaw(85.0F);
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
  ASSERT_NEAR(transform->rotation.y, 0.0F, 0.01F)
      << "the idle body keeps its facing until the attack begins";

  controller.primary_action_down();
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
  EXPECT_NEAR(transform->rotation.y, 85.0F, 0.01F)
      << "a swing has to land where the player is looking, not where the body "
         "happened to be resting";
}

TEST_F(CommanderMotorTest, TurningAwayFromAWallStartsAtTheAccelerationLimit) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);

  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  for (int offset = -12; offset <= 12; ++offset) {
    auto const blocked =
        Game::Systems::NavGrid::world_to_grid(static_cast<float>(offset), 3.0F);
    pathfinder->set_obstacle(blocked.x, blocked.y, true);
  }

  CommanderControlController controller;
  controller.set_presentation_trace_enabled(true);
  Render::GL::Camera camera;
  constexpr float k_dt = 1.0F / 60.0F;

  controller.input().forward = true;
  int contact_frames = 0;
  for (int frame = 0; frame < 240 && contact_frames < 3; ++frame) {
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
    if (controller.presentation_trace().motor.blocked) {
      ++contact_frames;
    }
  }
  ASSERT_EQ(contact_frames, 3) << "the commander never reached the wall";
  float const pinned_z = transform->position.z;

  controller.input().forward = false;
  controller.input().right = true;
  float const before_x = transform->position.x;
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
  float const first_step = std::abs(transform->position.x - before_x) / k_dt;

  ASSERT_NEAR(transform->position.z, pinned_z, 0.05F)
      << "the commander should still be against the wall when he turns away";

  Engine::Core::World open_world;
  auto* open_commander = create_commander(open_world, 0.0F, -20.0F);
  ASSERT_NE(open_commander, nullptr);
  auto* open_transform =
      open_commander->get_component<Engine::Core::TransformComponent>();
  CommanderControlController open_controller;
  Render::GL::Camera open_camera;
  open_controller.input().right = true;
  float const open_before = open_transform->position.x;
  ASSERT_TRUE(open_controller.update(
      open_world, open_commander->get_id(), 1, open_camera, k_dt));
  float const standing_start =
      std::abs(open_transform->position.x - open_before) / k_dt;

  EXPECT_NEAR(first_step, standing_start, 0.02F)
      << "turning away from a wall must start at the same acceleration limit as "
         "a standing start: "
      << first_step << " m/s against " << standing_start << " m/s";
  EXPECT_NEAR(standing_start, k_commander_ground_acceleration_mps2 / 60.0F, 0.02F)
      << "a standing start must spend exactly one tick of the documented "
         "acceleration budget";
}

TEST_F(CommanderMotorTest, AReversalPassesThroughZeroAtTheAccelerationLimit) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, -20.0F);
  ASSERT_NE(commander, nullptr);
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);

  CommanderControlController controller;
  controller.set_presentation_trace_enabled(true);
  Render::GL::Camera camera;
  constexpr float k_dt = 1.0F / 60.0F;

  controller.input().forward = true;
  controller.input().run = true;
  for (int frame = 0; frame < 150; ++frame) {
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
  }
  float const sprint_speed = controller.presentation_trace().motor.actual_velocity.z();
  ASSERT_GT(sprint_speed, 3.0F) << "the commander has to be sprinting first";

  controller.input().forward = false;
  controller.input().backward = true;

  float previous = sprint_speed;
  float worst_step = 0.0F;
  float most_negative = 0.0F;
  bool crossed_zero_continuously = false;
  for (int frame = 0; frame < 90; ++frame) {
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
    float const now = controller.presentation_trace().motor.actual_velocity.z();
    worst_step = std::max(worst_step, std::abs(now - previous));
    if (previous > 0.0F && now <= 0.0F && std::abs(now - previous) < 1.0F) {
      crossed_zero_continuously = true;
    }
    most_negative = std::min(most_negative, now);
    previous = now;
  }

  EXPECT_TRUE(crossed_zero_continuously)
      << "the reversal has to pass through zero rather than jump across it";
  EXPECT_LT(most_negative, -1.0F) << "the commander must end up backpedalling";

  float const budget = (k_commander_ground_deceleration_mps2 * k_dt) + 0.05F;
  EXPECT_LT(worst_step, budget) << "largest single-tick velocity change " << worst_step
                                << " m/s against a budget of " << budget << " m/s";
}

TEST_F(CommanderMotorTest, TheAccelerationLimitDoesNotDependOnTheTickRate) {
  auto run = [](float tick_seconds, int ticks) {
    Engine::Core::World world;
    auto* commander = create_commander(world, 0.0F, -20.0F);
    EXPECT_NE(commander, nullptr);
    CommanderControlController controller;
    controller.set_presentation_trace_enabled(true);
    Render::GL::Camera camera;
    controller.input().forward = true;
    for (int frame = 0; frame < ticks; ++frame) {
      EXPECT_TRUE(
          controller.update(world, commander->get_id(), 1, camera, tick_seconds));
    }
    return controller.presentation_trace().motor.actual_velocity.length();
  };

  float const at_60 = run(1.0F / 60.0F, 6);
  float const at_120 = run(1.0F / 120.0F, 12);
  float const at_30 = run(1.0F / 30.0F, 3);

  EXPECT_NEAR(at_60, at_120, 0.05F)
      << "the same tenth of a second must build the same speed at 60 and 120 Hz";
  EXPECT_NEAR(at_60, at_30, 0.05F)
      << "the same tenth of a second must build the same speed at 60 and 30 Hz";
}

TEST_F(CommanderMotorTest, CrowdSeparationIsBoundedAndRepeatable) {
  auto run_once = [](std::vector<QVector3D>* path) {
    Engine::Core::World world;
    auto* commander = create_commander(world, 0.0F, 0.0F);
    EXPECT_NE(commander, nullptr);
    if (commander == nullptr) {
      return;
    }
    auto* transform = commander->get_component<Engine::Core::TransformComponent>();

    for (int index = 0; index < 8; ++index) {
      auto* crowd = world.create_entity();
      if (crowd == nullptr) {
        continue;
      }
      float const angle = static_cast<float>(index) * 0.785F;
      crowd->add_component<Engine::Core::TransformComponent>(
          std::cos(angle) * 0.35F, 0.0F, std::sin(angle) * 0.35F);
      auto* unit = crowd->add_component<Engine::Core::UnitComponent>();
      if (unit != nullptr) {
        unit->health = 100;
        unit->max_health = 100;
        unit->owner_id = 1;
        unit->speed = 3.0F;
        unit->spawn_type = Game::Units::SpawnType::Swordsman;
      }
    }

    CommanderControlController controller;
    Render::GL::Camera camera;
    constexpr float k_dt = 1.0F / 60.0F;
    for (int frame = 0; frame < 60; ++frame) {
      EXPECT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
      path->emplace_back(
          transform->position.x, transform->position.y, transform->position.z);
    }
  };

  std::vector<QVector3D> first;
  std::vector<QVector3D> second;
  run_once(&first);
  run_once(&second);

  ASSERT_EQ(first.size(), second.size());
  ASSERT_FALSE(first.empty());
  for (std::size_t index = 0; index < first.size(); ++index) {
    EXPECT_EQ(first[index], second[index])
        << "separation frame " << index
        << " differs between two identical runs; the solve is not deterministic";
  }

  constexpr float k_dt = 1.0F / 60.0F;
  constexpr float k_max_push_per_second = 2.4F;
  float largest = 0.0F;
  for (std::size_t index = 1; index < first.size(); ++index) {
    largest = std::max(largest, (first[index] - first[index - 1]).length());
  }
  EXPECT_LE(largest, (k_max_push_per_second * k_dt) + 1.0e-4F)
      << "a crowd shoved the commander " << largest << " m in one tick against a "
      << (k_max_push_per_second * k_dt) << " m budget";
}

// Exercise the locomotion owner directly: these checks must not depend on camera
// smoothing, renderer timing or AI target selection.
auto locomotion_handles(Engine::Core::Entity& entity) -> App::Core::CommanderHandles {
  return {entity,
          *entity.get_component<Engine::Core::TransformComponent>(),
          *entity.get_component<Engine::Core::UnitComponent>(),
          entity.get_component<Engine::Core::CommanderComponent>(),
          entity.get_component<Engine::Core::MovementComponent>()};
}

TEST_F(CommanderMotorTest, LockingOnPreservesLeftAndRightAtEveryHeading) {
  Engine::Core::TransformComponent transform;
  for (float yaw : {0.0F, 45.0F, 90.0F, 180.0F, 270.0F, 359.0F}) {
    auto const front =
        App::Core::CommanderLocomotion::move_basis(yaw, 1, 0, transform, std::nullopt)
            .forward;
    for (int forward : {-1, 0, 1}) {
      for (int right : {-1, 0, 1}) {
        auto const free = App::Core::CommanderLocomotion::move_basis(
            yaw, forward, right, transform, std::nullopt);
        auto const locked = App::Core::CommanderLocomotion::move_basis(
            yaw, forward, right, transform, front * 4.0F);
        EXPECT_LT((free.move - locked.move).length(), 1.0e-5F)
            << "yaw=" << yaw << " forward=" << forward << " right=" << right;
      }
    }
  }
}

TEST_F(CommanderMotorTest, JumpRequiresItsFullStaminaCostAndConsumesOnlyOnce) {
  Engine::Core::World world;
  auto* entity = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(entity, nullptr);
  auto body = locomotion_handles(*entity);
  auto* stamina = entity->add_component<Engine::Core::StaminaComponent>();
  constexpr float cost = Engine::Core::CombatStateComponent::k_stamina_cost_jump;
  for (float available : {0.0F, cost - 0.01F, cost, cost + 1.0F}) {
    App::Core::CommanderLocomotion locomotion;
    stamina->stamina = available;
    stamina->regen_delay_remaining = 0.0F;
    auto const result =
        locomotion.advance_jump(body, {.jump_pressed = true, .dt = 1.0F / 60.0F});
    bool const accepted = available >= cost;
    EXPECT_EQ(result.active, accepted);
    EXPECT_EQ(result.edge,
              accepted ? App::Core::EdgeOutcome::Consumed
                       : App::Core::EdgeOutcome::Refused);
    EXPECT_FLOAT_EQ(stamina->stamina, accepted ? available - cost : available);
    if (!accepted) {
      EXPECT_FLOAT_EQ(stamina->regen_delay_remaining, 0.0F);
      EXPECT_FALSE(body.commander_data->jump_active);
    } else {
      auto const again =
          locomotion.advance_jump(body, {.jump_pressed = true, .dt = 1.0F / 60.0F});
      EXPECT_EQ(again.edge, App::Core::EdgeOutcome::Refused);
      EXPECT_FLOAT_EQ(stamina->stamina, available - cost);
    }
    body.commander_data->jump_active = false;
  }
}

TEST_F(CommanderMotorTest, ExhaustedDodgeCannotGrantInvulnerabilityOrCancelAnAction) {
  Engine::Core::World world;
  auto* entity = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(entity, nullptr);
  auto body = locomotion_handles(*entity);
  auto* stamina = entity->add_component<Engine::Core::StaminaComponent>();
  auto* health = entity->add_component<Engine::Core::RpgHealthComponent>();
  auto* action = entity->add_component<Engine::Core::RpgCommanderActionComponent>();
  constexpr float cost = Engine::Core::CombatStateComponent::k_stamina_cost_dodge;
  for (float available : {0.0F, cost - 0.01F, cost, cost + 1.0F}) {
    App::Core::CommanderLocomotion locomotion;
    stamina->stamina = available;
    stamina->regen_delay_remaining = 0.0F;
    health->dodge_grace_remaining = 0.0F;
    action->action_running = true;
    auto const edge = locomotion.try_start_dodge(body, {.pressed = true}, nullptr);
    bool const accepted = available >= cost;
    EXPECT_EQ(edge,
              accepted ? App::Core::EdgeOutcome::Consumed
                       : App::Core::EdgeOutcome::Refused);
    EXPECT_FLOAT_EQ(stamina->stamina, accepted ? available - cost : available);
    if (!accepted) {
      EXPECT_EQ(locomotion.dodge_state(), DodgeState::None);
      EXPECT_FLOAT_EQ(health->dodge_grace_remaining, 0.0F);
      EXPECT_FLOAT_EQ(stamina->regen_delay_remaining, 0.0F);
      EXPECT_TRUE(action->action_running);
    } else {
      EXPECT_GT(health->dodge_grace_remaining, 0.0F);
      EXPECT_EQ(locomotion.try_start_dodge(body, {.pressed = true}, nullptr),
                App::Core::EdgeOutcome::Refused);
      EXPECT_FLOAT_EQ(stamina->stamina, available - cost);
    }
  }
}

TEST_F(CommanderMotorTest, AStaggeredBodyCannotRollOrJumpOutOfTheHit) {
  Engine::Core::World world;
  auto* entity = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(entity, nullptr);
  auto body = locomotion_handles(*entity);
  auto* stamina = entity->add_component<Engine::Core::StaminaComponent>();
  auto* health = entity->add_component<Engine::Core::RpgHealthComponent>();
  float const before = stamina->stamina;

  App::Core::CommanderLocomotion locomotion;
  EXPECT_EQ(
      locomotion.try_start_dodge(body, {.pressed = true, .staggered = true}, nullptr),
      App::Core::EdgeOutcome::Refused);
  EXPECT_EQ(locomotion.dodge_state(), DodgeState::None);
  EXPECT_FLOAT_EQ(health->dodge_grace_remaining, 0.0F)
      << "a stagger must not be escaped into invulnerability";

  auto const jump = locomotion.advance_jump(
      body, {.jump_pressed = true, .staggered = true, .dt = 1.0F / 60.0F});
  EXPECT_EQ(jump.edge, App::Core::EdgeOutcome::Refused);
  EXPECT_FALSE(jump.active);
  EXPECT_FLOAT_EQ(stamina->stamina, before) << "a refused action costs nothing";
}

TEST_F(CommanderMotorTest, DodgePublishesAcceptedVelocityWhenItHitsAWall) {
  Engine::Core::World world;
  auto* entity = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(entity, nullptr);
  auto body = locomotion_handles(*entity);
  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  for (int offset = -12; offset <= 12; ++offset) {
    auto const cell =
        Game::Systems::NavGrid::world_to_grid(static_cast<float>(offset), 1.0F);
    pathfinder->set_obstacle(cell.x, cell.y, true);
  }
  App::Core::CommanderLocomotion locomotion;
  App::Core::CommanderMotor motor;
  motor.bind_world(&world);
  ASSERT_EQ(locomotion.try_start_dodge(body, {.pressed = true}, nullptr),
            App::Core::EdgeOutcome::Consumed);
  bool hit_wall = false;
  constexpr float dt = 1.0F / 60.0F;
  for (int tick = 0; tick < 20; ++tick) {
    QVector3D const before(body.transform.position.x, 0.0F, body.transform.position.z);
    auto const report = locomotion.advance_motion(body, motor, {.dt = dt});
    QVector3D const after(body.transform.position.x, 0.0F, body.transform.position.z);
    auto const accepted = (after - before) / dt;
    if (report.dodge_pose_active) {
      EXPECT_NEAR(report.actual_speed, accepted.length(), 1.0e-4F);
      EXPECT_NEAR(locomotion.planar_speed(), accepted.length(), 1.0e-4F);
      EXPECT_NEAR(body.movement->get_vx(), accepted.x(), 1.0e-4F);
      EXPECT_NEAR(body.movement->get_vz(), accepted.z(), 1.0e-4F);
      hit_wall |= report.blocked;
    }
  }
  EXPECT_TRUE(hit_wall);
}

void block_row(float z) {
  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  for (int offset = -12; offset <= 12; ++offset) {
    auto const cell =
        Game::Systems::NavGrid::world_to_grid(static_cast<float>(offset), z);
    pathfinder->set_obstacle(cell.x, cell.y, true);
  }
}

// A 0.72 m jump clears no obstacle the navigation grid knows about (the
// lowest, a boulder, stands 1.2 m). The airborne body therefore collides like
// the grounded one, and an ordinary landing is never repaired by a snap-back.
TEST_F(CommanderMotorTest, AJumpIntoAWallStaysOutsideItAndNeverSnapsBack) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  auto* data = commander->get_component<Engine::Core::CommanderComponent>();
  block_row(2.0F);

  CommanderControlController controller;
  controller.set_presentation_trace_enabled(true);
  Render::GL::Camera camera;
  constexpr float dt = 1.0F / 60.0F;
  controller.input().forward = true;
  for (int tick = 0; tick < 12; ++tick) {
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, dt));
  }

  controller.request_jump();
  int airborne_ticks = 0;
  for (int tick = 0; tick < 90; ++tick) {
    QVector3D const before(transform->position.x, 0.0F, transform->position.z);
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, dt));
    QVector3D const after(transform->position.x, 0.0F, transform->position.z);
    airborne_ticks += data->jump_active ? 1 : 0;
    auto const& motor = controller.presentation_trace().motor;
    EXPECT_TRUE(
        App::Core::CommanderMotor::is_walkable_at(*commander, after.x(), after.z()))
        << "tick " << tick << " left the body inside the wall at z = " << after.z();
    EXPECT_LT(after.z(), 2.0F) << "tick " << tick;
    EXPECT_FLOAT_EQ(motor.snap_back_distance, 0.0F) << "tick " << tick;
    EXPECT_NE(motor.displacement_source,
              App::Core::CommanderDisplacementSource::JumpRecovery)
        << "tick " << tick;
    EXPECT_LE((after - before).length(), 7.0F * dt)
        << "tick " << tick << " displaced the body discontinuously";
  }
  EXPECT_GT(airborne_ticks, 30) << "the jump never left the ground";
}

TEST_F(CommanderMotorTest, AirborneMomentumIsCommittedAtTakeoff) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  auto* data = commander->get_component<Engine::Core::CommanderComponent>();

  CommanderControlController controller;
  Render::GL::Camera camera;
  constexpr float dt = 1.0F / 60.0F;
  auto step = [&]() -> QVector3D {
    QVector3D const before(transform->position.x, 0.0F, transform->position.z);
    EXPECT_TRUE(controller.update(world, commander->get_id(), 1, camera, dt));
    return (QVector3D(transform->position.x, 0.0F, transform->position.z) - before) /
           dt;
  };
  auto land = [&]() {
    for (int tick = 0; tick < 120 && data->jump_active; ++tick) {
      static_cast<void>(step());
    }
    ASSERT_FALSE(data->jump_active);
  };

  controller.input().forward = true;
  QVector3D takeoff;
  for (int tick = 0; tick < 60; ++tick) {
    takeoff = step();
  }
  ASSERT_GT(takeoff.z(), 3.0F);

  // Releasing the stick in the air keeps the jump's travel.
  controller.request_jump();
  static_cast<void>(step());
  ASSERT_TRUE(data->jump_active);
  controller.input().forward = false;
  QVector3D last = takeoff;
  while (data->jump_active) {
    QVector3D const velocity = step();
    if (data->jump_active) {
      last = velocity;
    }
  }
  EXPECT_GE(last.z(), 0.95F * takeoff.z())
      << "releasing input braked the jump from " << takeoff.z() << " to " << last.z()
      << " m/s";
  land();

  // Reversing in the air steers at the air limit; it cannot flip the jump.
  controller.input().forward = true;
  for (int tick = 0; tick < 60; ++tick) {
    takeoff = step();
  }
  controller.request_jump();
  static_cast<void>(step());
  ASSERT_TRUE(data->jump_active);
  controller.input().forward = false;
  controller.input().backward = true;
  QVector3D previous = takeoff;
  int airborne = 0;
  while (data->jump_active) {
    QVector3D const velocity = step();
    if (!data->jump_active) {
      break;
    }
    EXPECT_LE((velocity - previous).length(),
              (k_commander_air_acceleration_mps2 * dt) + 1.0e-3F)
        << "airborne tick " << airborne;
    if (airborne < 24) {
      EXPECT_GT(velocity.z(), 0.0F)
          << "airborne tick " << airborne << " already flipped the jump";
    }
    previous = velocity;
    ++airborne;
  }
  EXPECT_GT(airborne, 30);
}

TEST_F(CommanderMotorTest, WadingWalkAndSprintUseSharedWaterAndChillRules) {
  for (const float hz : {30.0F, 60.0F, 120.0F}) {
    for (const bool sprint : {false, true}) {
      Engine::Core::World world;
      auto* entity = create_commander(world, 0.0F, 0.0F);
      auto* water = entity->add_component<Engine::Core::WadingComponent>();
      water->depth = 0.45F;
      water->speed = 0.55F;
      water->chill = 1.0F;
      auto* transform = entity->get_component<Engine::Core::TransformComponent>();
      CommanderControlController controller;
      Render::GL::Camera camera;
      controller.input().forward = true;
      controller.input().run = sprint;
      const float dt = 1.0F / hz;
      float speed = 0.0F;
      for (int tick = 0; tick < static_cast<int>(hz); ++tick) {
        const float before = transform->position.z;
        ASSERT_TRUE(controller.update(world, entity->get_id(), 1, camera, dt));
        speed = (transform->position.z - before) / dt;
      }
      EXPECT_NEAR(speed, 3.75F * 0.55F * 0.9F, 0.01F);
      auto* stamina = entity->get_component<Engine::Core::StaminaComponent>();
      ASSERT_NE(stamina, nullptr);
      EXPECT_FALSE(stamina->run_requested)
          << "sprint input must not drain stamina in a ford";

      // Leaving the water restores sprint, but retains the shared chill penalty.
      water->depth = 0.0F;
      for (int tick = 0; tick < static_cast<int>(hz); ++tick) {
        const float before = transform->position.z;
        ASSERT_TRUE(controller.update(world, entity->get_id(), 1, camera, dt));
        speed = (transform->position.z - before) / dt;
      }
      const float run_scale =
          sprint ? Engine::Core::StaminaComponent::k_run_speed_multiplier : 1.0F;
      EXPECT_NEAR(speed, 3.75F * 0.9F * run_scale, 0.01F);
      EXPECT_EQ(stamina->run_requested, sprint);
    }
  }
}

TEST_F(CommanderMotorTest, WadingScalesDodgeTravelWithoutChangingItsTimelineOrCost) {
  struct Result {
    float distance;
    float stamina;
    int frames;
  };
  auto dodge = [&](bool wet) {
    Engine::Core::World world;
    auto* entity = create_commander(world, 0.0F, 0.0F);
    if (wet) {
      auto* water = entity->add_component<Engine::Core::WadingComponent>();
      water->depth = 0.45F;
      water->speed = 0.55F;
    }
    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    CommanderControlController controller;
    Render::GL::Camera camera;
    controller.input().forward = true;
    controller.request_dodge();
    int frames = 0;
    // Sample the roll and recovery, before ordinary walking resumes.
    for (int tick = 0; tick < 30; ++tick) {
      EXPECT_TRUE(controller.update(world, entity->get_id(), 1, camera, 1.0F / 60.0F));
      EXPECT_FALSE(
          entity->get_component<Engine::Core::StaminaComponent>()->run_requested);
      if (entity->get_component<Engine::Core::CommanderComponent>()->dodge_active) {
        ++frames;
      }
    }
    return Result{transform->position.z,
                  entity->get_component<Engine::Core::StaminaComponent>()->stamina,
                  frames};
  };
  const auto dry = dodge(false);
  const auto wet = dodge(true);
  ASSERT_GT(dry.distance, 1.0F);
  EXPECT_NEAR(wet.distance, dry.distance * 0.55F, 0.02F);
  EXPECT_FLOAT_EQ(wet.stamina, dry.stamina);
  EXPECT_EQ(wet.frames, dry.frames);
}

TEST_F(CommanderMotorTest, FordBridgeAndDeepWaterKeepTheirSharedTraversalRules) {
  Game::Map::MapDefinition map;
  map.coordSystem = Game::Map::CoordSystem::World;
  map.grid.width = 96;
  map.grid.height = 96;
  map.grid.tile_size = 1.0F;
  map.biome.procedural_trees_enabled = false;
  map.biome.procedural_boulders_enabled = false;
  map.biome.procedural_iron_ore_enabled = false;
  map.rivers.push_back(
      {QVector3D(-48.0F, 0.0F, 0.0F), QVector3D(48.0F, 0.0F, 0.0F), 8.0F});
  map.fords.push_back({QStringLiteral("ford"),
                       QVector3D(0.0F, 0.0F, 0.0F),
                       14.0F,
                       {.depth = 0.45F, .speed = 0.55F}});
  map.bridges.push_back(
      {QVector3D(24.0F, 0.0F, -11.0F), QVector3D(24.0F, 0.0F, 11.0F), 8.0F, 0.5F});
  auto& terrain = Game::Map::TerrainService::instance();
  terrain.initialize(map);
  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  pathfinder->mark_navigation_grid_dirty();
  pathfinder->update_navigation_grid();

  for (const float hz : {30.0F, 60.0F, 120.0F}) {
    for (const float lane_x : {-24.0F, 0.0F, 24.0F}) {
      Engine::Core::World world;
      auto* entity = create_commander(world, lane_x, -14.0F);
      auto* transform = entity->get_component<Engine::Core::TransformComponent>();
      world.add_system(std::make_unique<Game::Systems::TerrainAlignmentSystem>());
      Game::Systems::FordSystem ford({.terrain = terrain});
      CommanderControlController controller;
      Render::GL::Camera camera;
      controller.input().forward = true;
      controller.input().run = true;
      const float dt = 1.0F / hz;
      int wet_ticks = 0;
      bool exited_water = false;
      bool crossed_deck = false;
      for (int tick = 0; tick < static_cast<int>(hz * 14.0F); ++tick) {
        ford.update(&world, dt);
        const auto* water = entity->get_component<Engine::Core::WadingComponent>();
        const bool wet = water != nullptr && water->in_water();
        ASSERT_TRUE(controller.update(world, entity->get_id(), 1, camera, dt));
        if (wet) {
          ++wet_ticks;
          EXPECT_FALSE(
              entity->get_component<Engine::Core::StaminaComponent>()->run_requested);
        } else if (wet_ticks > 0) {
          exited_water = true;
        }
        world.update(dt);
        EXPECT_NEAR(transform->position.y,
                    terrain.resolve_surface_world_y(transform->position.x,
                                                    transform->position.z),
                    0.001F);
        crossed_deck |= lane_x == 24.0F && std::abs(transform->position.z) < 1.0F &&
                        transform->position.y > 0.1F;
        if (transform->position.z >= 14.0F) {
          break;
        }
      }
      if (lane_x == -24.0F) {
        EXPECT_LT(transform->position.z, -3.0F)
            << "deep water must block direct control";
        EXPECT_EQ(wet_ticks, 0);
      } else {
        EXPECT_GE(transform->position.z, 14.0F)
            << "crossing stalled at " << hz << " Hz";
        if (lane_x == 0.0F) {
          EXPECT_GT(wet_ticks, static_cast<int>(hz));
          EXPECT_TRUE(exited_water);
        } else {
          EXPECT_EQ(wet_ticks, 0) << "the bridge deck must remain dry";
          EXPECT_TRUE(crossed_deck);
        }
      }
    }
  }
}

TEST_F(CommanderMotorTest, FrontLoadedRollKeepsItsDistanceAcrossTickRatesAndHitches) {
  for (const auto& steps :
       std::vector<std::vector<float>>{{1.0F / 30.0F},
                                       {1.0F / 60.0F},
                                       {1.0F / 120.0F},
                                       {1.0F / 120.0F, 0.033F, 0.05F, 0.1F}}) {
    for (const bool wet : {false, true}) {
      Engine::Core::World world;
      auto* entity = create_commander(world, 0.0F, 0.0F);
      if (wet) {
        auto* water = entity->add_component<Engine::Core::WadingComponent>();
        water->depth = 0.45F;
        water->speed = 0.55F;
      }
      auto* transform = entity->get_component<Engine::Core::TransformComponent>();
      const auto* data = entity->get_component<Engine::Core::CommanderComponent>();
      CommanderControlController controller;
      Render::GL::Camera camera;
      controller.request_dodge();
      float first_speed = 0.0F;
      float final_speed = 0.0F;
      int roll_ticks = 0;
      for (int tick = 0; tick < 100; ++tick) {
        const float dt = steps[tick % steps.size()];
        const float before = transform->position.z;
        ASSERT_TRUE(controller.update(world, entity->get_id(), 1, camera, dt));
        if (!data->dodge_active) {
          break;
        }
        final_speed = (transform->position.z - before) / dt;
        if (roll_ticks++ == 0) {
          first_speed = final_speed;
        }
      }
      ASSERT_GT(roll_ticks, 0);
      EXPECT_FALSE(data->dodge_active);
      EXPECT_NEAR(transform->position.z, 2.34F * (wet ? 0.55F : 1.0F), 0.001F);
      EXPECT_GT(first_speed, final_speed * 2.0F)
          << "the roll must lose speed before recovery instead of stopping at full "
             "speed";
    }
  }
}

TEST_F(CommanderMotorTest, BackwardRollKeepsTakeoffFacingWhileTheCameraTurns) {
  Engine::Core::World world;
  auto* entity = create_commander(world, 0.0F, 0.0F);
  auto* transform = entity->get_component<Engine::Core::TransformComponent>();
  auto* data = entity->get_component<Engine::Core::CommanderComponent>();
  CommanderControlController controller;
  Render::GL::Camera camera;
  ASSERT_TRUE(controller.update(world, entity->get_id(), 1, camera, 1.0F / 60.0F));
  controller.request_dodge(QVector3D(0.0F, 0.0F, -1.0F));
  for (int tick = 0; tick < 21; ++tick) {
    controller.set_view_yaw(static_cast<float>(tick) * 4.0F);
    ASSERT_TRUE(controller.update(world, entity->get_id(), 1, camera, 1.0F / 60.0F));
    ASSERT_TRUE(data->dodge_active);
    EXPECT_NEAR(transform->rotation.y, 0.0F, 0.01F)
        << "body yaw must not twist the rendered somersault";
  }
  EXPECT_LT(transform->position.z, -2.0F);
}

} // namespace
