#include <QVector3D>

#include <cstddef>
#include <gtest/gtest.h>

#include "render/gl/mesh.h"
#include "render/gl/primitives.h"
#include "render/humanoid/asset/humanoid_derived_meshes.h"

namespace {

auto count_inward_side_normals(const Render::GL::Mesh& mesh) -> std::size_t {
  std::size_t inward = 0;
  for (auto const& v : mesh.get_vertices()) {
    QVector3D const normal(v.normal[0], v.normal[1], v.normal[2]);
    QVector3D const radial(v.position[0], 0.0F, v.position[2]);

    if (radial.lengthSquared() < 1e-6F || std::abs(normal.y()) > 0.95F) {
      continue;
    }
    if (QVector3D::dotProduct(normal, radial) < 0.0F) {
      ++inward;
    }
  }
  return inward;
}

} // namespace

TEST(UnitTorsoOrientation, SideNormalsPointOutward) {
  Render::GL::Mesh* torso = Render::GL::get_unit_torso();
  ASSERT_NE(torso, nullptr);
  EXPECT_EQ(count_inward_side_normals(*torso), 0U);
}

TEST(UnitTorsoOrientation, ArmourTorsoPartKeepsOutwardNormals) {
  Render::GL::Mesh* part = Render::Humanoid::humanoid_mesh_part(
      Render::Humanoid::HumanoidMeshPart::TorsoNoBottomCap);
  ASSERT_NE(part, nullptr);
  EXPECT_EQ(count_inward_side_normals(*part), 0U);
}
