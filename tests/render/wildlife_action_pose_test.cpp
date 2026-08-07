#include <QVector3D>

#include <algorithm>
#include <gtest/gtest.h>

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "render/entity/registry.h"
#include "render/entity/wildlife/wildlife_draw_state.h"
#include "render/wildlife/sheep_spec.h"
#include "render/wildlife/wolf_spec.h"

namespace {

using Render::Wildlife::SheepDrive;
using Render::Wildlife::SheepGait;
using Render::Wildlife::WolfDrive;
using Render::Wildlife::WolfGait;

auto lowest_point(const Render::Wildlife::RigPose& pose) -> float {
  float lowest = pose.root.y();
  for (const auto& leg : pose.legs) {
    lowest = std::min({lowest, leg.knee.y(), leg.foot.y(), leg.toe.y()});
  }
  return lowest;
}

auto highest_point(const Render::Wildlife::RigPose& pose) -> float {
  return std::max({pose.body_front.y(), pose.body_rear.y(), pose.withers.y()});
}

TEST(WildlifeActionPose, WolfLungeDrivesTheWholeBodyForward) {
  WolfDrive rest;
  rest.gait = WolfGait::Stand;
  WolfDrive lunging = rest;
  lunging.lunge = 1.0F;

  const auto still = Render::Wildlife::wolf_pose(rest);
  const auto snap = Render::Wildlife::wolf_pose(lunging);

  EXPECT_GT(snap.muzzle.z() - still.muzzle.z(), 0.20F);

  EXPECT_LT(snap.body_front.y(), still.body_front.y() - 0.06F);
  EXPECT_GT(snap.body_rear.y(), still.body_rear.y() + 0.01F);

  EXPECT_GT(snap.legs[0].toe.z() - still.legs[0].toe.z(), 0.15F);
}

TEST(WildlifeActionPose, WolfLungeKeepsTheHeadAttached) {
  WolfDrive rest;
  rest.gait = WolfGait::Stand;
  const auto still = Render::Wildlife::wolf_pose(rest);
  float const rest_neck = (still.poll - still.withers).length();
  float const rest_head = (still.muzzle - still.poll).length();
  float const rest_ear = (still.ear_tip_l - still.ear_base_l).length();

  for (int step = 0; step <= 10; ++step) {
    WolfDrive lunging = rest;
    lunging.lunge = static_cast<float>(step) / 10.0F;
    const auto pose = Render::Wildlife::wolf_pose(lunging);

    EXPECT_NEAR((pose.poll - pose.withers).length(), rest_neck, rest_neck * 0.12F)
        << "neck stretched at lunge " << lunging.lunge;
    EXPECT_NEAR((pose.muzzle - pose.poll).length(), rest_head, rest_head * 0.12F)
        << "head detached at lunge " << lunging.lunge;
    EXPECT_NEAR((pose.ear_tip_l - pose.ear_base_l).length(), rest_ear, rest_ear * 0.12F)
        << "ear stretched at lunge " << lunging.lunge;

    for (const auto& leg : pose.legs) {
      float const upper = (leg.knee - leg.shoulder).length();
      float const lower = (leg.foot - leg.knee).length();
      EXPECT_GT(upper, 0.02F);
      EXPECT_GT(lower, 0.02F);
    }
  }
}

TEST(WildlifeActionPose, WolfBiteOpensAnArticulatedJaw) {
  WolfDrive resting;
  resting.gait = WolfGait::Stand;
  WolfDrive biting = resting;
  biting.jaw_open = 1.0F;

  const auto closed = Render::Wildlife::wolf_pose(resting);
  const auto open = Render::Wildlife::wolf_pose(biting);

  EXPECT_NEAR((open.jaw_tip - open.jaw_hinge).length(),
              (closed.jaw_tip - closed.jaw_hinge).length(),
              0.001F);
  EXPECT_LT(open.jaw_tip.y(), closed.jaw_tip.y() - 0.04F);
  EXPECT_GT((open.jaw_tip - open.muzzle).length(),
            (closed.jaw_tip - closed.muzzle).length() + 0.03F);
}

TEST(WildlifeActionPose, WolfCollapsePutsTheAnimalOnTheGround) {
  WolfDrive standing;
  standing.gait = WolfGait::Stand;
  WolfDrive fallen = standing;
  fallen.collapse = 1.0F;

  const auto up = Render::Wildlife::wolf_pose(standing);
  const auto down = Render::Wildlife::wolf_pose(fallen);

  EXPECT_LT(highest_point(down), highest_point(up) * 0.45F);
  EXPECT_LT(down.muzzle.y(), up.muzzle.y() * 0.35F);

  EXPECT_GT(std::abs(down.body_front.x() - up.body_front.x()), 0.05F);

  EXPECT_GE(lowest_point(down), -0.05F);
}

TEST(WildlifeActionPose, WolfCollapseIsProgressive) {
  WolfDrive half;
  half.gait = WolfGait::Stand;
  half.collapse = 0.5F;
  WolfDrive full = half;
  full.collapse = 1.0F;

  const auto midway = Render::Wildlife::wolf_pose(half);
  const auto finished = Render::Wildlife::wolf_pose(full);
  WolfDrive standing;
  standing.gait = WolfGait::Stand;
  const auto up = Render::Wildlife::wolf_pose(standing);

  EXPECT_LT(highest_point(midway), highest_point(up));
  EXPECT_LT(highest_point(finished), highest_point(midway));
}

TEST(WildlifeActionPose, SheepCollapsePutsTheAnimalOnTheGround) {
  SheepDrive standing;
  standing.gait = SheepGait::Stand;
  SheepDrive fallen = standing;
  fallen.collapse = 1.0F;

  const auto up = Render::Wildlife::sheep_pose(standing);
  const auto down = Render::Wildlife::sheep_pose(fallen);

  EXPECT_LT(highest_point(down), highest_point(up) * 0.45F);
  EXPECT_LT(down.muzzle.y(), up.muzzle.y() * 0.40F);
  EXPECT_GT(std::abs(down.body_front.x() - up.body_front.x()), 0.05F);
  EXPECT_GE(lowest_point(down), -0.05F);
}

TEST(WildlifeActionState, BiteAndDeathProgressComeFromSimulationComponents) {
  Engine::Core::World world;
  auto* entity = world.create_entity();
  auto* wildlife = entity->add_component<Engine::Core::WildlifeComponent>();
  wildlife->species = Game::Wildlife::Species::Wolf;
  wildlife->behavior = Game::Wildlife::Behavior::Stalk;
  wildlife->bite_timer =
      Engine::Core::WildlifeComponent::k_bite_animation_seconds * 0.5F;

  Render::GL::DrawContext ctx;
  ctx.entity = entity;
  auto state = Render::GL::Wildlife::resolve_draw_state(ctx, 3.1F);
  EXPECT_NEAR(state.bite_progress, 0.5F, 0.001F);
  EXPECT_EQ(state.behavior, Game::Wildlife::Behavior::Stalk);

  auto* death = entity->add_component<Engine::Core::DeathAnimationComponent>();
  death->state = Engine::Core::DeathSequenceState::Dying;
  death->state_time = 0.45F;
  death->state_duration = 0.90F;
  state = Render::GL::Wildlife::resolve_draw_state(ctx, 3.1F);
  EXPECT_NEAR(state.death_progress, 0.5F, 0.001F);
  EXPECT_FALSE(state.dead);

  death->state = Engine::Core::DeathSequenceState::DeadHold;
  state = Render::GL::Wildlife::resolve_draw_state(ctx, 3.1F);
  EXPECT_FLOAT_EQ(state.death_progress, 1.0F);
  EXPECT_TRUE(state.dead);
}

} // namespace
