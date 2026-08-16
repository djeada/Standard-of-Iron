#include <gtest/gtest.h>

#include "app/commander/commander_camera_rig.h"
#include "core/component.h"
#include "scene/camera.h"

using App::Core::CommanderCameraInputs;
using App::Core::CommanderCameraRig;
using App::Core::CommanderFramingState;
using Engine::Core::FightContext;

namespace {

auto default_inputs() -> CommanderCameraInputs {
  CommanderCameraInputs inputs;
  inputs.dt = 1.0F / 60.0F;
  inputs.fight_context = FightContext::None;
  return inputs;
}

void settle(CommanderCameraRig& rig,
            Render::GL::Camera& camera,
            const CommanderCameraInputs& inputs,
            int steps = 240) {
  for (int i = 0; i < steps; ++i) {
    rig.update(camera, inputs);
  }
}

} // namespace

TEST(CommanderCameraRig, BowAimOverridesEveryOtherFraming) {
  EXPECT_EQ(CommanderCameraRig::select_framing(true, true, FightContext::Duel),
            CommanderFramingState::BowAim);
  EXPECT_EQ(CommanderCameraRig::select_framing(true, false, FightContext::None),
            CommanderFramingState::BowAim);
}

TEST(CommanderCameraRig, DuelLockNeedsBothALockAndADuelContext) {
  EXPECT_EQ(CommanderCameraRig::select_framing(false, true, FightContext::Duel),
            CommanderFramingState::DuelLock);
  EXPECT_EQ(CommanderCameraRig::select_framing(false, false, FightContext::Duel),
            CommanderFramingState::Melee);
  EXPECT_EQ(CommanderCameraRig::select_framing(false, true, FightContext::Skirmish),
            CommanderFramingState::Melee);
}

TEST(CommanderCameraRig, NoRingMeansExploreFraming) {
  EXPECT_EQ(CommanderCameraRig::select_framing(false, false, FightContext::None),
            CommanderFramingState::Explore);
}

TEST(CommanderCameraRig, AimingTightensTheFieldOfView) {
  CommanderCameraRig rig;
  Render::GL::Camera camera;

  auto inputs = default_inputs();
  settle(rig, camera, inputs);
  float const hip_fov = rig.fov();

  inputs.aiming_bow = true;
  settle(rig, camera, inputs);
  float const aim_fov = rig.fov();

  EXPECT_GT(hip_fov, 60.0F);
  EXPECT_LT(aim_fov, 55.0F);
  EXPECT_GT(rig.aim_blend(), 0.9F);
  EXPECT_EQ(rig.framing_state(), CommanderFramingState::BowAim);
}

TEST(CommanderCameraRig, ImpactKickDecaysBackToRest) {
  CommanderCameraRig rig;
  Render::GL::Camera camera;

  auto inputs = default_inputs();
  settle(rig, camera, inputs);
  float const rest_fov = rig.fov();

  rig.add_impact_kick(1.0F);
  rig.update(camera, inputs);
  float const kicked_fov = rig.fov();
  EXPECT_GT(kicked_fov, rest_fov);

  settle(rig, camera, inputs);
  EXPECT_NEAR(rig.fov(), rest_fov, 0.1F);
}

TEST(CommanderCameraRig, MeleeFramingLooksDownAtTheFightButAimingDoesNot) {
  CommanderCameraRig melee_rig;
  CommanderCameraRig explore_rig;
  Render::GL::Camera camera;

  auto explore = default_inputs();
  settle(explore_rig, camera, explore);

  auto melee = default_inputs();
  melee.fight_context = FightContext::Skirmish;
  settle(melee_rig, camera, melee);

  ASSERT_EQ(melee_rig.framing_state(), CommanderFramingState::Melee);
  EXPECT_LT(melee_rig.forward().y(), explore_rig.forward().y())
      << "melee framing must tilt toward the fight so it fills the frame";

  CommanderCameraRig aim_rig;
  auto aiming = default_inputs();
  aiming.fight_context = FightContext::Skirmish;
  aiming.aiming_bow = true;
  settle(aim_rig, camera, aiming);

  ASSERT_EQ(aim_rig.framing_state(), CommanderFramingState::BowAim);
  EXPECT_NEAR(aim_rig.forward().y(), explore_rig.forward().y(), 0.02F)
      << "the bow reticle is the camera axis; aiming may never be tilted off it";
}

TEST(CommanderCameraRig, ResetClearsSmoothedState) {
  CommanderCameraRig rig;
  Render::GL::Camera camera;

  auto inputs = default_inputs();
  inputs.move_speed = 2.0F;
  settle(rig, camera, inputs, 30);
  EXPECT_TRUE(rig.eye_valid());

  rig.reset();
  EXPECT_FALSE(rig.eye_valid());
  EXPECT_EQ(rig.aim_blend(), 0.0F);
  EXPECT_EQ(rig.bob_amplitude(), 0.0F);
  EXPECT_EQ(rig.framing_state(), CommanderFramingState::Explore);
}
