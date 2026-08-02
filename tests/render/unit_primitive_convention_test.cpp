#include <QVector3D>

#include <cmath>
#include <gtest/gtest.h>
#include <string_view>
#include <vector>

#include "render/gl/mesh.h"
#include "render/gl/primitives.h"

namespace {

struct Extent {
  float radial{0.0F};
  float half_height{0.0F};
};

auto measure(const Render::GL::Mesh* mesh) -> Extent {
  Extent extent;
  for (const auto& vertex : mesh->get_vertices()) {
    const float radial = std::hypot(vertex.position[0], vertex.position[2]);
    extent.radial = std::max(extent.radial, radial);
    extent.half_height = std::max(extent.half_height, std::abs(vertex.position[1]));
  }
  return extent;
}

} // namespace

TEST(UnitPrimitiveConvention, SpanPrimitivesShareUnitRadiusAndUnitHeight) {

  struct Case {
    std::string_view name;
    const Render::GL::Mesh* mesh;
  };

  const std::vector<Case> cases{
      {"cylinder", Render::GL::get_unit_cylinder()},
      {"capsule", Render::GL::get_unit_capsule()},
      {"cone", Render::GL::get_unit_cone()},
      {"tapered_cylinder", Render::GL::get_unit_tapered_cylinder(1.0F, 1.0F)},
  };

  for (const auto& c : cases) {
    ASSERT_NE(c.mesh, nullptr) << c.name;
    const Extent extent = measure(c.mesh);
    EXPECT_NEAR(extent.radial, 1.0F, 1.0e-4F)
        << c.name
        << " must span radius 1 so Geom::cylinder_between scales it by the "
           "caller's radius verbatim";
    EXPECT_NEAR(extent.half_height, 0.5F, 1.0e-4F)
        << c.name << " must span height 1 so the head/tail span sets its length";
  }
}

TEST(UnitPrimitiveConvention, TaperedCylinderHonoursItsTailRatio) {
  const Render::GL::Mesh* mesh = Render::GL::get_unit_tapered_cylinder(1.0F, 0.5F);
  ASSERT_NE(mesh, nullptr);

  float top_radial = 0.0F;
  float bottom_radial = 0.0F;
  for (const auto& vertex : mesh->get_vertices()) {
    const float radial = std::hypot(vertex.position[0], vertex.position[2]);
    if (vertex.position[1] > 0.0F) {
      top_radial = std::max(top_radial, radial);
    } else {
      bottom_radial = std::max(bottom_radial, radial);
    }
  }

  EXPECT_NEAR(bottom_radial, 1.0F, 1.0e-4F);
  EXPECT_NEAR(top_radial, 0.5F, 1.0e-4F);
}

TEST(UnitPrimitiveConvention, SphereSpansUnitRadius) {
  const Extent extent = measure(Render::GL::get_unit_sphere());
  EXPECT_NEAR(extent.radial, 1.0F, 1.0e-3F);
  EXPECT_NEAR(extent.half_height, 1.0F, 1.0e-3F);
}
