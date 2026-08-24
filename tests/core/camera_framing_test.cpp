#include <QVector3D>

#include <gtest/gtest.h>

#include "game/camera_framing.h"
#include "game/game_config.h"
#include "game/render_bridge/camera_service.h"
#include "game/session/session_context.h"
#include "scene/camera.h"

namespace {

void settle(Render::GL::Camera& camera) {
  camera.update(10.0F);
}

auto height_above_target(const Render::GL::Camera& camera) -> float {
  return camera.get_position().y() - camera.get_target().y();
}

class CameraFramingTest : public ::testing::Test {
protected:
  void TearDown() override { Game::GameConfig::instance().clear_authored_camera(); }
};

TEST_F(CameraFramingTest, AResetPullsTheAuthoredViewInWithoutLosingTheScale) {

  EXPECT_NEAR(Game::reset_framing({.distance = 273.0F}).distance, 91.0F, 0.01F);
  EXPECT_NEAR(Game::reset_framing({.distance = 336.0F}).distance, 112.0F, 0.01F);
  EXPECT_NEAR(Game::reset_framing({.distance = 82.0F}).distance, 27.33F, 0.01F);
}

TEST_F(CameraFramingTest, SmallMapsResetNoCloserThanAFormationIsWide) {
  EXPECT_NEAR(Game::reset_framing({.distance = 30.0F}).distance,
              Game::k_min_reset_distance,
              0.01F);
  EXPECT_NEAR(Game::reset_framing({.distance = 28.0F}).distance,
              Game::k_min_reset_distance,
              0.01F);
}

TEST_F(CameraFramingTest, AMapAuthoredCloserThanTheFloorKeepsItsOwnFraming) {

  EXPECT_NEAR(Game::reset_framing({.distance = 18.0F}).distance, 18.0F, 0.01F);
}

TEST_F(CameraFramingTest, TheTiltAndSwingOfTheMissionSurviveAReset) {
  const auto framing =
      Game::reset_framing({.distance = 273.0F, .pitch = 50.0F, .yaw = 210.0F});

  EXPECT_FLOAT_EQ(framing.pitch, 50.0F);
  EXPECT_FLOAT_EQ(framing.yaw, 210.0F);
}

TEST_F(CameraFramingTest, WithNoMapLoadedTheResetFallsBackToTheBuiltInDefault) {
  auto& config = Game::GameConfig::instance();
  config.clear_authored_camera();

  const auto framing = config.camera_reset_framing();

  EXPECT_FLOAT_EQ(framing.distance, config.get_camera_default_distance());
  EXPECT_FLOAT_EQ(framing.pitch, config.get_camera_default_pitch());
  EXPECT_FLOAT_EQ(framing.yaw, config.get_camera_default_yaw());
}

TEST_F(CameraFramingTest, LoadingAMapMakesTheResetFollowThatMapRatherThanTheDefault) {
  auto& config = Game::GameConfig::instance();
  config.set_authored_camera({.distance = 273.0F, .pitch = 48.0F, .yaw = 225.0F});

  const auto framing = config.camera_reset_framing();

  EXPECT_GT(framing.distance, config.get_camera_default_distance());
  EXPECT_NEAR(framing.distance, 91.0F, 0.01F);
  EXPECT_FLOAT_EQ(framing.pitch, 48.0F);
}

TEST_F(CameraFramingTest, TiltingUpRaisesTheCameraAndTiltingDownLowersIt) {
  Game::Systems::CameraService service(
      Game::Session::SessionContext::active().visibility());
  Render::GL::Camera camera;
  camera.set_rts_view(QVector3D(0.0F, 0.0F, 0.0F), 40.0F, 48.0F, 225.0F);
  const float opening_height = height_above_target(camera);

  service.tilt(camera, 1, false);
  settle(camera);
  const float raised = height_above_target(camera);
  EXPECT_GT(raised, opening_height)
      << "tilt up must climb towards an overhead view, not dive to the horizon";

  service.tilt(camera, -1, false);
  settle(camera);
  service.tilt(camera, -1, false);
  settle(camera);
  EXPECT_LT(height_above_target(camera), opening_height)
      << "tilt down must drop towards the horizon";
}

TEST_F(CameraFramingTest, HoldingShiftTiltsFurtherInTheSameDirection) {
  Game::Systems::CameraService service(
      Game::Session::SessionContext::active().visibility());
  Render::GL::Camera plain;
  Render::GL::Camera shifted;
  plain.set_rts_view(QVector3D(0.0F, 0.0F, 0.0F), 40.0F, 48.0F, 225.0F);
  shifted.set_rts_view(QVector3D(0.0F, 0.0F, 0.0F), 40.0F, 48.0F, 225.0F);

  service.tilt(plain, 1, false);
  service.tilt(shifted, 1, true);
  settle(plain);
  settle(shifted);

  EXPECT_GT(height_above_target(shifted), height_above_target(plain));
}

} // namespace
