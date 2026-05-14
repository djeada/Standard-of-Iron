#include <QVector3D>

#include <gtest/gtest.h>

#include "render/gl/mesh.h"
#include "render/gl/primitives.h"

namespace {

auto triangle_normal_y(const Render::GL::Vertex& a,
                       const Render::GL::Vertex& b,
                       const Render::GL::Vertex& c) -> float {
  QVector3D const pa(a.position[0], a.position[1], a.position[2]);
  QVector3D const pb(b.position[0], b.position[1], b.position[2]);
  QVector3D const pc(c.position[0], c.position[1], c.position[2]);
  return QVector3D::crossProduct(pb - pa, pc - pa).y();
}

TEST(OrientationArrowMesh, NotNull) {
  Render::GL::Mesh* mesh = Render::GL::get_orientation_arrow();
  ASSERT_NE(mesh, nullptr);
}

TEST(OrientationArrowMesh, HasSufficientGeometry) {
  Render::GL::Mesh* mesh = Render::GL::get_orientation_arrow();
  ASSERT_NE(mesh, nullptr);
  const auto& verts = mesh->get_vertices();
  const auto& idx = mesh->get_indices();

  EXPECT_GE(verts.size(), 16u);
  EXPECT_GE(idx.size(), 36u);
  EXPECT_EQ(idx.size() % 3, 0u);
}

TEST(OrientationArrowMesh, AllVerticesWithinExpectedBounds) {
  Render::GL::Mesh* mesh = Render::GL::get_orientation_arrow();
  ASSERT_NE(mesh, nullptr);
  const auto& verts = mesh->get_vertices();

  for (const auto& v : verts) {
    EXPECT_LE(v.position[0], 0.4F) << "X too large";
    EXPECT_GE(v.position[0], -0.4F) << "X too small";
    EXPECT_LE(v.position[1], 0.15F) << "Y too large";
    EXPECT_GE(v.position[1], -0.15F) << "Y too small";
    EXPECT_LE(v.position[2], 0.01F) << "Z should be <= 0 (points toward -Z)";
    EXPECT_GE(v.position[2], -2.0F) << "Z too far";
  }
}

TEST(OrientationArrowMesh, TopFaceNormalsPointUpward) {
  Render::GL::Mesh* mesh = Render::GL::get_orientation_arrow();
  ASSERT_NE(mesh, nullptr);
  const auto& verts = mesh->get_vertices();
  const auto& idx = mesh->get_indices();

  constexpr int k_top_tris = 6;
  ASSERT_GE(idx.size(), static_cast<std::size_t>(k_top_tris * 3));
  for (int i = 0; i < k_top_tris; ++i) {
    float ny = triangle_normal_y(
        verts[idx[i * 3 + 0]], verts[idx[i * 3 + 1]], verts[idx[i * 3 + 2]]);
    EXPECT_GT(ny, 0.0F) << "Top-face triangle " << i << " should face upward";
  }
}

TEST(OrientationArrowMesh, IndicesInRange) {
  Render::GL::Mesh* mesh = Render::GL::get_orientation_arrow();
  ASSERT_NE(mesh, nullptr);
  const auto& verts = mesh->get_vertices();
  const auto& idx = mesh->get_indices();
  for (std::size_t i = 0; i < idx.size(); ++i) {
    EXPECT_LT(idx[i], static_cast<unsigned int>(verts.size()))
        << "Index " << i << " out of range";
  }
}

} // namespace
