#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <string>

#include "animation/death_pose_manifest.h"
#include "animation/rig/pose_fk.h"

namespace {

using Animation::HumanoidDeathCollapse;
using Animation::HumanoidDeathPoseSample;
using Animation::PoseVec3;

constexpr std::array<HumanoidDeathCollapse, 4> k_collapses{
    HumanoidDeathCollapse::BackSprawl,
    HumanoidDeathCollapse::FacePlant,
    HumanoidDeathCollapse::SideCrumple,
    HumanoidDeathCollapse::MountedUnseat,
};

[[nodiscard]] auto distance(PoseVec3 a, PoseVec3 b) -> float {
  float const dx = a.x - b.x;
  float const dy = a.y - b.y;
  float const dz = a.z - b.z;
  return std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
}

[[nodiscard]] auto sample_at(HumanoidDeathCollapse collapse,
                             float phase) -> HumanoidDeathPoseSample {
  return Animation::resolve_humanoid_death_pose({.collapse = collapse, .phase = phase});
}

[[nodiscard]] auto label(HumanoidDeathCollapse collapse, float phase) -> std::string {
  return std::string(Animation::humanoid_death_collapse_name(collapse)) + " @ " +
         std::to_string(phase);
}

constexpr int k_sample_count = 41;

} // namespace

TEST(DeathCollapseTest, NoFallStretchesALimbPastItsBindLength) {
  Animation::PoseFk::HumanoidRigMetrics const rig{};

  for (auto const collapse : k_collapses) {
    for (int i = 0; i < k_sample_count; ++i) {
      float const phase =
          static_cast<float>(i) / static_cast<float>(k_sample_count - 1);
      auto const pose = sample_at(collapse, phase);
      ASSERT_TRUE(pose.active) << label(collapse, phase);

      auto const segment = [&](PoseVec3 a, PoseVec3 b, float bind, const char* name) {
        EXPECT_NEAR(distance(a, b), bind, 1.0e-3F)
            << name << " " << label(collapse, phase);
      };

      segment(pose.shoulder_l, pose.elbow_l, rig.upper_arm_len, "upper_arm_l");
      segment(pose.shoulder_r, pose.elbow_r, rig.upper_arm_len, "upper_arm_r");
      segment(pose.elbow_l, pose.hand_l, rig.fore_arm_len, "fore_arm_l");
      segment(pose.elbow_r, pose.hand_r, rig.fore_arm_len, "fore_arm_r");
      segment(pose.knee_l, pose.foot_l, rig.lower_leg_len, "lower_leg_l");
      segment(pose.knee_r, pose.foot_r, rig.lower_leg_len, "lower_leg_r");
      segment(pose.neck_base, pose.head, rig.head_rise, "neck_to_head");
      segment(pose.pelvis, pose.neck_base, rig.neck_rise, "spine");
    }
  }
}

TEST(DeathCollapseTest, NoFallDrivesAJointThroughTheGround) {
  struct JointFloor {
    const char* name;
    PoseVec3 HumanoidDeathPoseSample::*joint;
    float floor;
  };

  constexpr float k_allowance = 0.06F;
  const std::array<JointFloor, 11> joints{{
      {"pelvis", &HumanoidDeathPoseSample::pelvis, 0.10F},
      {"neck", &HumanoidDeathPoseSample::neck_base, 0.11F},
      {"head", &HumanoidDeathPoseSample::head, 0.09F},
      {"elbow_l", &HumanoidDeathPoseSample::elbow_l, 0.045F},
      {"elbow_r", &HumanoidDeathPoseSample::elbow_r, 0.045F},
      {"hand_l", &HumanoidDeathPoseSample::hand_l, 0.03F},
      {"hand_r", &HumanoidDeathPoseSample::hand_r, 0.03F},
      {"knee_l", &HumanoidDeathPoseSample::knee_l, 0.06F},
      {"knee_r", &HumanoidDeathPoseSample::knee_r, 0.06F},
      {"foot_l", &HumanoidDeathPoseSample::foot_l, 0.0F},
      {"foot_r", &HumanoidDeathPoseSample::foot_r, 0.0F},
  }};

  for (auto const collapse : k_collapses) {
    for (int i = 0; i < k_sample_count; ++i) {
      float const phase =
          static_cast<float>(i) / static_cast<float>(k_sample_count - 1);
      auto const pose = sample_at(collapse, phase);
      for (auto const& joint : joints) {
        EXPECT_GE((pose.*joint.joint).y, joint.floor - k_allowance)
            << joint.name << " " << label(collapse, phase);
      }
    }
  }
}

TEST(DeathCollapseTest, EveryFallStartsUprightAndEndsFlatOnTheGround) {
  for (auto const collapse : k_collapses) {
    auto const standing = sample_at(collapse, 0.0F);
    auto const settled = sample_at(collapse, 1.0F);

    EXPECT_GT(standing.head.y, 1.5F)
        << Animation::humanoid_death_collapse_name(collapse);
    EXPECT_GT(standing.pelvis.y, 0.9F)
        << Animation::humanoid_death_collapse_name(collapse);

    EXPECT_LT(settled.head.y, 0.30F)
        << Animation::humanoid_death_collapse_name(collapse);
    EXPECT_LT(settled.pelvis.y, 0.30F)
        << Animation::humanoid_death_collapse_name(collapse);

    float const trunk_rise = settled.neck_base.y - settled.pelvis.y;
    float const trunk_len = distance(settled.neck_base, settled.pelvis);
    EXPECT_LT(std::abs(trunk_rise / trunk_len), 0.43F)
        << Animation::humanoid_death_collapse_name(collapse);
  }
}

TEST(DeathCollapseTest, TheHeldCorpsePoseIsTheLastFrameOfTheFall) {
  for (auto const collapse : k_collapses) {
    auto const settled = sample_at(collapse, 1.0F);
    auto const past_end = sample_at(collapse, 1.4F);
    EXPECT_NEAR(distance(settled.head, past_end.head), 0.0F, 1.0e-5F)
        << Animation::humanoid_death_collapse_name(collapse);
    EXPECT_NEAR(distance(settled.pelvis, past_end.pelvis), 0.0F, 1.0e-5F)
        << Animation::humanoid_death_collapse_name(collapse);
  }
}

TEST(DeathCollapseTest, TheThreeInfantryFallsEndInDifferentPlaces) {
  auto const back = sample_at(Animation::humanoid_infantry_death_collapse(0), 1.0F);
  auto const face = sample_at(Animation::humanoid_infantry_death_collapse(1), 1.0F);
  auto const side = sample_at(Animation::humanoid_infantry_death_collapse(2), 1.0F);

  EXPECT_GT(distance(back.head, face.head), 1.0F);
  EXPECT_GT(distance(back.head, side.head), 0.3F);
  EXPECT_GT(distance(face.head, side.head), 1.0F);

  EXPECT_LT(back.head.z, back.pelvis.z);
  EXPECT_GT(face.head.z, face.pelvis.z);
}

TEST(DeathCollapseTest, VariantsMapOntoTheThreeInfantryFalls) {
  EXPECT_EQ(Animation::humanoid_infantry_death_collapse(0),
            HumanoidDeathCollapse::BackSprawl);
  EXPECT_EQ(Animation::humanoid_infantry_death_collapse(1),
            HumanoidDeathCollapse::FacePlant);
  EXPECT_EQ(Animation::humanoid_infantry_death_collapse(2),
            HumanoidDeathCollapse::SideCrumple);

  for (std::uint8_t v = 0U; v < 16U; ++v) {
    EXPECT_NE(Animation::humanoid_infantry_death_collapse(v),
              HumanoidDeathCollapse::None);
  }
}

TEST(DeathCollapseTest, NoCollapseIsInert) {
  auto const none = sample_at(HumanoidDeathCollapse::None, 0.5F);
  EXPECT_FALSE(none.active);
}
