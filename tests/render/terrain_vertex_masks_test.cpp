#include <gtest/gtest.h>
#include <vector>

#include "game/map/terrain.h"
#include "render/ground/terrain_quad_tessellation.h"
#include "render/ground/terrain_vertex_masks.h"

namespace {

using Game::Map::TerrainType;

TEST(TerrainVertexMasksTest, EntryMaskAndLandformSurviveThePackedAttribute) {
  for (float entry : {0.0F, 0.25F, 0.5F, 0.97F, 1.0F}) {
    for (float hill : {0.0F, 0.25F, 0.5F, 1.0F}) {
      for (float mountain : {0.0F, 0.5F, 1.0F}) {
        const float packed =
            Render::Ground::pack_entry_and_landform(entry, hill, mountain);
        const auto unpacked = Render::Ground::unpack_entry_and_landform(packed);
        EXPECT_NEAR(unpacked.entry_mask, entry, 2.0e-3F);
        EXPECT_FLOAT_EQ(unpacked.hill, hill);
        EXPECT_FLOAT_EQ(unpacked.mountain, mountain);
      }
    }
  }
}

TEST(TerrainVertexMasksTest, LandformWeightsFadeAcrossAHillRim) {
  const int size = 8;
  std::vector<TerrainType> types(static_cast<std::size_t>(size * size),
                                 TerrainType::Flat);
  for (int z = 0; z < size; ++z) {
    for (int x = 4; x < size; ++x) {
      types[static_cast<std::size_t>(z * size + x)] = TerrainType::Hill;
    }
  }
  const auto weights = Render::Ground::compute_landform_weights(types, size, size);
  const auto hill_at = [&](int x) {
    return weights.hill[static_cast<std::size_t>(4 * size + x)];
  };
  EXPECT_FLOAT_EQ(hill_at(1), 0.0F);
  EXPECT_FLOAT_EQ(hill_at(6), 1.0F);
  EXPECT_GT(hill_at(4), hill_at(3));
  EXPECT_GT(hill_at(3), 0.0F);
  EXPECT_LT(hill_at(4), 1.0F);
  for (const float mountain : weights.mountain) {
    EXPECT_FLOAT_EQ(mountain, 0.0F);
  }
}

TEST(TerrainVertexMasksTest, QuadTessellationFollowsTheChosenDiagonal) {
  std::vector<unsigned int> falling;
  Render::Ground::append_quad_triangles(falling, 0U, 1U, 2U, 3U, false);
  EXPECT_EQ(falling, (std::vector<unsigned int>{0U, 1U, 2U, 2U, 1U, 3U}));

  std::vector<unsigned int> rising;
  Render::Ground::append_quad_triangles(rising, 0U, 1U, 2U, 3U, true);
  EXPECT_EQ(rising, (std::vector<unsigned int>{0U, 1U, 3U, 0U, 3U, 2U}));

  std::vector<unsigned int> subdivided;
  Render::Ground::append_subdivided_quad_triangles(
      subdivided, {0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U}, true);
  ASSERT_EQ(subdivided.size(), 24U);
  EXPECT_EQ(subdivided[0], 0U);
  EXPECT_EQ(subdivided[2], 4U)
      << "the first sub-quad creases through the centre vertex";
}

} // namespace
