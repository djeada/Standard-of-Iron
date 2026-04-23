#include "render/elephant/rig.h"
#include "render/gl/humanoid/humanoid_types.h"
#include <QVector3D>
#include <cmath>
#include <gtest/gtest.h>

using namespace Render::GL;

namespace {

auto make_default_anim(float time, bool moving) -> AnimationInputs {
  AnimationInputs anim{};
  anim.time = time;
  anim.is_moving = moving;
  anim.is_running = false;
  anim.is_attacking = false;
  anim.is_melee = false;
  anim.is_in_hold_mode = false;
  anim.is_exiting_hold = false;
  anim.hold_exit_progress = 0.0F;
  anim.combat_phase = CombatAnimPhase::Idle;
  anim.combat_phase_progress = 0.0F;
  return anim;
}

} // namespace

TEST(ElephantMotionProfileTest, DimensionsHaveHeavierAndGroundedProportions) {
  for (uint32_t seed = 1; seed <= 32; ++seed) {
    ElephantDimensions const dims = make_elephant_dimensions(seed);

    float const width_height_ratio = dims.body_width / dims.body_height;
    EXPECT_GE(width_height_ratio, 0.85F);
    EXPECT_LE(width_height_ratio, 1.30F);

    float const leg_bulk_ratio = dims.leg_radius / dims.body_width;
    EXPECT_GE(leg_bulk_ratio, 0.25F);
    EXPECT_LE(leg_bulk_ratio, 0.45F);

    EXPECT_GT(dims.trunk_length, dims.head_length * 2.3F);
    EXPECT_GT(dims.foot_radius, dims.leg_radius * 0.80F);
  }
}

TEST(ElephantMotionProfileTest, MovingMotionAddsWeightAndBodyTransfer) {
  QVector3D const fabric_base(0.45F, 0.25F, 0.20F);
  QVector3D const metal_base(0.62F, 0.60F, 0.55F);
  ElephantProfile profile = make_elephant_profile(42U, fabric_base, metal_base);

  AnimationInputs const anim = make_default_anim(1.2F, true);
  ElephantMotionSample const sample = evaluate_elephant_motion(profile, anim);

  EXPECT_TRUE(sample.is_moving);
  EXPECT_GE(sample.phase, 0.0F);
  EXPECT_LT(sample.phase, 1.0F);

  EXPECT_GE(sample.weight_compression, 0.0F);
  EXPECT_GT(std::abs(sample.body_pitch), 0.01F);
  EXPECT_GT(std::abs(sample.body_roll), 0.01F);
  EXPECT_LE(std::abs(sample.bob), profile.dims.move_bob_amplitude * 1.3F);
}

TEST(ElephantMotionProfileTest, IdleMotionStaysCalmComparedToMoving) {
  QVector3D const fabric_base(0.50F, 0.20F, 0.18F);
  QVector3D const metal_base(0.60F, 0.58F, 0.52F);
  ElephantProfile profile = make_elephant_profile(1337U, fabric_base, metal_base);

  ElephantMotionSample const idle =
      evaluate_elephant_motion(profile, make_default_anim(2.1F, false));
  ElephantMotionSample const moving =
      evaluate_elephant_motion(profile, make_default_anim(2.1F, true));

  EXPECT_FALSE(idle.is_moving);
  EXPECT_FLOAT_EQ(idle.weight_compression, 0.0F);
  EXPECT_LT(std::abs(idle.ear_flap), std::abs(moving.ear_flap));
  EXPECT_LT(std::abs(idle.body_roll), std::abs(moving.body_roll));
}
