#include "render/geom/selection_ring.h"
#include "render/gl/mesh.h"

#include <QVector3D>
#include <gtest/gtest.h>

namespace {

auto triangle_normal_y(const Render::GL::Vertex &a, const Render::GL::Vertex &b,
                       const Render::GL::Vertex &c) -> float {
  QVector3D const pa(a.position[0], a.position[1], a.position[2]);
  QVector3D const pb(b.position[0], b.position[1], b.position[2]);
  QVector3D const pc(c.position[0], c.position[1], c.position[2]);
  return QVector3D::crossProduct(pb - pa, pc - pa).y();
}

TEST(SelectionRingMesh, FacesUpwardForOverheadRendering) {
  auto *mesh = Render::Geom::SelectionRing::get();
  ASSERT_NE(mesh, nullptr);

  auto const &vertices = mesh->get_vertices();
  auto const &indices = mesh->get_indices();
  ASSERT_GE(indices.size(), 6u);

  float const first_tri_y = triangle_normal_y(
      vertices[indices[0]], vertices[indices[1]], vertices[indices[2]]);
  float const second_tri_y = triangle_normal_y(
      vertices[indices[3]], vertices[indices[4]], vertices[indices[5]]);

  EXPECT_GT(first_tri_y, 0.0F);
  EXPECT_GT(second_tri_y, 0.0F);
}

} // namespace
