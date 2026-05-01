

#include "render/geom/parts.h"
#include "render/math/bone_frame.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <gtest/gtest.h>

namespace {

QVector3D col(const QMatrix4x4 &m, int c) {
  return {m.data()[c * 4 + 0], m.data()[c * 4 + 1], m.data()[c * 4 + 2]};
}

bool approx(const QVector3D &a, const QVector3D &b, float eps = 1e-4F) {
  return (a - b).length() < eps;
}

TEST(OrientedCylinder, VerticalSegmentHonoursRightAxis) {

  const QVector3D top{0.0F, 1.6F, 0.0F};
  const QVector3D bot{0.0F, 1.0F, 0.0F};
  const QVector3D right{1.0F, 0.0F, 0.0F};
  const float r_right = 0.25F;
  const float r_forward = 0.15F;

  QMatrix4x4 m =
      Render::Geom::oriented_cylinder(top, bot, right, r_right, r_forward);

  QVector3D const x_col = col(m, 0);
  EXPECT_NEAR(x_col.length(), r_right, 1e-4F);
  EXPECT_GT(x_col.x(), 0.0F);
  EXPECT_NEAR(x_col.y(), 0.0F, 1e-4F);
  EXPECT_NEAR(x_col.z(), 0.0F, 1e-4F);

  QVector3D const z_col = col(m, 2);
  EXPECT_NEAR(z_col.length(), r_forward, 1e-4F);
  EXPECT_NEAR(z_col.x(), 0.0F, 1e-4F);
  EXPECT_NEAR(z_col.y(), 0.0F, 1e-4F);
  EXPECT_GT(std::abs(z_col.z()), 0.99F * r_forward);
}

TEST(OrientedCylinder, RightAxisVariesOverride) {

  const QVector3D top{0.0F, 1.0F, 0.0F};
  const QVector3D bot{0.0F, 0.0F, 0.0F};
  const float r_right = 0.3F;
  const float r_forward = 0.1F;

  QMatrix4x4 const mx = Render::Geom::oriented_cylinder(
      top, bot, QVector3D{1.0F, 0.0F, 0.0F}, r_right, r_forward);
  QMatrix4x4 const mz = Render::Geom::oriented_cylinder(
      top, bot, QVector3D{0.0F, 0.0F, 1.0F}, r_right, r_forward);

  EXPECT_TRUE(approx(col(mx, 0).normalized(), QVector3D(1, 0, 0)));

  QVector3D const mz_x = col(mz, 0).normalized();
  EXPECT_NEAR(std::abs(mz_x.z()), 1.0F, 1e-4F);
  EXPECT_NEAR(mz_x.x(), 0.0F, 1e-4F);
}

TEST(OrientedCylinder, DegenerateSegmentStillProducesValidFrame) {

  const QVector3D p{1.0F, 2.0F, 3.0F};
  QMatrix4x4 m =
      Render::Geom::oriented_cylinder(p, p, QVector3D{1, 0, 0}, 0.2F, 0.1F);

  EXPECT_GT(col(m, 0).length(), 0.0F);
  EXPECT_GT(col(m, 1).length(), 0.0F);
  EXPECT_GT(col(m, 2).length(), 0.0F);
  QVector3D const translation{m.data()[12], m.data()[13], m.data()[14]};
  EXPECT_TRUE(approx(translation, p));
}

TEST(BoneFrame, MakeBoneFrameOrthonormalises) {

  Render::Math::BoneFrame f = Render::Math::make_bone_frame(
      QVector3D{5, 10, 7}, QVector3D{0, 1, 0}, QVector3D{1, 1, 0});

  EXPECT_NEAR(QVector3D::dotProduct(f.right, f.up), 0.0F, 1e-4F);
  EXPECT_NEAR(f.right.length(), 1.0F, 1e-4F);
  EXPECT_NEAR(f.up.length(), 1.0F, 1e-4F);
  EXPECT_NEAR(f.forward.length(), 1.0F, 1e-4F);
  EXPECT_TRUE(approx(QVector3D::crossProduct(f.right, f.up), f.forward, 1e-4F));
}

} // namespace
