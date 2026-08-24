#include <cmath>
#include <cstdio>
#include <gtest/gtest.h>
#include <vector>

#include "app/commander/commander_control_controller.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/nav_grid.h"
#include "scene/camera.h"

namespace {

constexpr float k_simulation_tick_seconds = 1.0F / 60.0F;

struct PresentedSample {
  QVector3D authoritative_position;
  QVector3D presented_position;
  QVector3D visual_anchor;
  QVector3D eye;
  float presented_yaw{0.0F};
  float yaw{0.0F};
  float largest_presented_step{0.0F};
  int presented_frames{0};
};

class CommanderPresentationPoseTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::NavGrid::initialize(64, 64);
  }

  void TearDown() override {
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
  }

  static auto create_commander(Engine::Core::World& world) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    if (entity == nullptr) {
      return nullptr;
    }
    entity->add_component<Engine::Core::TransformComponent>(0.0F, 0.0F, 0.0F);
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

  static auto run_scripted_second(float presentation_hz,
                                  float seconds = 1.0F) -> PresentedSample {
    Engine::Core::World world;
    auto* commander = create_commander(world);
    EXPECT_NE(commander, nullptr);
    if (commander == nullptr) {
      return {};
    }

    CommanderControlController controller;
    Render::GL::Camera camera;
    const auto commander_id = commander->get_id();
    const float frame_seconds = 1.0F / presentation_hz;

    controller.key_down(Qt::Key_W);

    PresentedSample sample;
    bool have_previous_presented = false;
    QVector3D previous_presented;

    float presented_accumulator = 0.0F;
    const auto ticks =
        static_cast<int>(std::lround(seconds / k_simulation_tick_seconds));
    for (int tick = 0; tick < ticks; ++tick) {
      static_cast<void>(controller.update_simulation(
          world, commander_id, 1, k_simulation_tick_seconds));
      presented_accumulator += k_simulation_tick_seconds;
      while (presented_accumulator >= frame_seconds - 1.0e-6F) {
        presented_accumulator -= frame_seconds;
        static_cast<void>(controller.sample_frame_intent(nullptr));
        controller.update_camera_presentation(
            world, commander_id, camera, frame_seconds);

        auto const& pose = controller.presentation_pose();
        QVector3D const presented(pose.position.x, pose.position.y, pose.position.z);
        if (have_previous_presented) {
          sample.largest_presented_step = std::max(
              sample.largest_presented_step, (presented - previous_presented).length());
        }
        previous_presented = presented;
        have_previous_presented = true;
        ++sample.presented_frames;
      }
    }

    auto const* transform =
        commander->get_component<Engine::Core::TransformComponent>();
    auto const& trace = controller.camera_trace();
    auto const& pose = controller.presentation_pose();
    sample.authoritative_position = transform != nullptr
                                        ? QVector3D(transform->position.x,
                                                    transform->position.y,
                                                    transform->position.z)
                                        : QVector3D();
    sample.presented_position =
        QVector3D(pose.position.x, pose.position.y, pose.position.z);
    sample.presented_yaw = pose.yaw;
    sample.visual_anchor = trace.visual_anchor;
    sample.eye = trace.eye_resolved;
    sample.yaw = trace.yaw;
    return sample;
  }
};

TEST_F(CommanderPresentationPoseTest, PresentationPoseIsFrameRateInvariant) {
  const auto reference = run_scripted_second(60.0F);
  ASSERT_GT(reference.authoritative_position.z(), 0.5F)
      << "the scripted second has to actually move the commander";

  for (float const presentation_hz : {30.0F, 120.0F, 144.0F}) {
    const auto sample = run_scripted_second(presentation_hz);

    EXPECT_NEAR(
        (sample.authoritative_position - reference.authoritative_position).length(),
        0.0F,
        1.0e-4F)
        << presentation_hz
        << " Hz: the same scripted ticks must produce the same simulation";
    EXPECT_NEAR((sample.presented_position - reference.presented_position).length(),
                0.0F,
                0.03F)
        << presentation_hz << " Hz: the presented pose depends on the display rate";
    EXPECT_NEAR(std::abs(sample.presented_yaw - reference.presented_yaw), 0.0F, 0.25F)
        << presentation_hz << " Hz: presented yaw depends on the display rate";
  }
}

TEST_F(CommanderPresentationPoseTest, PresentedMotionIsContinuousAboveTheTickRate) {
  const auto ticked = run_scripted_second(60.0F);
  const auto fast = run_scripted_second(144.0F);

  ASSERT_GT(ticked.presented_frames, 0);
  ASSERT_GT(fast.presented_frames, ticked.presented_frames);

  EXPECT_LT(fast.largest_presented_step, ticked.largest_presented_step)
      << "at 144 Hz over a 60 Hz simulation the body must move in smaller steps "
         "than the simulation takes, not hold still and jump";
}

TEST_F(CommanderPresentationPoseTest,
       TheCameraFramesThePresentedPoseNotTheRawTransform) {
  Engine::Core::World world;
  auto* commander = create_commander(world);
  ASSERT_NE(commander, nullptr);

  CommanderControlController controller;
  Render::GL::Camera camera;
  const auto commander_id = commander->get_id();

  controller.key_down(Qt::Key_W);
  for (int tick = 0; tick < 20; ++tick) {
    static_cast<void>(controller.update_simulation(
        world, commander_id, 1, k_simulation_tick_seconds));
    controller.update_camera_presentation(
        world, commander_id, camera, k_simulation_tick_seconds);
  }

  static_cast<void>(
      controller.update_simulation(world, commander_id, 1, k_simulation_tick_seconds));
  controller.update_camera_presentation(
      world, commander_id, camera, k_simulation_tick_seconds * 0.5F);

  auto const& pose = controller.presentation_pose();
  QVector3D const presented(pose.position.x, pose.position.y, pose.position.z);
  auto const* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);
  QVector3D const authoritative(
      transform->position.x, transform->position.y, transform->position.z);

  EXPECT_LT(pose.alpha, 1.0F) << "half a tick in, the pose must still be interpolating";
  EXPECT_GT((presented - authoritative).length(), 1.0e-4F)
      << "the presented pose must sit between the two authoritative samples";
  EXPECT_NEAR((controller.camera_trace().commander_position - presented).length(),
              0.0F,
              1.0e-5F)
      << "the camera must frame the presented pose, not the raw simulation transform";
}

TEST_F(CommanderPresentationPoseTest, ATeleportSnapsInsteadOfSmearing) {
  Engine::Core::World world;
  auto* commander = create_commander(world);
  ASSERT_NE(commander, nullptr);
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);

  CommanderControlController controller;
  Render::GL::Camera camera;
  const auto commander_id = commander->get_id();

  for (int tick = 0; tick < 10; ++tick) {
    static_cast<void>(controller.update_simulation(
        world, commander_id, 1, k_simulation_tick_seconds));
    controller.update_camera_presentation(
        world, commander_id, camera, k_simulation_tick_seconds);
  }

  transform->position.x = 30.0F;
  transform->position.z = 30.0F;
  static_cast<void>(
      controller.update_simulation(world, commander_id, 1, k_simulation_tick_seconds));
  controller.update_camera_presentation(
      world, commander_id, camera, k_simulation_tick_seconds * 0.25F);

  auto const& pose = controller.presentation_pose();
  EXPECT_NEAR(pose.position.x, transform->position.x, 1.0e-3F);
  EXPECT_NEAR(pose.position.z, transform->position.z, 1.0e-3F);
  EXPECT_FALSE(pose.extrapolated);

  auto const* sample =
      commander->get_component<Engine::Core::CommanderPresentationSampleComponent>();
  ASSERT_NE(sample, nullptr);
  EXPECT_TRUE(sample->snap)
      << "a correction above the teleport threshold must be marked as a snap";
}

TEST_F(CommanderPresentationPoseTest, ResettingControlSnapsTheNextPresentedPose) {
  Engine::Core::World world;
  auto* commander = create_commander(world);
  ASSERT_NE(commander, nullptr);

  CommanderControlController controller;
  Render::GL::Camera camera;
  const auto commander_id = commander->get_id();

  controller.key_down(Qt::Key_W);
  for (int tick = 0; tick < 10; ++tick) {
    static_cast<void>(controller.update_simulation(
        world, commander_id, 1, k_simulation_tick_seconds));
    controller.update_camera_presentation(
        world, commander_id, camera, k_simulation_tick_seconds);
  }

  controller.reset();
  static_cast<void>(
      controller.update_simulation(world, commander_id, 1, k_simulation_tick_seconds));

  auto const* sample =
      commander->get_component<Engine::Core::CommanderPresentationSampleComponent>();
  ASSERT_NE(sample, nullptr);
  EXPECT_TRUE(sample->snap)
      << "mode entry, focus loss and death all reset control; none of them may "
         "blend the commander in from where he used to be";
}

} // namespace
