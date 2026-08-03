#include <cmath>
#include <gtest/gtest.h>

#include "animation/showcase_pose_manifest.h"
#include "core/component.h"
#include "core/entity.h"
#include "core/world.h"
#include "systems/arrow_system.h"
#include "systems/showcase_routine_system.h"

using namespace Engine::Core;
using namespace Game::Systems;

namespace {

auto move_id(Animation::HumanoidShowcaseMove move) -> std::uint8_t {
  return static_cast<std::uint8_t>(move);
}

class ShowcaseRoutineSystemTest : public ::testing::Test {
protected:
  void SetUp() override { world = std::make_unique<World>(); }

  auto make_performer(float facing_degrees = 0.0F) -> Entity* {
    auto* entity = world->create_entity();
    auto* transform = entity->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
    transform->rotation.y = facing_degrees;
    transform->scale = {1.0F, 1.0F, 1.0F};
    entity->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
    return entity;
  }

  std::unique_ptr<World> world;
  ShowcaseRoutineSystem system;
};

} // namespace

TEST_F(ShowcaseRoutineSystemTest, WalksThroughStepsAndLoops) {
  auto* entity = make_performer();
  auto* routine = entity->add_component<ShowcaseRoutineComponent>();
  routine->steps = {{move_id(Animation::HumanoidShowcaseMove::Jump), 1.0F, 0.0F},
                    {move_id(Animation::HumanoidShowcaseMove::Handstand), 2.0F, 0.0F}};
  routine->loop = true;

  system.update(world.get(), 0.5F);
  EXPECT_TRUE(routine->active);
  EXPECT_EQ(routine->current_move, move_id(Animation::HumanoidShowcaseMove::Jump));
  EXPECT_NEAR(routine->phase, 0.5F, 1.0e-4F);

  system.update(world.get(), 1.0F);
  EXPECT_EQ(routine->current_move, move_id(Animation::HumanoidShowcaseMove::Handstand));
  EXPECT_NEAR(routine->phase, 0.25F, 1.0e-4F);

  system.update(world.get(), 2.0F);
  EXPECT_EQ(routine->current_move, move_id(Animation::HumanoidShowcaseMove::Jump));
}

TEST_F(ShowcaseRoutineSystemTest, StopsAtTheEndWhenNotLooping) {
  auto* entity = make_performer();
  auto* routine = entity->add_component<ShowcaseRoutineComponent>();
  routine->steps = {{move_id(Animation::HumanoidShowcaseMove::Jump), 1.0F, 0.0F}};
  routine->loop = false;

  system.update(world.get(), 1.5F);
  EXPECT_TRUE(routine->finished);
  EXPECT_FALSE(routine->active);
}

TEST_F(ShowcaseRoutineSystemTest, HoldsTheFinalPoseForTheHoldWindow) {
  auto* entity = make_performer();
  auto* routine = entity->add_component<ShowcaseRoutineComponent>();
  routine->steps = {{move_id(Animation::HumanoidShowcaseMove::Handstand), 1.0F, 1.0F}};
  routine->loop = false;

  system.update(world.get(), 1.4F);
  EXPECT_TRUE(routine->active);
  EXPECT_NEAR(routine->phase, 1.0F, 1.0e-4F);
  EXPECT_FALSE(routine->finished);
}

TEST_F(ShowcaseRoutineSystemTest, CarriesAuthoredRootMotionIntoTheTransform) {
  auto* entity = make_performer();
  auto* transform = entity->get_component<TransformComponent>();
  auto* routine = entity->add_component<ShowcaseRoutineComponent>();
  routine->steps = {{move_id(Animation::HumanoidShowcaseMove::FrontFlip), 1.0F, 0.0F}};
  routine->loop = false;

  system.update(world.get(), 1.0F);
  auto const authored = Animation::humanoid_showcase_root_travel(
      Animation::HumanoidShowcaseMove::FrontFlip, 1.0F);
  EXPECT_GT(authored.z, 1.0F);
  EXPECT_NEAR(transform->position.z, authored.z, 1.0e-3F);
  EXPECT_NEAR(transform->position.x, 0.0F, 1.0e-3F);
}

TEST_F(ShowcaseRoutineSystemTest, RootMotionFollowsTheFacingYaw) {
  auto* entity = make_performer(90.0F);
  auto* transform = entity->get_component<TransformComponent>();
  auto* routine = entity->add_component<ShowcaseRoutineComponent>();
  routine->steps = {{move_id(Animation::HumanoidShowcaseMove::FrontFlip), 1.0F, 0.0F}};
  routine->loop = false;

  system.update(world.get(), 1.0F);
  auto const authored = Animation::humanoid_showcase_root_travel(
      Animation::HumanoidShowcaseMove::FrontFlip, 1.0F);
  EXPECT_NEAR(transform->position.x, authored.z, 1.0e-2F);
  EXPECT_NEAR(transform->position.z, 0.0F, 1.0e-2F);
}

TEST_F(ShowcaseRoutineSystemTest, ReleasesOneJavelinPerSpearThrow) {
  world->add_system(std::make_unique<ArrowSystem>());
  auto* entity = make_performer();
  auto* routine = entity->add_component<ShowcaseRoutineComponent>();
  routine->steps = {{move_id(Animation::HumanoidShowcaseMove::SpearThrow), 2.0F, 0.0F}};
  routine->loop = false;
  routine->has_throw_target = true;
  routine->throw_target_z = 8.0F;

  auto* arrows = world->get_system<ArrowSystem>();
  ASSERT_NE(arrows, nullptr);
  system.update(world.get(), 0.4F);
  EXPECT_TRUE(arrows->arrows().empty());

  system.update(world.get(), 1.0F);
  ASSERT_EQ(arrows->arrows().size(), 1U);
  EXPECT_EQ(arrows->arrows().front().style, ArrowVisualStyle::Javelin);

  system.update(world.get(), 0.3F);
  EXPECT_EQ(arrows->arrows().size(), 1U);
}

TEST(HumanoidShowcaseManifest, EveryMoveResolvesAPoseAcrossItsPhase) {
  for (std::uint8_t id = 1;
       id < static_cast<std::uint8_t>(Animation::HumanoidShowcaseMove::Count);
       ++id) {
    auto const move = static_cast<Animation::HumanoidShowcaseMove>(id);
    for (int step = 0; step <= 20; ++step) {
      auto const sample = Animation::resolve_humanoid_showcase_pose(
          {.move = move, .phase = static_cast<float>(step) / 20.0F});
      ASSERT_TRUE(sample.active) << Animation::humanoid_showcase_move_name(move);
      EXPECT_TRUE(std::isfinite(sample.pelvis.y));
      EXPECT_GT(sample.head.y, -1.0F);
      EXPECT_LT(sample.head.y, 4.0F);
    }
  }
}

TEST(HumanoidShowcaseManifest, LimbSegmentsKeepTheirAuthoredLength) {
  Animation::HumanoidShowcaseRig const rig{};
  for (std::uint8_t id = 1;
       id < static_cast<std::uint8_t>(Animation::HumanoidShowcaseMove::Count);
       ++id) {
    auto const move = static_cast<Animation::HumanoidShowcaseMove>(id);
    for (int step = 0; step <= 10; ++step) {
      auto const sample = Animation::resolve_humanoid_showcase_pose(
          {.move = move, .phase = static_cast<float>(step) / 10.0F});
      auto const length = [](Animation::PoseVec3 a, Animation::PoseVec3 b) {
        float const dx = a.x - b.x;
        float const dy = a.y - b.y;
        float const dz = a.z - b.z;
        return std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
      };
      EXPECT_NEAR(
          length(sample.shoulder_r, sample.elbow_r), rig.upper_arm_len, 1.0e-3F);
      EXPECT_NEAR(length(sample.elbow_r, sample.hand_r), rig.fore_arm_len, 1.0e-3F);
      EXPECT_NEAR(length(sample.knee_l, sample.foot_l), rig.lower_leg_len, 1.0e-3F);
    }
  }
}

TEST(HumanoidShowcaseManifest, MoveNamesRoundTrip) {
  for (std::uint8_t id = 1;
       id < static_cast<std::uint8_t>(Animation::HumanoidShowcaseMove::Count);
       ++id) {
    auto const move = static_cast<Animation::HumanoidShowcaseMove>(id);
    auto const name = Animation::humanoid_showcase_move_name(move);
    EXPECT_EQ(Animation::humanoid_showcase_move_from_name(name), move);
    EXPECT_EQ(Animation::humanoid_showcase_move_from_name(name.substr(9)), move);
  }
  EXPECT_EQ(Animation::humanoid_showcase_move_from_name("not_a_move"),
            Animation::HumanoidShowcaseMove::None);
}

TEST(HumanoidShowcaseManifest, ClipIdsCoverEveryMoveAndStayInRange) {
  EXPECT_EQ(Animation::humanoid_showcase_clip(0U), Animation::k_unmapped_clip);
  for (std::uint8_t id = 1;
       id < static_cast<std::uint8_t>(Animation::HumanoidShowcaseMove::Count);
       ++id) {
    auto const clip = Animation::humanoid_showcase_clip(id);
    EXPECT_GE(clip, Animation::k_humanoid_showcase_first_clip);
    EXPECT_LT(clip, Animation::k_humanoid_clip_count);
  }
  EXPECT_EQ(Animation::humanoid_showcase_clip(
                static_cast<std::uint8_t>(Animation::HumanoidShowcaseMove::Count)),
            Animation::k_unmapped_clip);
}
