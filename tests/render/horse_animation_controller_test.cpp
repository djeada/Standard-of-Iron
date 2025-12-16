#include "render/horse/horse_animation_controller.h"
#include "render/horse/rig.h"
#include "render/humanoid/rig.h"
#include <QVector3D>
#include <cmath>
#include <gtest/gtest.h>

using namespace Render::GL;

class HorseAnimationControllerTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Create a basic horse profile
    QVector3D const leather_base(0.5F, 0.4F, 0.3F);
    QVector3D const cloth_base(0.7F, 0.2F, 0.1F);
    profile = make_horse_profile(12345, leather_base, cloth_base);

    // Initialize animation inputs
    anim.time = 0.0F;
    anim.is_moving = false;
    anim.is_attacking = false;
    anim.is_melee = false;
    anim.is_in_hold_mode = false;
    anim.is_exiting_hold = false;
    anim.hold_exit_progress = 0.0F;

    // Initialize rider context
    rider_ctx = HumanoidAnimationContext{};
    rider_ctx.inputs = anim;
    rider_ctx.variation = VariationParams::fromSeed(54321);
    rider_ctx.gait.state = HumanoidMotionState::Idle;
    rider_ctx.gait.cycle_time = 0.0F;
    rider_ctx.gait.cycle_phase = 0.0F;
    rider_ctx.gait.speed = 0.0F;
    rider_ctx.gait.normalized_speed = 0.0F;
  }

  HorseProfile profile;
  AnimationInputs anim;
  HumanoidAnimationContext rider_ctx;

  // Helper to check if a float is approximately equal
  bool approxEqual(float a, float b, float epsilon = 0.01F) {
    return std::abs(a - b) < epsilon;
  }
};

TEST_F(HorseAnimationControllerTest, ConstructorInitializesCorrectly) {
  HorseAnimationController controller(profile, anim, rider_ctx);

  EXPECT_EQ(controller.get_current_phase(), 0.0F);
  EXPECT_EQ(controller.get_current_bob(), 0.0F);
  EXPECT_GT(controller.get_stride_cycle(), 0.0F);
}

TEST_F(HorseAnimationControllerTest, SetGaitUpdatesParameters) {
  HorseAnimationController controller(profile, anim, rider_ctx);

  // Test walk gait
  controller.set_gait(GaitType::WALK);
  controller.update_gait_parameters();
  EXPECT_TRUE(approxEqual(profile.gait.cycle_time, 1.1F, 0.01F));

  // Test trot gait
  controller.set_gait(GaitType::TROT);
  controller.update_gait_parameters();
  EXPECT_TRUE(approxEqual(profile.gait.cycle_time, 0.55F, 0.01F));

  // Test canter gait
  controller.set_gait(GaitType::CANTER);
  controller.update_gait_parameters();
  EXPECT_TRUE(approxEqual(profile.gait.cycle_time, 0.45F, 0.01F));

  // Test gallop gait
  controller.set_gait(GaitType::GALLOP);
  controller.update_gait_parameters();
  EXPECT_TRUE(approxEqual(profile.gait.cycle_time, 0.35F, 0.01F));
}

TEST_F(HorseAnimationControllerTest, IdleGeneratesBobbing) {
  HorseAnimationController controller(profile, anim, rider_ctx);

  controller.idle(1.0F);
  float const phase1 = controller.get_current_phase();
  float const bob1 = controller.get_current_bob();

  // Advance time
  anim.time = 1.0F;
  controller.idle(1.0F);
  float const phase2 = controller.get_current_phase();
  float const bob2 = controller.get_current_bob();

  // Phase and bob should change over time
  EXPECT_NE(phase1, phase2);
  // Bob values should be small for idle
  EXPECT_LT(std::abs(bob1), 0.01F);
  EXPECT_LT(std::abs(bob2), 0.01F);
}

TEST_F(HorseAnimationControllerTest, AccelerateChangesGait) {
  HorseAnimationController controller(profile, anim, rider_ctx);

  // Start at idle
  controller.set_gait(GaitType::IDLE);

  // Accelerate to walk speed
  controller.accelerate(2.0F);
  // Advance time to complete transition
  anim.time += 0.5F;
  controller.update_gait_parameters();
  EXPECT_TRUE(approxEqual(profile.gait.cycle_time, 1.1F, 0.01F));

  // Accelerate to trot speed
  controller.accelerate(3.0F);
  anim.time += 0.5F;
  controller.update_gait_parameters();
  EXPECT_TRUE(approxEqual(profile.gait.cycle_time, 0.55F, 0.01F));

  // Accelerate to gallop speed
  controller.accelerate(6.0F);
  anim.time += 0.5F;
  controller.update_gait_parameters();
  EXPECT_TRUE(approxEqual(profile.gait.cycle_time, 0.35F, 0.01F));
}

TEST_F(HorseAnimationControllerTest, DecelerateChangesGait) {
  HorseAnimationController controller(profile, anim, rider_ctx);

  // Start at gallop
  controller.set_gait(GaitType::GALLOP);

  // Decelerate to canter
  controller.decelerate(3.0F);
  // Advance time to complete transition
  anim.time += 0.5F;
  controller.update_gait_parameters();
  EXPECT_TRUE(approxEqual(profile.gait.cycle_time, 0.45F, 0.01F));

  // Decelerate to trot
  controller.decelerate(2.0F);
  anim.time += 0.5F;
  controller.update_gait_parameters();
  EXPECT_TRUE(approxEqual(profile.gait.cycle_time, 0.55F, 0.01F));
}

TEST_F(HorseAnimationControllerTest, TurnSetsAngles) {
  HorseAnimationController controller(profile, anim, rider_ctx);

  float const yaw = 0.5F;
  float const banking = 0.3F;

  controller.turn(yaw, banking);

  // We can't directly test internal state, but we can verify no crashes
  EXPECT_NO_THROW(controller.update_gait_parameters());
}

TEST_F(HorseAnimationControllerTest, StrafeStepModifiesPhase) {
  HorseAnimationController controller(profile, anim, rider_ctx);

  float const initial_phase = controller.get_current_phase();

  controller.strafe_step(true, 1.0F);
  float const after_left = controller.get_current_phase();

  controller.strafe_step(false, 1.0F);
  float const after_right = controller.get_current_phase();

  // Phase should change after strafe steps
  EXPECT_NE(initial_phase, after_left);
  EXPECT_NE(after_left, after_right);
}

TEST_F(HorseAnimationControllerTest, SpecialAnimationsExecuteWithoutErrors) {
  HorseAnimationController controller(profile, anim, rider_ctx);

  // Test rear
  EXPECT_NO_THROW(controller.rear(0.5F));
  EXPECT_NO_THROW(controller.rear(1.0F));

  // Test kick
  EXPECT_NO_THROW(controller.kick(true, 0.8F));
  EXPECT_NO_THROW(controller.kick(false, 0.6F));

  // Test buck
  EXPECT_NO_THROW(controller.buck(0.7F));

  // Test jump
  EXPECT_NO_THROW(controller.jump_obstacle(1.5F, 3.0F));

  // Should still update parameters without crash
  EXPECT_NO_THROW(controller.update_gait_parameters());
}

TEST_F(HorseAnimationControllerTest, StateQueriesReturnValidValues) {
  HorseAnimationController controller(profile, anim, rider_ctx);

  controller.set_gait(GaitType::TROT);
  controller.update_gait_parameters();

  float const phase = controller.get_current_phase();
  float const bob = controller.get_current_bob();
  float const stride = controller.get_stride_cycle();

  // Phase should be in [0, 1)
  EXPECT_GE(phase, 0.0F);
  EXPECT_LT(phase, 1.0F);

  // Bob should be reasonable
  EXPECT_LT(std::abs(bob), 1.0F);

  // Stride cycle should be positive
  EXPECT_GT(stride, 0.0F);
  EXPECT_LT(stride, 2.0F);
}

TEST_F(HorseAnimationControllerTest, UpdateGaitParametersWithRiderContext) {
  HorseAnimationController controller(profile, anim, rider_ctx);

  // Set rider to walking
  rider_ctx.gait.state = HumanoidMotionState::Walk;
  rider_ctx.gait.cycle_time = 0.75F;
  rider_ctx.gait.cycle_phase = 0.25F;
  rider_ctx.gait.speed = 1.5F;
  rider_ctx.gait.normalized_speed = 0.5F;

  controller.set_gait(GaitType::WALK);
  controller.update_gait_parameters();

  // Phase should match rider context
  EXPECT_TRUE(approxEqual(controller.get_current_phase(), 0.25F, 0.01F));

  // Bob should be non-zero during movement
  EXPECT_NE(controller.get_current_bob(), 0.0F);
}

TEST_F(HorseAnimationControllerTest, PhaseProgressesOverTime) {
  HorseAnimationController controller(profile, anim, rider_ctx);

  controller.set_gait(GaitType::WALK);

  anim.time = 0.0F;
  controller.update_gait_parameters();
  float const phase1 = controller.get_current_phase();

  anim.time = 0.4F;
  controller.update_gait_parameters();
  float const phase2 = controller.get_current_phase();

  anim.time = 0.8F;
  controller.update_gait_parameters();
  float const phase3 = controller.get_current_phase();

  // Phase should increase as time progresses
  EXPECT_NE(phase1, phase2);
  EXPECT_NE(phase2, phase3);
}

TEST_F(HorseAnimationControllerTest, BobIntensityAffectsIdleBob) {
  HorseAnimationController controller(profile, anim, rider_ctx);

  anim.time = 0.5F;

  controller.idle(0.5F);
  float const bob_half = std::abs(controller.get_current_bob());

  controller.idle(1.0F);
  float const bob_full = std::abs(controller.get_current_bob());

  // Full intensity should produce larger bob
  EXPECT_GE(bob_full, bob_half);
}

TEST_F(HorseAnimationControllerTest, ClampingBehaviorForSpecialAnimations) {
  HorseAnimationController controller(profile, anim, rider_ctx);

  // Test clamping for rear (should handle out-of-range values)
  EXPECT_NO_THROW(controller.rear(-0.5F));
  EXPECT_NO_THROW(controller.rear(1.5F));

  // Test clamping for kick
  EXPECT_NO_THROW(controller.kick(true, -1.0F));
  EXPECT_NO_THROW(controller.kick(false, 2.0F));

  // Test clamping for buck
  EXPECT_NO_THROW(controller.buck(-0.5F));
  EXPECT_NO_THROW(controller.buck(2.0F));

  // Test clamping for turn banking
  EXPECT_NO_THROW(controller.turn(3.14F, -2.0F));
  EXPECT_NO_THROW(controller.turn(-3.14F, 2.0F));
}

TEST_F(HorseAnimationControllerTest, GaitTransitionsAreSmoothAndGradual) {
  HorseAnimationController controller(profile, anim, rider_ctx);

  // Start at walk
  controller.set_gait(GaitType::WALK);
  float const walk_cycle = profile.gait.cycle_time;

  // Accelerate to gallop
  controller.accelerate(10.0F);

  // After short time, should be transitioning (not at final value)
  anim.time += 0.1F;
  controller.update_gait_parameters();
  float const transition_cycle1 = profile.gait.cycle_time;
  EXPECT_GT(transition_cycle1, 0.35F);      // Not yet at gallop cycle time
  EXPECT_LT(transition_cycle1, walk_cycle); // But moving toward it

  // After enough time, should reach final value
  anim.time += 0.5F;
  controller.update_gait_parameters();
  EXPECT_TRUE(approxEqual(profile.gait.cycle_time, 0.35F, 0.01F));
}
