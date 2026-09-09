#include <array>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <map>

#include "render/gl/backend/tent_mesh.h"
#include "render/gl/backend/tent_parts.h"

namespace {

using Render::GL::BackendPipelines::build_tent_mesh;
using Render::GL::BackendPipelines::PropMeshData;
namespace TentParts = Render::GL::BackendPipelines::TentParts;

constexpr std::array<float, 5> k_material_codes{TentParts::k_material_canvas,
                                                TentParts::k_material_lining,
                                                TentParts::k_material_wood,
                                                TentParts::k_material_rope,
                                                TentParts::k_material_floor};

auto nearest_material(float length) -> float {
  float best = k_material_codes.front();
  for (float code : k_material_codes) {
    if (std::abs(code - length) < std::abs(best - length)) {
      best = code;
    }
  }
  return best;
}

TEST(TentMeshTest, EveryNormalCarriesAnExactMaterialCode) {
  const PropMeshData mesh = build_tent_mesh();
  ASSERT_FALSE(mesh.vertices.empty());
  std::map<float, int> seen;
  for (const auto& [position, normal] : mesh.vertices) {
    const float length = normal.length();
    const float code = nearest_material(length);
    EXPECT_NEAR(length, code, 2.0e-3F)
        << "vertex at (" << position.x() << ", " << position.y() << ", " << position.z()
        << ") has a normal of length " << length << ", which is no material code";
    ++seen[code];
  }
  for (float code : k_material_codes) {
    EXPECT_GT(seen[code], 0) << "no vertex uses material code " << code;
  }
}

TEST(TentMeshTest, CanvasIsTwoSided) {
  const PropMeshData mesh = build_tent_mesh();
  int canvas = 0;
  int lining = 0;
  for (const auto& vertex : mesh.vertices) {
    const float code = nearest_material(vertex.second.length());
    canvas += code == TentParts::k_material_canvas ? 1 : 0;
    lining += code == TentParts::k_material_lining ? 1 : 0;
  }

  EXPECT_GT(lining, canvas * 3 / 4);
  EXPECT_LE(lining, canvas);
}

TEST(TentMeshTest, IndicesFitTheUploadedIndexType) {
  const PropMeshData mesh = build_tent_mesh();
  EXPECT_LT(mesh.vertices.size(), size_t{65536});
  EXPECT_EQ(mesh.indices.size() % 3U, 0U);
  for (const auto index : mesh.indices) {
    EXPECT_LT(static_cast<size_t>(index), mesh.vertices.size());
  }
}

TEST(TentMeshTest, TheDoorIsOpen) {

  const PropMeshData mesh = build_tent_mesh();
  for (const auto& [position, normal] : mesh.vertices) {
    const float code = nearest_material(normal.length());
    if (code != TentParts::k_material_canvas && code != TentParts::k_material_lining) {
      continue;
    }
    const bool in_gable_plane =
        std::abs(position.z() + TentParts::k_half_depth) < 0.012F;
    const bool in_doorway =
        std::abs(position.x()) < TentParts::k_door_half_width - 0.04F &&
        position.y() < TentParts::k_door_height - 0.005F && position.y() > 0.06F;
    EXPECT_FALSE(in_gable_plane && in_doorway)
        << "cloth at (" << position.x() << ", " << position.y() << ", " << position.z()
        << ") closes the doorway";
  }
}

} // namespace
