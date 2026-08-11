#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>

#include "render/wildlife/sheep_spec.h"
#include "render/wildlife/wildlife_rig.h"

namespace {

using Render::Wildlife::RigPose;
using Render::Wildlife::SheepDrive;

constexpr std::size_t k_bone_count = 18U;

[[nodiscard]] auto
bone_lengths(const RigPose& pose) -> std::array<float, k_bone_count> {
  std::array<float, k_bone_count> lengths{};
  std::size_t next = 0;
  lengths[next++] = (pose.poll - pose.withers).length();
  lengths[next++] = (pose.muzzle - pose.poll).length();
  lengths[next++] = (pose.ear_tip_l - pose.ear_base_l).length();
  lengths[next++] = (pose.ear_tip_r - pose.ear_base_r).length();
  lengths[next++] = (pose.tail_mid - pose.tail_base).length();
  lengths[next++] = (pose.tail_tip - pose.tail_mid).length();
  for (const auto& leg : pose.legs) {
    lengths[next++] = (leg.knee - leg.shoulder).length();
    lengths[next++] = (leg.foot - leg.knee).length();
    lengths[next++] = (leg.toe - leg.foot).length();
  }
  return lengths;
}

[[nodiscard]] auto rest_pose() -> RigPose {
  SheepDrive drive;
  return Render::Wildlife::sheep_pose(drive);
}

constexpr float k_tolerance = 0.02F;

void expect_rig_holds(const SheepDrive& drive, const char* what) {
  const auto rest = bone_lengths(rest_pose());
  const auto live = bone_lengths(Render::Wildlife::sheep_pose(drive));
  static constexpr std::array<const char*, k_bone_count> k_names{"neck (withers->poll)",
                                                                 "head (poll->muzzle)",
                                                                 "left ear",
                                                                 "right ear",
                                                                 "tail base",
                                                                 "tail tip",
                                                                 "leg 0 upper",
                                                                 "leg 0 lower",
                                                                 "leg 0 hoof",
                                                                 "leg 1 upper",
                                                                 "leg 1 lower",
                                                                 "leg 1 hoof",
                                                                 "leg 2 upper",
                                                                 "leg 2 lower",
                                                                 "leg 2 hoof",
                                                                 "leg 3 upper",
                                                                 "leg 3 lower",
                                                                 "leg 3 hoof"};
  for (std::size_t i = 0; i < rest.size(); ++i) {
    if (rest[i] < 1.0e-4F) {
      continue;
    }
    const float ratio = live[i] / rest[i];
    EXPECT_NEAR(ratio, 1.0F, k_tolerance)
        << what << ": " << k_names[i] << " is " << ratio << "x its rest length";
  }
}

TEST(SheepRigIntegrityTest, NeckHoldsThroughTheGaitCycle) {
  for (int step = 0; step <= 32; ++step) {
    SheepDrive drive;
    drive.stride_phase = static_cast<float>(step) / 32.0F;
    drive.speed_ratio = 1.0F;
    expect_rig_holds(drive, "full-speed flee");
  }
}

TEST(SheepRigIntegrityTest, NeckHoldsWhileGrazing) {
  for (int step = 0; step <= 16; ++step) {
    SheepDrive drive;
    drive.stride_phase = static_cast<float>(step) / 16.0F;
    drive.graze = 1.0F;
    expect_rig_holds(drive, "grazing");
  }
}

TEST(SheepRigIntegrityTest, NeckHoldsThroughTheDeathCollapse) {
  for (int step = 0; step <= 40; ++step) {
    SheepDrive drive;
    drive.collapse = static_cast<float>(step) / 40.0F;
    drive.stride_phase = 0.37F;
    drive.speed_ratio = 0.6F;
    expect_rig_holds(drive, "death collapse");
  }
}

TEST(SheepRigIntegrityTest, HeadStaysAttachedToTheWithers) {

  const float rest_neck = bone_lengths(rest_pose())[0];
  float worst = 0.0F;
  for (int collapse = 0; collapse <= 20; ++collapse) {
    for (int step = 0; step <= 12; ++step) {
      SheepDrive drive;
      drive.collapse = static_cast<float>(collapse) / 20.0F;
      drive.stride_phase = static_cast<float>(step) / 12.0F;
      drive.speed_ratio = 1.0F;
      drive.alert = 1.0F;
      const RigPose pose = Render::Wildlife::sheep_pose(drive);
      worst =
          std::max(worst, std::abs((pose.poll - pose.withers).length() - rest_neck));
    }
  }
  EXPECT_LT(worst, rest_neck * k_tolerance)
      << "neck length drifted by " << worst << " m across the animation set";
}

} // namespace
