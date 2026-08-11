#include <array>
#include <cmath>
#include <gtest/gtest.h>

#include "render/wildlife/wildlife_rig.h"
#include "render/wildlife/wolf_spec.h"

namespace {

using Render::Wildlife::RigPose;
using Render::Wildlife::WolfDrive;
using Render::Wildlife::WolfGait;

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
  WolfDrive drive;
  return Render::Wildlife::wolf_pose(drive);
}

constexpr float k_tolerance = 0.02F;

void expect_rig_holds(const WolfDrive& drive, const char* what) {
  const auto rest = bone_lengths(rest_pose());
  const auto live = bone_lengths(Render::Wildlife::wolf_pose(drive));
  static constexpr std::array<const char*, k_bone_count> k_names{"neck (withers->poll)",
                                                                 "head (poll->muzzle)",
                                                                 "left ear",
                                                                 "right ear",
                                                                 "tail base",
                                                                 "tail tip",
                                                                 "leg 0 upper",
                                                                 "leg 0 lower",
                                                                 "leg 0 paw",
                                                                 "leg 1 upper",
                                                                 "leg 1 lower",
                                                                 "leg 1 paw",
                                                                 "leg 2 upper",
                                                                 "leg 2 lower",
                                                                 "leg 2 paw",
                                                                 "leg 3 upper",
                                                                 "leg 3 lower",
                                                                 "leg 3 paw"};
  for (std::size_t i = 0; i < rest.size(); ++i) {
    if (rest[i] < 1.0e-4F) {
      continue;
    }
    const float ratio = live[i] / rest[i];
    EXPECT_NEAR(ratio, 1.0F, k_tolerance)
        << what << ": " << k_names[i] << " is " << ratio << "x its rest length";
  }
}

} // namespace

TEST(WolfRigIntegrityTest, BonesHoldThroughTheRunCycle) {
  for (int step = 0; step <= 32; ++step) {
    WolfDrive drive;
    drive.stride_phase = static_cast<float>(step) / 32.0F;
    drive.speed_ratio = 1.0F;
    drive.gait = WolfGait::Run;
    expect_rig_holds(drive, "full-speed run");
  }
}

TEST(WolfRigIntegrityTest, BonesHoldWhileLungingWithOpenJaw) {
  for (int step = 0; step <= 16; ++step) {
    WolfDrive drive;
    drive.stride_phase = static_cast<float>(step) / 16.0F;
    drive.lunge = 1.0F;
    drive.jaw_open = 1.0F;
    expect_rig_holds(drive, "lunge");
  }
}

TEST(WolfRigIntegrityTest, BonesHoldThroughTheDeathCollapse) {
  for (int step = 0; step <= 40; ++step) {
    WolfDrive drive;
    drive.collapse = static_cast<float>(step) / 40.0F;
    drive.stride_phase = 0.37F;
    drive.speed_ratio = 0.6F;
    expect_rig_holds(drive, "death collapse");
  }
}

TEST(WolfRigIntegrityTest, BonesHoldAcrossCollapseAndStride) {
  for (int collapse = 0; collapse <= 20; ++collapse) {
    for (int step = 0; step <= 12; ++step) {
      WolfDrive drive;
      drive.collapse = static_cast<float>(collapse) / 20.0F;
      drive.stride_phase = static_cast<float>(step) / 12.0F;
      drive.speed_ratio = 0.8F;
      expect_rig_holds(drive, "collapse while running");
    }
  }
}
