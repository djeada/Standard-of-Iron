#include <gtest/gtest.h>

#include "animation/rig/mounted_seat.h"
#include "render/entity/mounted_knight_pose.h"
#include "render/equipment/weapons/spear_renderer.h"
#include "render/horse/dimensions.h"
#include "render/horse/horse_motion.h"

namespace {

// Combat hit detection consumes frozen copies of these rig values so the
// simulation can run without the renderer (tools/balance_sim is explicitly
// headless).  Freezing was behaviour-neutral because the trace always used a
// default-seeded horse and a default spear config rather than the per-entity
// rig.
//
// This test is the safety net for that decision: it recomputes the values from
// the live rig and fails if they drift.  A failure is not a bug in itself -- it
// means the rig changed and someone must decide whether combat reach should
// follow it.  Re-freeze animation/rig/mounted_seat.h deliberately, do not
// silently relax the tolerance.
constexpr float k_tolerance = 1.0e-4F;

} // namespace

TEST(MountedSeatFrame, FrozenGameplayValuesMatchTheRig) {
  auto profile = Render::GL::make_horse_profile(0U, {}, {});
  auto mount = Render::GL::compute_mount_frame(profile);
  Render::GL::tune_mounted_knight_frame(profile.dims, mount);

  using namespace Animation::Rig::MountedSeat;

  EXPECT_NEAR(mount.seat_position.x(), position.x(), k_tolerance);
  EXPECT_NEAR(mount.seat_position.y(), position.y(), k_tolerance);
  EXPECT_NEAR(mount.seat_position.z(), position.z(), k_tolerance);

  EXPECT_NEAR(mount.seat_forward.x(), forward.x(), k_tolerance);
  EXPECT_NEAR(mount.seat_forward.y(), forward.y(), k_tolerance);
  EXPECT_NEAR(mount.seat_forward.z(), forward.z(), k_tolerance);

  EXPECT_NEAR(mount.seat_right.x(), right.x(), k_tolerance);
  EXPECT_NEAR(mount.seat_right.y(), right.y(), k_tolerance);
  EXPECT_NEAR(mount.seat_right.z(), right.z(), k_tolerance);

  EXPECT_NEAR(mount.seat_up.x(), up.x(), k_tolerance);
  EXPECT_NEAR(mount.seat_up.y(), up.y(), k_tolerance);
  EXPECT_NEAR(mount.seat_up.z(), up.z(), k_tolerance);
}

TEST(MountedSeatFrame, FrozenSpearReachMatchesTheRenderConfig) {
  Render::GL::SpearRenderConfig const config{};

  EXPECT_NEAR(config.spear_length, Animation::Rig::WeaponReach::spear_shaft,
              k_tolerance);
  EXPECT_NEAR(config.spearhead_length, Animation::Rig::WeaponReach::spear_head,
              k_tolerance);
  EXPECT_NEAR(config.spear_length + config.spearhead_length,
              Animation::Rig::WeaponReach::spear_total,
              k_tolerance);
}
