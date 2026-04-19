// Tests for the Stage-13 SoftwareRasterizer. No OpenGL context required.

#include "render/software/software_rasterizer.h"

#include <QImage>
#include <QMatrix4x4>
#include <QVector3D>
#include <gtest/gtest.h>

using Render::Software::ColoredTriangle;
using Render::Software::RasterSettings;
using Render::Software::SoftwareRasterizer;

namespace {

auto make_view_proj() -> QMatrix4x4 {
  QMatrix4x4 proj;
  proj.perspective(60.0F, 4.0F / 3.0F, 0.1F, 100.0F);
  QMatrix4x4 view;
  view.lookAt(QVector3D(0, 0, 5), QVector3D(0, 0, 0), QVector3D(0, 1, 0));
  return proj * view;
}

auto count_non_clear_pixels(const QImage &img, const QColor &clear) -> int {
  int count = 0;
  for (int y = 0; y < img.height(); ++y) {
    for (int x = 0; x < img.width(); ++x) {
      if (img.pixelColor(x, y) != clear) {
        ++count;
      }
    }
  }
  return count;
}

} // namespace

TEST(SoftwareRasterizerTest, EmptySceneProducesClearColor) {
  RasterSettings s;
  s.width = 80;
  s.height = 60;
  s.clear_color = QColor(10, 20, 30, 255);
  SoftwareRasterizer r(s);
  r.set_view_projection(make_view_proj());
  QImage img = r.render();
  EXPECT_EQ(img.width(), 80);
  EXPECT_EQ(img.height(), 60);
  EXPECT_EQ(img.pixelColor(0, 0), s.clear_color);
  EXPECT_EQ(img.pixelColor(40, 30), s.clear_color);
  EXPECT_EQ(count_non_clear_pixels(img, s.clear_color), 0);
}

TEST(SoftwareRasterizerTest, SingleCubeRendersPixels) {
  RasterSettings s;
  s.width = 128;
  s.height = 128;
  SoftwareRasterizer r(s);
  r.set_view_projection(make_view_proj());
  QMatrix4x4 m;
  r.submit_cube(m, QVector3D(1.0F, 0.2F, 0.2F), 1.0F);
  EXPECT_EQ(r.triangle_count(), 12U);
  QImage img = r.render();
  EXPECT_GT(count_non_clear_pixels(img, s.clear_color), 200);
}

TEST(SoftwareRasterizerTest, TriangleBehindCameraCulled) {
  RasterSettings s;
  s.width = 64;
  s.height = 64;
  SoftwareRasterizer r(s);
  r.set_view_projection(make_view_proj());
  // Camera is at z=5 looking toward origin. Put a tri at z=+20 (behind
  // camera).
  ColoredTriangle tri{{-1, -1, 20}, {1, -1, 20}, {0, 1, 20}, {1, 1, 1}, 1.0F};
  r.submit(tri);
  QImage img = r.render();
  EXPECT_EQ(count_non_clear_pixels(img, s.clear_color), 0);
}

TEST(SoftwareRasterizerTest, PainterAlgorithmNearerOverwritesFarther) {
  RasterSettings s;
  s.width = 128;
  s.height = 128;
  s.clear_color = QColor(0, 0, 0, 255);
  SoftwareRasterizer r(s);
  r.set_view_projection(make_view_proj());
  // Far red triangle, big.
  r.submit(ColoredTriangle{
      {-2, -2, -2}, {2, -2, -2}, {0, 2, -2}, {1.0F, 0.0F, 0.0F}, 1.0F});
  // Near green triangle, small, in the center.
  r.submit(ColoredTriangle{{-0.3F, -0.3F, 1},
                           {0.3F, -0.3F, 1},
                           {0.0F, 0.3F, 1},
                           {0.0F, 1.0F, 0.0F},
                           1.0F});
  QImage img = r.render();
  QColor center = img.pixelColor(64, 64);
  EXPECT_GT(center.green(), center.red());
}

TEST(SoftwareRasterizerTest, SubmitCubeAdds12Triangles) {
  SoftwareRasterizer r;
  QMatrix4x4 m;
  r.submit_cube(m, QVector3D(1, 1, 1));
  EXPECT_EQ(r.triangle_count(), 12U);
  r.submit_cube(m, QVector3D(1, 1, 1));
  EXPECT_EQ(r.triangle_count(), 24U);
}

TEST(SoftwareRasterizerTest, ClearRemovesTriangles) {
  SoftwareRasterizer r;
  r.submit_cube(QMatrix4x4(), QVector3D(1, 1, 1));
  EXPECT_EQ(r.triangle_count(), 12U);
  r.clear();
  EXPECT_EQ(r.triangle_count(), 0U);
}

TEST(SoftwareRasterizerTest, OutsideNdcIsCulled) {
  RasterSettings s;
  s.width = 64;
  s.height = 64;
  SoftwareRasterizer r(s);
  r.set_view_projection(make_view_proj());
  // Far off-screen to the right — all three verts NDC.x > 1.2.
  r.submit(
      ColoredTriangle{{50, -1, 0}, {52, -1, 0}, {51, 1, 0}, {1, 1, 1}, 1.0F});
  QImage img = r.render();
  EXPECT_EQ(count_non_clear_pixels(img, s.clear_color), 0);
}

TEST(SoftwareRasterizerTest, SettingsPreserved) {
  RasterSettings s;
  s.width = 200;
  s.height = 150;
  s.clear_color = QColor(99, 77, 55, 255);
  SoftwareRasterizer r(s);
  EXPECT_EQ(r.settings().width, 200);
  EXPECT_EQ(r.settings().height, 150);
  QImage img = r.render();
  EXPECT_EQ(img.width(), 200);
  EXPECT_EQ(img.height(), 150);
  EXPECT_EQ(img.pixelColor(0, 0), s.clear_color);
}
