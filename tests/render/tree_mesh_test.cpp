#include <algorithm>
#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <utility>

#include "game/map/map_definition.h"
#include "render/gl/backend/tree_mesh.h"

namespace {
using namespace Render::GL::BackendPipelines;

TEST(TreeMesh, BranchesAndCrownsHaveFiniteOutwardFacesWithinInstanceBudget) {
  for (const auto& mesh : {build_olive_tree_mesh()}) {
    ASSERT_FALSE(mesh.vertices.empty());
    ASSERT_EQ(mesh.indices.size() % 3U, 0U);
    // These meshes are instanced for forests, so protect against accidental
    // subdivision growth and the 16-bit element-buffer limit.
    EXPECT_LT(mesh.vertices.size(), 4096U);
    EXPECT_LT(mesh.indices.size() / 3U, 5000U);
    for (const auto& v : mesh.vertices) {
      EXPECT_TRUE(std::isfinite(v.position.x()));
      EXPECT_TRUE(std::isfinite(v.position.y()));
      EXPECT_TRUE(std::isfinite(v.position.z()));
      EXPECT_NEAR(v.normal.length(), 1.0F, 0.001F);
    }
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3U) {
      for (std::size_t j = 0; j < 3; ++j) {
        ASSERT_LT(mesh.indices[i + j], mesh.vertices.size());
      }
      const auto& a = mesh.vertices[mesh.indices[i]];
      const auto& b = mesh.vertices[mesh.indices[i + 1U]];
      const auto& c = mesh.vertices[mesh.indices[i + 2U]];
      const auto face =
          QVector3D::crossProduct(b.position - a.position, c.position - a.position);
      ASSERT_GT(face.lengthSquared(), 1.0e-14F) << "degenerate triangle " << i / 3U;
      EXPECT_GT(QVector3D::dotProduct(face, a.normal + b.normal + c.normal), 0.0F)
          << "back-facing branch or crown triangle " << i / 3U;
    }
  }
}

TEST(TreeMesh, RootsRemainGroundedAndFoliageUsesTheSpeciesMaterialRanges) {
  using Game::Map::WorldProp;
  const std::array cases{
      std::pair{build_olive_tree_mesh(), WorldProp::Type::OliveTree}};
  for (const auto& [mesh, species] : cases) {
    float bottom = 10.0F;
    float top = 0.0F;
    std::size_t foliage = 0;
    std::size_t bark = 0;
    for (const auto& v : mesh.vertices) {
      bottom = std::min(bottom, v.position.y());
      top = std::max(top, v.position.y());
      if (v.v >= 0.55F) {
        ++foliage;
        EXPECT_GT(v.position.y(), 0.30F);
      } else {
        ++bark;
        EXPECT_LT(v.v, 0.34F) << "branch vertices must not shade as foliage";
      }
      EXPECT_LT(std::hypot(v.position.x(), v.position.z()), 0.80F);
    }
    EXPECT_LE(bottom, 0.0F);
    EXPECT_GT(bottom, -0.06F);
    const float declared_height = Game::Map::world_prop_model_height(species);
    EXPECT_GT(top, declared_height * 0.80F);
    EXPECT_LE(top, declared_height);
    EXPECT_GT(foliage, 500U);
    EXPECT_GT(bark, 100U);
  }
}
} // namespace
