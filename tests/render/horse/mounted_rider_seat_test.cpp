#include <QVector3D>

#include <cmath>
#include <gtest/gtest.h>
#include <iostream>

#include "animation/rig/humanoid_proportions.h"
#include "render/horse/dimensions.h"
#include "render/horse/horse_motion.h"
#include "render/horse/horse_profile_data.h"
#include "render/horse/horse_source_asset.h"

namespace {

using Render::GL::HumanProportions;

// The rider is posed from the mount frame, so how natural the seat looks is
// decided by where these points sit on the horse. Pinning the relationships
// keeps a horse resize from quietly leaving the rider hovering or splayed.
TEST(MountedRiderSeatTest, SeatAndStirrupsSitOnTheHorse) {
  auto const profile = Render::GL::make_horse_profile(
      0U, QVector3D(0.4F, 0.3F, 0.2F), QVector3D(0.5F, 0.5F, 0.5F));
  auto const mount = Render::GL::compute_mount_frame(profile);
  auto const& dims = profile.dims;

  std::cout << "saddle_center y=" << mount.saddle_center.y()
            << "  seat y=" << mount.seat_position.y()
            << "  barrel_center_y=" << dims.barrel_center_y
            << "  saddle_height=" << dims.saddle_height << "\n"
            << "stirrup_attach y=" << mount.stirrup_attach_left.y()
            << "  stirrup_bottom y=" << mount.stirrup_bottom_left.y()
            << "  x=" << mount.stirrup_bottom_left.x()
            << "  z=" << mount.stirrup_bottom_left.z() << "\n"
            << "body_width=" << dims.body_width
            << "  stirrup_out=" << dims.stirrup_out
            << "  stirrup_drop=" << dims.stirrup_drop << "\n";

  // The seat has to be above the barrel, and the stirrups below the seat.
  EXPECT_GT(mount.seat_position.y(), dims.barrel_center_y);
  EXPECT_LT(mount.stirrup_bottom_left.y(), mount.seat_position.y());
  EXPECT_LT(mount.stirrup_bottom_left.y(), mount.stirrup_attach_left.y());

  // Stirrups hang outside the barrel, mirrored about the spine.
  EXPECT_LT(mount.stirrup_bottom_left.x(), 0.0F);
  EXPECT_GT(mount.stirrup_bottom_right.x(), 0.0F);
  EXPECT_NEAR(mount.stirrup_bottom_left.x(), -mount.stirrup_bottom_right.x(), 1.0e-4F);
  EXPECT_GT(std::abs(mount.stirrup_bottom_left.x()), dims.body_width * 0.5F);

  // A rider's leg only reaches so far. The drop from seat to stirrup should be
  // in the range a bent leg can cover, not a full standing leg length.
  float const seat_to_stirrup = mount.seat_position.y() - mount.stirrup_bottom_left.y();
  float const straight_leg = HumanProportions::UPPER_LEG_LEN +
                             HumanProportions::LOWER_LEG_LEN;
  EXPECT_GT(seat_to_stirrup, straight_leg * 0.35F)
      << "stirrups are tucked up under the rider";
  EXPECT_LT(seat_to_stirrup, straight_leg * 0.95F)
      << "rider cannot reach the stirrups without straightening the leg";
}

// Cavalry and infantry share a world, so the mounted rider's eye level has to
// stay in a believable band above a man on foot.
TEST(MountedRiderSeatTest, RiderSitsAtABelievableHeight) {
  auto const profile = Render::GL::make_horse_profile(
      0U, QVector3D(0.4F, 0.3F, 0.2F), QVector3D(0.5F, 0.5F, 0.5F));
  auto const mount = Render::GL::compute_mount_frame(profile);

  // Seated, a rider's head sits roughly a torso above the saddle.
  float const seated_head_y =
      mount.seat_position.y() +
      (HumanProportions::HEAD_TOP_Y - HumanProportions::WAIST_Y);
  float const versus_footman = seated_head_y / HumanProportions::TOTAL_HEIGHT;
  std::cout << "seat y=" << mount.seat_position.y()
            << "  seated head y=" << seated_head_y << "  ratio=" << versus_footman
            << "\n";
  EXPECT_GT(versus_footman, 1.15F) << "rider is not meaningfully above the infantry";
  EXPECT_LT(versus_footman, 1.60F) << "rider towers over the infantry";
}

} // namespace
