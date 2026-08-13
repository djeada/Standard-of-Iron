#include <QVector3D>

#include <algorithm>
#include <gtest/gtest.h>

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "render/entity/registry.h"
#include "render/entity/wildlife/wildlife_draw_state.h"
#include "render/wildlife/sheep_spec.h"
#include "render/wildlife/wildlife_rig.h"
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

auto mean_foot_offset(const Render::Wildlife::RigPose& pose) -> float {
  float total = 0.0F;
  for (const auto& leg : pose.legs) {
    total += leg.foot.x();
  }
  return total / static_cast<float>(pose.legs.size());
}

auto body_to_foot_drop(const Render::Wildlife::RigPose& pose) -> float {
  float feet = pose.legs[0].foot.y();
  for (const auto& leg : pose.legs) {
    feet = std::min(feet, leg.foot.y());
  }
  return highest_point(pose) - feet;
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

  EXPECT_GT(std::abs(mean_foot_offset(down)), 0.25F);
  EXPECT_LT(std::abs(mean_foot_offset(up)), 0.05F);
  EXPECT_LT(body_to_foot_drop(down), body_to_foot_drop(up) * 0.5F);

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
  EXPECT_GT(std::abs(mean_foot_offset(down)), 0.25F);
  EXPECT_LT(std::abs(mean_foot_offset(up)), 0.05F);
  EXPECT_LT(body_to_foot_drop(down), body_to_foot_drop(up) * 0.5F);
  EXPECT_GE(lowest_point(down), -0.05F);
}

TEST(WildlifeDeathMotion, LegsGiveWayBeforeTheBodyHasFallen) {

  const auto early = Render::Wildlife::death_motion(0.15F);
  EXPECT_GT(early.buckle, 0.6F) << "the legs should be most of the way gone by 15%";
  EXPECT_LT(early.fall, 0.15F) << "the body should barely have moved yet";
}

TEST(WildlifeDeathMotion, TheHeadArrivesAfterTheBody) {
  for (float const phase : {0.25F, 0.40F, 0.55F}) {
    const auto m = Render::Wildlife::death_motion(phase);
    EXPECT_LT(m.head, m.fall) << "dead weight on a neck lands after the body, at phase "
                              << phase;
  }
}

TEST(WildlifeDeathMotion, TheBodyBouncesOnceAndThenLiesStill) {
  const auto before_impact = Render::Wildlife::death_motion(0.40F);
  EXPECT_FLOAT_EQ(before_impact.settle, 0.0F)
      << "nothing should bounce before the body has hit anything";

  bool bounced = false;
  for (int step = 0; step <= 20; ++step) {
    float const phase = 0.58F + (0.42F * static_cast<float>(step) / 20.0F);
    bounced = bounced || std::abs(Render::Wildlife::death_motion(phase).settle) > 0.05F;
  }
  EXPECT_TRUE(bounced) << "the impact should produce a visible settle";

  const auto at_rest = Render::Wildlife::death_motion(1.0F);
  EXPECT_LT(std::abs(at_rest.settle), 0.02F)
      << "and it should have damped out by the end";
}

TEST(WildlifeDeathMotion, TheStruggleIsOverEarly) {
  bool struggled = false;
  for (int step = 0; step <= 10; ++step) {
    float const phase = 0.02F + (0.18F * static_cast<float>(step) / 10.0F);
    struggled =
        struggled || std::abs(Render::Wildlife::death_motion(phase).thrash) > 0.2F;
  }
  EXPECT_TRUE(struggled)
      << "there should be a struggle while there is strength for one";

  for (float const late : {0.55F, 0.75F, 1.0F}) {
    EXPECT_LT(std::abs(Render::Wildlife::death_motion(late).thrash), 0.02F)
        << "and nothing should still be twitching at phase " << late;
  }
}

TEST(WildlifeDeathMotion, EveryStageIsFinishedWhenTheClipIs) {
  const auto done = Render::Wildlife::death_motion(1.0F);
  EXPECT_FLOAT_EQ(done.buckle, 1.0F);
  EXPECT_FLOAT_EQ(done.fall, 1.0F);
  EXPECT_FLOAT_EQ(done.roll, 1.0F);
  EXPECT_FLOAT_EQ(done.head, 1.0F);
}

TEST(WildlifeActionPose, WolfWorriesWhatItHasHoldOf) {

  WolfDrive holding;
  holding.gait = WolfGait::Stand;
  holding.lunge = 1.0F;
  WolfDrive shaking = holding;
  shaking.head_shake = 1.0F;

  const auto still = Render::Wildlife::wolf_pose(holding);
  const auto worried = Render::Wildlife::wolf_pose(shaking);

  EXPECT_GT(std::abs(worried.muzzle.x() - still.muzzle.x()), 0.05F);

  float const muzzle_swing = std::abs(worried.muzzle.x() - still.muzzle.x());
  float const withers_swing = std::abs(worried.withers.x() - still.withers.x());
  EXPECT_GT(muzzle_swing, withers_swing);

  WolfDrive const other_way = [&] {
    WolfDrive d = holding;
    d.head_shake = -1.0F;
    return d;
  }();
  const auto worried_other = Render::Wildlife::wolf_pose(other_way);
  EXPECT_LT((worried.muzzle.x() - still.muzzle.x()) *
                (worried_other.muzzle.x() - still.muzzle.x()),
            0.0F)
      << "the shake has to go both ways or it is a lean, not a shake";
}

TEST(WildlifeActionPose, AStandingWolfStillBreathesAndLooksAround) {

  WolfDrive rest;
  rest.gait = WolfGait::Stand;
  const auto still = Render::Wildlife::wolf_pose(rest);

  WolfDrive alive = rest;
  alive.breath = 1.0F;
  alive.head_turn = 1.0F;
  alive.tail_sway = 1.0F;
  const auto moved = Render::Wildlife::wolf_pose(alive);

  EXPECT_GT(moved.body_front.y() - still.body_front.y(), 0.004F)
      << "the ribcage has to rise, otherwise a standing wolf is a statue";
  EXPECT_GT(std::abs(moved.muzzle.x() - still.muzzle.x()), 0.05F);
  EXPECT_GT(std::abs(moved.tail_tip.x() - still.tail_tip.x()), 0.02F);

  WolfDrive const other_way = [&] {
    WolfDrive d = rest;
    d.head_turn = -1.0F;
    return d;
  }();
  const auto turned_other = Render::Wildlife::wolf_pose(other_way);
  EXPECT_LT((moved.muzzle.x() - still.muzzle.x()) *
                (turned_other.muzzle.x() - still.muzzle.x()),
            0.0F);
}

TEST(WildlifeActionPose, TheWolfGapesBeforeItCloses) {

  WolfDrive shut;
  shut.gait = WolfGait::Stand;
  shut.lunge = 0.5F;
  WolfDrive gaping = shut;
  gaping.jaw_open = 1.0F;

  const auto closed = Render::Wildlife::wolf_pose(shut);
  const auto open = Render::Wildlife::wolf_pose(gaping);

  float const closed_gap = (closed.jaw_tip - closed.muzzle).length();
  float const open_gap = (open.jaw_tip - open.muzzle).length();
  EXPECT_GT(open_gap, closed_gap * 2.0F)
      << "a bite whose jaw barely parts reads as a nudge";

  float const hinge = (open.jaw_tip - open.jaw_hinge).length();
  float const rest_hinge = (closed.jaw_tip - closed.jaw_hinge).length();
  EXPECT_NEAR(hinge, rest_hinge, rest_hinge * 0.08F)
      << "the jaw must swing on its hinge, not stretch";
}

TEST(WildlifeActionPose, ASheepStartleThrowsItOffTheSpotAndBringsItBack) {

  SheepDrive rest;
  rest.gait = SheepGait::Stand;
  const auto still = Render::Wildlife::sheep_pose(rest);

  SheepDrive jolt = rest;
  jolt.startle = 0.22F;
  const auto thrown = Render::Wildlife::sheep_pose(jolt);

  EXPECT_GT(std::abs(thrown.body_rear.x() - still.body_rear.x()), 0.05F)
      << "the body has to shy sideways, not just nod";
  EXPECT_GT(thrown.body_rear.y() - still.body_rear.y(), 0.02F)
      << "the sheep should come off the ground as it bolts";
  EXPECT_GT(thrown.poll.y() - still.poll.y(), 0.03F) << "the head should be thrown up";

  SheepDrive settled = rest;
  settled.startle = 1.0F;
  const auto recovered = Render::Wildlife::sheep_pose(settled);
  EXPECT_NEAR(recovered.body_rear.x(), still.body_rear.x(), 0.012F);
  EXPECT_NEAR(recovered.body_rear.y(), still.body_rear.y(), 0.012F)
      << "the clip has to end where the gait clips start or it will pop";
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
