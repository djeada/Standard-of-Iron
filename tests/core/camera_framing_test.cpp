#include <QVector3D>

#include <gtest/gtest.h>

#include "game/camera_framing.h"
#include "game/game_config.h"
#include "game/map/map_loader.h"
#include "game/map/terrain_service.h"
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
  void SetUp() override { Game::Session::SessionContext::active().reset(); }

  void TearDown() override {
    Game::GameConfig::instance().clear_authored_camera();
    Game::Session::SessionContext::active().reset();
  }
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
      Game::Session::SessionContext::active().visibility(),
      Game::Session::SessionContext::active().terrain());
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
      Game::Session::SessionContext::active().visibility(),
      Game::Session::SessionContext::active().terrain());
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

TEST_F(CameraFramingTest, ThePitchNeverEscapesItsBandHoweverFarTheCameraStrays) {
  Render::GL::Camera camera;
  camera.set_map_bounds({.tile_size = 1.0F, .width = 64, .height = 64});
  camera.set_rts_view(QVector3D(0.0F, 0.0F, 0.0F), 40.0F, 55.0F, 0.0F);
  settle(camera);

  float const min_pitch = camera.get_pitch_min_deg();
  float const max_pitch = camera.get_pitch_max_deg();
  ASSERT_LT(min_pitch, max_pitch);

  for (float const distance : {200.0F, -400.0F, 800.0F, -1600.0F}) {
    camera.set_position(QVector3D(distance, camera.get_position().y(), distance));
    settle(camera);
    float const pitch = camera.get_pitch_deg();
    EXPECT_GE(pitch, min_pitch - 0.01F)
        << "pitch escaped below its band at offset " << distance;
    EXPECT_LE(pitch, max_pitch + 0.01F)
        << "pitch escaped above its band at offset " << distance;
  }

  for (int step = 0; step < 40; ++step) {
    camera.pan(60.0F, 60.0F);
    settle(camera);
  }
  EXPECT_GE(camera.get_pitch_deg(), min_pitch - 0.01F);
  EXPECT_LE(camera.get_pitch_deg(), max_pitch + 0.01F);
}

TEST_F(CameraFramingTest, AResetRestoresTheFramingEvenWhileFollowing) {
  Render::GL::Camera camera;
  camera.set_map_bounds({.tile_size = 1.0F, .width = 64, .height = 64});
  camera.set_rts_view(QVector3D(0.0F, 0.0F, 0.0F), 40.0F, 55.0F, 0.0F);
  camera.set_follow_enabled(true);
  camera.capture_follow_offset();

  camera.set_position(QVector3D(900.0F, 4.0F, 900.0F));
  settle(camera);
  for (int frame = 0; frame < 5; ++frame) {
    camera.update_follow(QVector3D(0.0F, 0.0F, 0.0F));
  }

  const auto framing = Game::GameConfig::instance().camera_reset_framing();
  camera.set_rts_view(
      QVector3D(0.0F, 0.0F, 0.0F), framing.distance, framing.pitch, framing.yaw);
  camera.capture_follow_offset();
  settle(camera);
  float const pitch_after_reset = camera.get_pitch_deg();
  float const height_after_reset = height_above_target(camera);

  for (int frame = 0; frame < 5; ++frame) {
    camera.update_follow(QVector3D(0.0F, 0.0F, 0.0F));
  }

  EXPECT_NEAR(camera.get_pitch_deg(), pitch_after_reset, 0.5F)
      << "following pulled the camera back off the framing the reset just set";
  EXPECT_NEAR(height_above_target(camera), height_after_reset, 1.0F);
  EXPECT_GT(height_after_reset, 1.0F)
      << "the reset left the camera down at ground level";
}

TEST_F(CameraFramingTest, FollowingAJumpingSelectionKeepsTheViewingAngle) {
  Render::GL::Camera camera;
  camera.set_map_bounds({.tile_size = 1.0F, .width = 128, .height = 128});
  camera.set_rts_view(QVector3D(0.0F, 0.0F, 0.0F), 30.0F, 48.0F, 225.0F);
  settle(camera);
  camera.set_follow_enabled(true);
  camera.capture_follow_offset();
  float const pitch_before = camera.get_pitch_deg();
  float const height_before = height_above_target(camera);

  for (int frame = 0; frame < 3; ++frame) {
    camera.update_follow(QVector3D(30.0F, 0.0F, 20.0F));
    EXPECT_NEAR(camera.get_pitch_deg(), pitch_before, 0.5F)
        << "frame " << frame << ": the camera swung towards the horizon mid-follow";
    EXPECT_NEAR(height_above_target(camera), height_before, 1.0F);
  }
}

TEST_F(CameraFramingTest, SiegeTownCanBeReachedFromCampWithoutRotating) {
  Game::Map::MapDefinition map;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QStringLiteral("assets/maps/map_victumulae.json"), map, &error))
      << error.toStdString();
  Game::Map::TerrainService terrain;
  terrain.initialize(map);
  auto const world_point = [&](float x, float z) -> QVector3D {
    return {(x - (map.grid.width - 1) * 0.5F) * map.grid.tile_size,
            0.0F,
            (z - (map.grid.height - 1) * 0.5F) * map.grid.tile_size};
  };

  for (float const distance : {map.camera.distance, 85.0F}) {
    for (float const yaw : {map.camera.yaw_deg, 0.0F, 90.0F, 180.0F, 270.0F}) {
      SCOPED_TRACE(::testing::Message() << "distance=" << distance << " yaw=" << yaw);
      Render::GL::Camera camera;
      camera.set_map_bounds({.tile_size = map.grid.tile_size,
                             .width = map.grid.width,
                             .height = map.grid.height});
      camera.set_rts_constraints(true);
      camera.set_ground_height_sampler([&](float x, float z) {
        return terrain.get_height_map()->get_height_at(x, z);
      });
      camera.set_perspective(
          map.camera.fov_y, 16.0F / 9.0F, map.camera.near_plane, map.camera.far_plane);
      camera.set_rts_view(world_point(80, 130), distance, map.camera.tilt_deg, yaw);
      const float pitch = camera.get_pitch_deg();
      const QVector3D heading = camera.get_forward_vector();

      for (const QVector3D goal : {world_point(80, 82),
                                   world_point(80, 50),
                                   world_point(80, 25),
                                   world_point(40, 50),
                                   world_point(120, 50)}) {
        SCOPED_TRACE(::testing::Message() << "goal=" << goal.x() << "," << goal.z());
        for (int frame = 0; frame < 360; ++frame) {
          QVector3D delta = goal - camera.get_target();
          delta.setY(0.0F);
          if (delta.length() > 0.5F) {
            delta = delta.normalized() * 0.5F;
          }
          QVector3D forward = camera.get_forward_vector();
          forward.setY(0.0F);
          forward.normalize();
          camera.pan_eased(QVector3D::dotProduct(delta, camera.get_right_vector()),
                           QVector3D::dotProduct(delta, forward));
          camera.update(1.0F / 60.0F);
        }
        EXPECT_NEAR(camera.get_target().x(), goal.x(), 0.1F);
        EXPECT_NEAR(camera.get_target().z(), goal.z(), 0.1F);
        EXPECT_NEAR(camera.get_pitch_deg(), pitch, 0.1F);
        EXPECT_LT((camera.get_forward_vector() - heading).length(), 0.01F);
        const QVector3D eye = camera.get_position();
        EXPECT_GE(eye.y() - terrain.get_height_map()->get_height_at(eye.x(), eye.z()),
                  Render::GL::CameraDefaults::k_rts_terrain_clearance - 0.01F);
        if (goal == world_point(80, 50)) {
          const float height =
              terrain.get_height_map()->get_height_at(goal.x(), goal.z());
          ASSERT_GT(height, 5.0F) << "the route must reach the raised citadel";
          EXPECT_NEAR(camera.get_target().y(), height, 0.2F);
          QPointF screen;
          ASSERT_TRUE(camera.world_to_screen(
              QVector3D(goal.x(), height, goal.z()), 1920, 1080, screen));
          EXPECT_NEAR(screen.x(), 960.0, 2.0);

          EXPECT_NEAR(screen.y(), 540.0, 4.0);
        }
      }
    }
  }
}
