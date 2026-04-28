#include "render/creature/quadruped/clip_set.h"

#include <gtest/gtest.h>

namespace {

using Render::Creature::Quadruped::clip_for_gait;
using Render::Creature::Quadruped::clip_for_motion;
using Render::Creature::Quadruped::ClipSet;
using Render::GL::GaitType;

TEST(QuadrupedClipSet, HorseGaitsMapDirectly) {
  constexpr ClipSet kHorseClips{0U, 1U, 2U, 3U, 4U, 5U};

  EXPECT_EQ(clip_for_gait(kHorseClips, GaitType::IDLE), 0U);
  EXPECT_EQ(clip_for_gait(kHorseClips, GaitType::WALK), 1U);
  EXPECT_EQ(clip_for_gait(kHorseClips, GaitType::TROT), 2U);
  EXPECT_EQ(clip_for_gait(kHorseClips, GaitType::CANTER), 3U);
  EXPECT_EQ(clip_for_gait(kHorseClips, GaitType::GALLOP), 4U);
  EXPECT_EQ(clip_for_motion(kHorseClips, GaitType::TROT, true), 5U);
}

TEST(QuadrupedClipSet, RunningFallsBackWhenFasterClipsAreMissing) {
  constexpr ClipSet kElephantClips{0U,
                                   1U,
                                   2U,
                                   Render::Creature::Quadruped::kInvalidClip,
                                   Render::Creature::Quadruped::kInvalidClip,
                                   3U};

  EXPECT_EQ(clip_for_motion(kElephantClips, false, false, false), 0U);
  EXPECT_EQ(clip_for_motion(kElephantClips, true, false, false), 1U);
  EXPECT_EQ(clip_for_motion(kElephantClips, true, true, false), 2U);
  EXPECT_EQ(clip_for_motion(kElephantClips, true, true, true), 3U);
}

TEST(QuadrupedClipSet, GaitFallbackWalksBackTowardAvailableLocomotion) {
  constexpr ClipSet kSparseClips{10U,
                                 Render::Creature::Quadruped::kInvalidClip,
                                 12U,
                                 Render::Creature::Quadruped::kInvalidClip,
                                 Render::Creature::Quadruped::kInvalidClip,
                                 19U};

  EXPECT_EQ(clip_for_gait(kSparseClips, GaitType::WALK), 12U);
  EXPECT_EQ(clip_for_gait(kSparseClips, GaitType::GALLOP), 12U);
  EXPECT_EQ(clip_for_motion(kSparseClips, GaitType::IDLE, true), 19U);
}

} // namespace
