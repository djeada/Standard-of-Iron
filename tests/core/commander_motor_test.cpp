#include <cmath>
#include <gtest/gtest.h>
#include <vector>

#include "app/commander/commander_control_controller.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/pathfinding.h"
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
    unit->spawn_type = Game::Units::SpawnType::Knight;
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

  float const view_turn = controller.view_yaw() - resting_yaw;
  float const body_turn = transform->rotation.y - resting_yaw;
  EXPECT_GT(body_turn, 0.0F) << "past the threshold the body has to follow";
  EXPECT_LT(body_turn, view_turn - 20.0F)
      << "the body turned " << body_turn << " degrees against a " << view_turn
      << " degree sweep; it is still following the camera one to one";

  for (int frame = 0; frame < 60; ++frame) {
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
  }
  EXPECT_NEAR(transform->rotation.y, controller.view_yaw(), 0.5F)
      << "once the player stops looking around the body must settle facing "
         "the same way the camera does";
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
  ASSERT_LT(std::abs(transform->rotation.y - 85.0F), 84.0F)
      << "the idle body is expected to lag a big look";

  controller.primary_action_down();
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
  EXPECT_NEAR(transform->rotation.y, 85.0F, 0.01F)
      << "a swing has to land where the player is looking, not where the body "
         "happened to be resting";
}

TEST_F(CommanderMotorTest, TurningAwayFromAWallKeepsTheSpeedAlreadyBuilt) {
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

  EXPECT_GT(first_step, standing_start * 2.0F)
      << "three frames of wall contact restarted the motor from a standstill: "
      << first_step << " m/s against " << standing_start
      << " m/s from a genuine standing start";
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
        unit->spawn_type = Game::Units::SpawnType::Knight;
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

} // namespace
