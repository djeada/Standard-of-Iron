#include <QVector3D>

#include <gtest/gtest.h>

#include "render/gl/backend/prop_mesh_builder.h"

namespace {

using Render::GL::BackendPipelines::append_oriented_box;
using Render::GL::BackendPipelines::PropMeshIndices;
using Render::GL::BackendPipelines::PropMeshVerts;

TEST(PropMeshBuilderTest, OrientedBoxNormalsPointAwayFromTheBoxCentre) {
  PropMeshVerts verts;
  PropMeshIndices idx;
  QVector3D const a(-0.58F, 0.02F, -0.20F);
  QVector3D const b(0.48F, 0.12F, 0.18F);
  append_oriented_box(verts, idx, a, b, 0.24F, 0.18F);
  ASSERT_EQ(verts.size(), 24U);
  ASSERT_EQ(idx.size(), 36U);

  QVector3D const centre = (a + b) * 0.5F;
  for (std::size_t face = 0; face < 6; ++face) {
    QVector3D face_centre;
    for (std::size_t corner = 0; corner < 4; ++corner) {
      face_centre += verts[face * 4 + corner].first;
    }
    face_centre *= 0.25F;
    QVector3D const outward = face_centre - centre;
    EXPECT_GT(QVector3D::dotProduct(verts[face * 4].second, outward), 0.0F)
        << "face " << face
        << " is lit from the inside; the iron ore is built from nothing but "
           "oriented boxes, so this is what turned it black";
  }
}

} // namespace
