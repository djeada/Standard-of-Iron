#include <gtest/gtest.h>

#include "render/creature/quadruped/clip_set.h"

namespace {

using Render::Creature::Quadruped::clip_for_gait;
using Render::Creature::Quadruped::clip_for_motion;
using Render::Creature::Quadruped::ClipSet;
using Render::GL::GaitType;

TEST(QuadrupedClipSet, HorseGaitsMapDirectly) {
  constexpr ClipSet k_horse_clips{0U, 1U, 2U, 3U, 4U, 5U};

  EXPECT_EQ(clip_for_gait(k_horse_clips, GaitType::IDLE), 0U);
  EXPECT_EQ(clip_for_gait(k_horse_clips, GaitType::WALK), 1U);
  EXPECT_EQ(clip_for_gait(k_horse_clips, GaitType::TROT), 2U);
  EXPECT_EQ(clip_for_gait(k_horse_clips, GaitType::CANTER), 3U);
  EXPECT_EQ(clip_for_gait(k_horse_clips, GaitType::GALLOP), 4U);
  EXPECT_EQ(clip_for_motion(k_horse_clips, GaitType::TROT, true), 5U);
}

TEST(QuadrupedClipSet, RunningFallsBackWhenFasterClipsAreMissing) {
  constexpr ClipSet k_elephant_clips{0U,
                                     1U,
                                     2U,
                                     Render::Creature::Quadruped::k_invalid_clip,
                                     Render::Creature::Quadruped::k_invalid_clip,
                                     3U};

  EXPECT_EQ(clip_for_motion(k_elephant_clips, false, false, false), 0U);
  EXPECT_EQ(clip_for_motion(k_elephant_clips, true, false, false), 1U);
  EXPECT_EQ(clip_for_motion(k_elephant_clips, true, true, false), 2U);
  EXPECT_EQ(clip_for_motion(k_elephant_clips, true, true, true), 3U);
}

TEST(QuadrupedClipSet, GaitFallbackWalksBackTowardAvailableLocomotion) {
  constexpr ClipSet k_sparse_clips{10U,
                                   Render::Creature::Quadruped::k_invalid_clip,
                                   12U,
                                   Render::Creature::Quadruped::k_invalid_clip,
                                   Render::Creature::Quadruped::k_invalid_clip,
                                   19U};

  EXPECT_EQ(clip_for_gait(k_sparse_clips, GaitType::WALK), 12U);
  EXPECT_EQ(clip_for_gait(k_sparse_clips, GaitType::GALLOP), 12U);
  EXPECT_EQ(clip_for_motion(k_sparse_clips, GaitType::IDLE, true), 19U);
}

} // namespace
