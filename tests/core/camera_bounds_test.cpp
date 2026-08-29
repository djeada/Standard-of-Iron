#include <QVector3D>

#include <cmath>
#include <gtest/gtest.h>

#include "scene/camera.h"

namespace {

using Render::GL::Camera;
namespace Defaults = Render::GL::CameraDefaults;

constexpr int k_map_size = 64;
constexpr float k_tile = 1.0F;

auto rts_camera() -> Camera {
  Camera camera;
  camera.set_perspective(45.0F, 16.0F / 9.0F, 1.0F, 200.0F);
  camera.set_map_bounds(
      {.tile_size = k_tile, .width = k_map_size, .height = k_map_size});
  camera.set_rts_constraints(true);
  return camera;
}

auto view_limit() -> float {
  return (static_cast<float>(k_map_size) * 0.5F - 0.5F) * k_tile +
         Defaults::k_rts_edge_view_margin_tiles * k_tile;
}

auto map_half() -> float {
  return (static_cast<float>(k_map_size) * 0.5F - 0.5F) * k_tile;
}

void settle(Camera& camera) {
  camera.update(10.0F);
}

void expect_view_on_map(const Camera& camera, const char* context) {
  QVector3D top;
  ASSERT_TRUE(camera.top_of_screen_ground_point(top))
      << context << ": the top of the screen looks at the sky";
  float const limit = view_limit() + 0.05F;
  EXPECT_LE(std::abs(top.x()), limit) << context << " top x " << top.x();
  EXPECT_LE(std::abs(top.z()), limit) << context << " top z " << top.z();
  float const half = map_half() + 0.05F;
  EXPECT_LE(std::abs(camera.get_target().x()), half) << context;
  EXPECT_LE(std::abs(camera.get_target().z()), half) << context;
}

TEST(CameraBounds, ZoomingOutIsCappedByTheMapSize) {
  Camera camera = rts_camera();
  camera.set_rts_view(QVector3D(0, 0, 0), 20.0F, 45.0F, 0.0F);
  for (int i = 0; i < 200; ++i) {
    camera.zoom_distance(-1.0F);
  }
  float const diagonal = std::sqrt(2.0F) * static_cast<float>(k_map_size) * k_tile;
  EXPECT_LE(camera.get_distance(), camera.max_distance() + 0.01F);
  EXPECT_LE(camera.max_distance(),
            diagonal * Defaults::k_rts_max_distance_diagonal_ratio + 0.01F);
  EXPECT_LT(camera.max_distance(), Defaults::k_max_rts_distance);
}

TEST(CameraBounds, TheHorizonIsNeverReachable) {
  Camera camera = rts_camera();
  camera.set_rts_view(QVector3D(0, 0, 0), 20.0F, 10.0F, 0.0F);
  settle(camera);
  EXPECT_LE(camera.get_pitch_deg(), Defaults::k_rts_pitch_max_near + 0.01F);

  for (int i = 0; i < 40; ++i) {
    camera.orbit(0.0F, 8.0F);
    settle(camera);
  }
  EXPECT_LE(camera.get_pitch_deg(), camera.get_pitch_max_deg() + 0.01F);
  QVector3D top;
  EXPECT_TRUE(camera.top_of_screen_ground_point(top));
}

TEST(CameraBounds, ZoomingOutTiltsTheViewTowardTopDown) {
  Camera camera = rts_camera();
  camera.set_rts_view(QVector3D(0, 0, 0), Defaults::k_min_rts_distance, 35.0F, 0.0F);
  settle(camera);
  EXPECT_NEAR(camera.get_pitch_deg(), -35.0F, 0.5F);

  for (int i = 0; i < 200; ++i) {
    camera.zoom_distance(-1.0F);
  }
  settle(camera);
  EXPECT_LE(camera.get_pitch_deg(), Defaults::k_rts_pitch_max_far + 0.5F);
}

TEST(CameraBounds, PanningToEveryEdgeKeepsTheTopOfTheScreenOnTheMap) {
  for (float const yaw : {0.0F, 45.0F, 90.0F, 180.0F, 225.0F, 270.0F}) {
    for (float const distance : {Defaults::k_min_rts_distance, 20.0F, 400.0F}) {
      struct Direction {
        float right;
        float forward;
      };
      for (auto const dir : {Direction{1, 0},
                             Direction{-1, 0},
                             Direction{0, 1},
                             Direction{0, -1},
                             Direction{1, 1},
                             Direction{-1, -1}}) {
        Camera camera = rts_camera();
        camera.set_rts_view(QVector3D(0, 0, 0), distance, 45.0F, yaw);
        settle(camera);
        expect_view_on_map(camera, "after reset");
        for (int step = 0; step < 150; ++step) {
          camera.pan(dir.right * 2.0F, dir.forward * 2.0F);
        }
        settle(camera);
        expect_view_on_map(camera, "after panning to the edge");
      }
    }
  }
}

TEST(CameraBounds, AnAuthoredFramingAtTheCornerIsPulledOntoTheMap) {
  Camera camera = rts_camera();
  camera.set_rts_view(QVector3D(map_half(), 0, map_half()), 300.0F, 45.0F, 225.0F);
  settle(camera);
  EXPECT_LE(camera.get_distance(), camera.max_distance() + 0.01F);
  expect_view_on_map(camera, "authored corner framing");
}

TEST(CameraBounds, FreeCamerasKeepTheLegacyBand) {
  Camera camera;
  camera.set_perspective(45.0F, 16.0F / 9.0F, 1.0F, 200.0F);
  camera.set_map_bounds(
      {.tile_size = k_tile, .width = k_map_size, .height = k_map_size});
  camera.set_rts_view(QVector3D(0, 0, 0), 20.0F, 10.0F, 0.0F);
  settle(camera);
  EXPECT_NEAR(camera.get_pitch_deg(), -10.0F, 0.5F);
  EXPECT_FLOAT_EQ(camera.max_distance(), Defaults::k_max_rts_distance);
}

} // namespace

namespace {

TEST(CameraBounds, TheEyeStaysClearOfRaisedTerrain) {
  Camera camera = rts_camera();
  auto const terrain = [](float x, float z) -> float {
    return (std::abs(x) < 10.0F && std::abs(z) < 10.0F) ? 9.0F : 0.0F;
  };
  camera.set_ground_height_sampler(terrain);
  auto const clearance = [&]() -> float {
    auto const& eye = camera.get_position();
    return eye.y() - terrain(eye.x(), eye.z());
  };
  camera.set_rts_view(QVector3D(0, 0, 0), Defaults::k_min_rts_distance, 35.0F, 0.0F);
  settle(camera);
  EXPECT_GE(clearance(), Defaults::k_rts_terrain_clearance - 0.01F);
  EXPECT_GT(camera.get_distance(), Defaults::k_min_rts_distance)
      << "the eye was not pushed back out of the hill";
  EXPECT_NEAR(camera.get_pitch_deg(), -35.0F, 0.5F);
  for (int i = 0; i < 20; ++i) {
    camera.zoom_distance(1.0F);
  }
  EXPECT_GE(clearance(), Defaults::k_rts_terrain_clearance - 0.01F);
}

TEST(CameraBounds, TheEyeNeverLeavesTheMapMargin) {
  Camera camera = rts_camera();
  camera.set_rts_view(QVector3D(0, 0, 0), 40.0F, 45.0F, 0.0F);
  for (int step = 0; step < 150; ++step) {
    camera.pan(0.0F, -2.0F);
  }
  settle(camera);
  float const limit = view_limit() + 0.05F;
  EXPECT_LE(std::abs(camera.get_position().z()), limit);
  EXPECT_LE(std::abs(camera.get_position().x()), limit);
}

} // namespace

namespace {

TEST(CameraMotion, AHeldPanKeyMovesAtASteadyDtScaledSpeedAndEasesOut) {
  Camera camera = rts_camera();
  camera.set_map_bounds({.tile_size = 1.0F, .width = 650, .height = 650});
  camera.set_rts_view(QVector3D(0, 0, 0), 20.0F, 45.0F, 0.0F);
  QVector3D const start = camera.get_target();
  camera.pan_eased(1.0F, 0.0F);
  EXPECT_EQ(camera.get_target(), start) << "a pan request must not jump";

  for (int i = 0; i < 60; ++i) {
    camera.pan_eased(1.0F, 0.0F);
    camera.update(1.0F / 60.0F);
  }
  float const travelled = (camera.get_target() - start).length();
  EXPECT_GT(travelled, 45.0F);
  EXPECT_LT(travelled, 62.0F);

  Camera slow = rts_camera();
  slow.set_map_bounds({.tile_size = 1.0F, .width = 650, .height = 650});
  slow.set_rts_view(QVector3D(0, 0, 0), 20.0F, 45.0F, 0.0F);
  for (int frame = 0; frame < 10; ++frame) {
    for (int tick = 0; tick < 6; ++tick) {
      slow.pan_eased(1.0F, 0.0F);
    }
    slow.update(0.1F);
  }
  EXPECT_NEAR((slow.get_target() - start).length(), travelled, 12.0F);

  QVector3D const at_release = camera.get_target();
  for (int i = 0; i < 60; ++i) {
    camera.update(1.0F / 60.0F);
  }
  float const coast = (camera.get_target() - at_release).length();
  EXPECT_GT(coast, 0.5F);
  EXPECT_LT(coast, 8.0F);
  EXPECT_FALSE(camera.has_pending_motion());
}

TEST(CameraMotion, AnEasedZoomKeepsTheAnchoredGroundUnderTheCursor) {
  Camera camera = rts_camera();
  camera.set_rts_view(QVector3D(0, 0, 0), 20.0F, 45.0F, 0.0F);
  QVector3D before;
  ASSERT_TRUE(camera.screen_to_ground(0.25, 0.3, 1.0, 1.0, before));
  camera.zoom_distance_eased(3.0F);
  camera.set_zoom_anchor(0.25F, 0.3F);
  for (int i = 0; i < 120; ++i) {
    camera.update(1.0F / 60.0F);
  }
  EXPECT_LT(camera.get_distance(), 20.0F);
  QVector3D after;
  ASSERT_TRUE(camera.screen_to_ground(0.25, 0.3, 1.0, 1.0, after));
  EXPECT_NEAR(after.x(), before.x(), 0.2F);
  EXPECT_NEAR(after.z(), before.z(), 0.2F);
}

TEST(CameraMotion, EasedZoomRespectsTheMapCap) {
  Camera camera = rts_camera();
  camera.set_rts_view(QVector3D(0, 0, 0), 20.0F, 45.0F, 0.0F);
  for (int i = 0; i < 50; ++i) {
    camera.zoom_distance_eased(-1.0F);
  }
  for (int i = 0; i < 200; ++i) {
    camera.update(1.0F / 60.0F);
  }
  EXPECT_NEAR(camera.get_distance(), camera.max_distance(), 0.05F);
}

} // namespace

namespace {

TEST(CameraMotion, RepeatedAnchoredWheelNotchesDoNotFlingTheCamera) {
  Camera camera;
  camera.set_perspective(45.0F, 1920.0F / 1080.0F, 1.0F, 1462.5F);
  camera.set_map_bounds({.tile_size = 1.0F, .width = 650, .height = 650});
  camera.set_rts_constraints(true);
  camera.set_ground_height_sampler([](float x, float z) -> float {
    return (std::abs(x - 40.0F) < 20.0F && std::abs(z - 30.0F) < 20.0F) ? 8.0F : 0.0F;
  });
  camera.set_rts_view(QVector3D(0, 0, 0), 85.0F, 55.0F, 210.0F);
  for (int i = 0; i < 60; ++i) {
    camera.update(1.0F / 60.0F);
  }
  QVector3D const start_target = camera.get_target();
  QVector3D before;
  ASSERT_TRUE(camera.screen_to_ground(0.208, 0.278, 1.0, 1.0, before));
  for (int notch = 0; notch < 6; ++notch) {
    camera.zoom_distance_eased(0.8F);
    camera.set_zoom_anchor(0.208F, 0.278F);
    camera.update(0.4F);
  }
  for (int i = 0; i < 60; ++i) {
    camera.update(1.0F / 60.0F);
  }
  QVector3D after;
  ASSERT_TRUE(camera.screen_to_ground(0.208, 0.278, 1.0, 1.0, after));
  EXPECT_NEAR(after.x(), before.x(), 1.0F);
  EXPECT_NEAR(after.z(), before.z(), 1.0F);
  EXPECT_LT((camera.get_target() - start_target).length(), 85.0F)
      << "the target travelled further than the whole zoom distance";
}

} // namespace
