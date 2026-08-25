#include <cmath>
#include <gtest/gtest.h>

#include "app/commander/commander_camera_rig.h"
#include "app/commander/rts_camera_bookmark.h"
#include "core/component.h"
#include "game/accessibility/commander_input_settings.h"
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

TEST(CommanderCameraRig, ImpactKickRespectsItsAmplitudeAndVelocityBudget) {
  CommanderCameraRig rig;
  Render::GL::Camera camera;

  auto inputs = default_inputs();
  settle(rig, camera, inputs);
  float const rest_fov = rig.fov();

  rig.add_impact_kick(20.0F);
  float previous = rest_fov;
  float peak = rest_fov;
  for (int frame = 0; frame < 60; ++frame) {
    rig.update(camera, inputs);
    float const now = rig.fov();
    EXPECT_LE(std::abs(now - previous), (45.0F * inputs.dt) + 1.0e-3F)
        << "the impulse may not move the FOV faster than its velocity budget";
    peak = std::max(peak, now);
    previous = now;
  }

  EXPECT_LE(peak - rest_fov, 3.0F + 1.0e-3F)
      << "an oversized impact must still clamp to the authored amplitude budget";
}

TEST(CommanderCameraRig, ImpactKickIsOffWhenTheCameraImpulseIsDisabled) {
  Game::Accessibility::CommanderInput::reset_to_defaults();
  Game::Accessibility::CommanderInput::set_camera_impulse_enabled(false);

  CommanderCameraRig rig;
  Render::GL::Camera camera;
  auto inputs = default_inputs();
  settle(rig, camera, inputs);
  float const rest_fov = rig.fov();

  rig.add_impact_kick(1.0F);
  rig.update(camera, inputs);
  EXPECT_NEAR(rig.fov(), rest_fov, 0.05F);

  Game::Accessibility::CommanderInput::reset_to_defaults();
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

namespace {

auto rig_inputs_at(float yaw,
                   const QVector3D& position,
                   float dt) -> App::Core::CommanderCameraInputs {
  App::Core::CommanderCameraInputs inputs;
  inputs.dt = dt;
  inputs.view_yaw_degrees = yaw;
  inputs.view_pitch_degrees = 0.0F;
  inputs.commander_position = position;
  return inputs;
}

} // namespace

TEST(CommanderCameraRigFeelTest, MouseRotationIsAppliedWithoutSmoothingLag) {
  App::Core::CommanderCameraRig rig;
  Render::GL::Camera camera;
  const QVector3D anchor(0.0F, 0.0F, 0.0F);

  for (int settle = 0; settle < 240; ++settle) {
    static_cast<void>(rig.update(camera, rig_inputs_at(0.0F, anchor, 1.0F / 60.0F)));
  }
  const QVector3D settled_eye = camera.get_position();

  static_cast<void>(rig.update(camera, rig_inputs_at(90.0F, anchor, 1.0F / 60.0F)));
  const QVector3D turned_eye = camera.get_position();

  EXPECT_GT((turned_eye - settled_eye).length(), 2.0F)
      << "a 90 degree turn barely moved the eye, so rotation is still being smoothed";

  static_cast<void>(rig.update(camera, rig_inputs_at(90.0F, anchor, 1.0F / 60.0F)));
  EXPECT_LT((camera.get_position() - turned_eye).length(), 0.05F)
      << "the eye kept drifting after the turn finished";
}

TEST(CommanderCameraRigFeelTest, TheVisualAnchorLagsTheSimulationPositionButIsBounded) {
  App::Core::CommanderCameraRig rig;
  Render::GL::Camera camera;

  static_cast<void>(rig.update(
      camera, rig_inputs_at(0.0F, QVector3D(0.0F, 0.0F, 0.0F), 1.0F / 60.0F)));
  EXPECT_TRUE(rig.state().anchor_valid);
  EXPECT_FLOAT_EQ(rig.state().visual_anchor.x(), 0.0F);

  static_cast<void>(rig.update(
      camera, rig_inputs_at(0.0F, QVector3D(4.0F, 0.0F, 0.0F), 1.0F / 60.0F)));
  const float lagged_x = rig.state().visual_anchor.x();
  EXPECT_GT(lagged_x, 0.0F) << "the anchor never followed the commander";
  EXPECT_LT(lagged_x, 4.0F) << "the anchor snapped instead of easing";
  EXPECT_GE(lagged_x, 4.0F - 0.31F) << "the anchor lagged further than the cap allows";

  for (int frame = 0; frame < 120; ++frame) {
    static_cast<void>(rig.update(
        camera, rig_inputs_at(0.0F, QVector3D(4.0F, 0.0F, 0.0F), 1.0F / 60.0F)));
  }
  EXPECT_NEAR(rig.state().visual_anchor.x(), 4.0F, 0.01F);
}

TEST(CommanderCameraRigFeelTest, LookVelocityIsReportedForTheFrame) {
  App::Core::CommanderCameraRig rig;
  Render::GL::Camera camera;
  const QVector3D anchor(0.0F, 0.0F, 0.0F);

  static_cast<void>(rig.update(camera, rig_inputs_at(0.0F, anchor, 1.0F / 60.0F)));
  static_cast<void>(rig.update(camera, rig_inputs_at(6.0F, anchor, 1.0F / 60.0F)));
  EXPECT_NEAR(rig.state().yaw_velocity, 360.0F, 1.0F);
  EXPECT_FLOAT_EQ(rig.state().yaw, 6.0F);

  static_cast<void>(rig.update(camera, rig_inputs_at(6.0F, anchor, 1.0F / 60.0F)));
  EXPECT_FLOAT_EQ(rig.state().yaw_velocity, 0.0F);
}

TEST(CommanderCameraRigFeelTest, YawVelocityCrossesTheSeamWithoutASpike) {
  App::Core::CommanderCameraRig rig;
  Render::GL::Camera camera;
  const QVector3D anchor(0.0F, 0.0F, 0.0F);

  static_cast<void>(rig.update(camera, rig_inputs_at(358.0F, anchor, 1.0F / 60.0F)));
  static_cast<void>(rig.update(camera, rig_inputs_at(2.0F, anchor, 1.0F / 60.0F)));
  EXPECT_NEAR(rig.state().yaw_velocity, 240.0F, 1.0F);
}

TEST(RtsCameraBookmarkTest, EnteringAndLeavingCommanderModeKeepsTheStrategicView) {
  Render::GL::Camera rts;
  rts.set_perspective(52.0F, 16.0F / 9.0F, 0.2F, 240.0F);
  rts.look_at(QVector3D(12.0F, 20.0F, -8.0F),
              QVector3D(12.0F, 0.0F, 4.0F),
              QVector3D(0.0F, 1.0F, 0.0F));

  const auto bookmark = App::Core::RtsCameraBookmark::capture(rts);
  ASSERT_TRUE(bookmark.valid);

  rts.look_at(QVector3D(0.0F, 2.0F, 0.0F),
              QVector3D(0.0F, 2.0F, 1.0F),
              QVector3D(0.0F, 1.0F, 0.0F));
  rts.set_perspective(68.0F, 16.0F / 9.0F, 0.05F, 200.0F);

  bookmark.restore(rts);
  EXPECT_NEAR(rts.get_position().x(), 12.0F, 1.0e-4F);
  EXPECT_NEAR(rts.get_position().y(), 20.0F, 1.0e-4F);
  EXPECT_NEAR(rts.get_position().z(), -8.0F, 1.0e-4F);
  EXPECT_NEAR(rts.get_target().z(), 4.0F, 1.0e-4F);
  EXPECT_NEAR(rts.get_fov(), 52.0F, 1.0e-4F);
  EXPECT_NEAR(rts.get_far(), 240.0F, 1.0e-4F);
}

TEST(RtsCameraBookmarkTest, AnEmptyBookmarkLeavesTheCameraAlone) {
  Render::GL::Camera camera;
  camera.look_at(QVector3D(3.0F, 4.0F, 5.0F),
                 QVector3D(0.0F, 0.0F, 0.0F),
                 QVector3D(0.0F, 1.0F, 0.0F));

  const App::Core::RtsCameraBookmark empty;
  empty.restore(camera);

  EXPECT_NEAR(camera.get_position().x(), 3.0F, 1.0e-4F);
  EXPECT_NEAR(camera.get_position().z(), 5.0F, 1.0e-4F);
}
