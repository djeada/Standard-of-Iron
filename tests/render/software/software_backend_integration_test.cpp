

#include "render/draw_queue.h"
#include "render/gl/camera.h"
#include "render/gl/mesh.h"
#include "render/profiling/frame_profile.h"
#include "render/software_backend.h"

#include <QImage>
#include <QMatrix4x4>
#include <gtest/gtest.h>

using Render::GL::Camera;
using Render::GL::DrawQueue;
using Render::GL::MeshCmd;
using Render::GL::SoftwareBackend;
using Render::Profiling::global_profile;
using Render::Profiling::Phase;
using Render::Profiling::PhaseScope;

namespace {

auto make_camera() -> Camera {
  Camera cam;
  cam.set_perspective(60.0F, 4.0F / 3.0F, 0.1F, 100.0F);
  cam.look_at(QVector3D(0, 5, 8), QVector3D(0, 0, 0), QVector3D(0, 1, 0));
  return cam;
}

} // namespace

TEST(SoftwareBackendIntegration, EmptyQueueProducesValidImage) {
  SoftwareBackend backend;
  Render::Software::RasterSettings s;
  s.width = 64;
  s.height = 48;
  s.clear_color = QColor(10, 20, 30, 255);
  backend.set_settings(s);
  backend.begin_frame();
  DrawQueue queue;
  Camera cam = make_camera();
  backend.execute(queue, cam);
  QImage const &img = backend.last_frame();
  EXPECT_EQ(img.width(), 64);
  EXPECT_EQ(img.height(), 48);
  EXPECT_EQ(img.pixelColor(0, 0), s.clear_color);
}

TEST(SoftwareBackendIntegration, MeshCmdBecomesVisibleTriangles) {
  SoftwareBackend backend;
  Render::Software::RasterSettings s;
  s.width = 128;
  s.height = 96;
  s.clear_color = QColor(0, 0, 0, 255);
  backend.set_settings(s);
  backend.begin_frame();

  DrawQueue queue;
  MeshCmd cmd;
  cmd.color = QVector3D(1.0F, 0.1F, 0.1F);
  cmd.alpha = 1.0F;

  queue.submit(cmd);

  Camera cam = make_camera();
  backend.execute(queue, cam);
  QImage const &img = backend.last_frame();
  int non_clear = 0;
  for (int y = 0; y < img.height(); ++y) {
    for (int x = 0; x < img.width(); ++x) {
      if (img.pixelColor(x, y) != s.clear_color) {
        ++non_clear;
      }
    }
  }
  EXPECT_GT(non_clear, 50);
}

TEST(SoftwareBackendIntegration, DrawPartCmdAlsoRendered) {
  SoftwareBackend backend;
  Render::Software::RasterSettings s;
  s.width = 96;
  s.height = 96;
  s.clear_color = QColor(0, 0, 0, 255);
  backend.set_settings(s);
  backend.begin_frame();

  DrawQueue queue;
  Render::GL::DrawPartCmd cmd;
  cmd.color = QVector3D(0.2F, 0.9F, 0.3F);
  cmd.alpha = 1.0F;
  queue.submit(cmd);

  Camera cam = make_camera();
  backend.execute(queue, cam);
  QImage const &img = backend.last_frame();
  int green_pixels = 0;
  for (int y = 0; y < img.height(); ++y) {
    for (int x = 0; x < img.width(); ++x) {
      QColor const c = img.pixelColor(x, y);
      if (c.green() > c.red() && c.green() > c.blue()) {
        ++green_pixels;
      }
    }
  }
  EXPECT_GT(green_pixels, 30);
}

TEST(FrameProfileIntegration, PhasesRecordElapsedInRenderer) {
  auto &p = global_profile();
  p.reset();
  p.enabled = true;
  { PhaseScope s(&p, Phase::Collection); }
  { PhaseScope s(&p, Phase::Sort); }
  { PhaseScope s(&p, Phase::Playback); }

  EXPECT_GE(p.total_us(), 0U);
  std::string out = Render::Profiling::format_overlay(p);
  EXPECT_NE(out.find("collect"), std::string::npos);
  EXPECT_NE(out.find("play"), std::string::npos);
}
