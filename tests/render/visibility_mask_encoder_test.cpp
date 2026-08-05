#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

#include "game/map/visibility_service.h"
#include "render/ground/visibility_mask_encoder.h"

namespace {

using Game::Map::VisibilityState;
using Render::Ground::encode_fog_mask_region;
using Render::Ground::encode_visibility_mask_region;
using Render::Ground::MaskRegion;

constexpr int k_seen_channel = 0;
constexpr int k_known_channel = 1;

auto channel_at(const std::vector<unsigned char>& texels,
                int width,
                int x,
                int z,
                int channel) -> int {
  return static_cast<int>(
      texels[static_cast<std::size_t>((z * width + x) * 4 + channel)]);
}

auto half_visible_grid(int width, int height) -> std::vector<std::uint8_t> {
  std::vector<std::uint8_t> cells(static_cast<std::size_t>(width * height),
                                  static_cast<std::uint8_t>(VisibilityState::Unseen));
  for (int z = 0; z < height; ++z) {
    for (int x = 0; x < width / 2; ++x) {
      cells[static_cast<std::size_t>(z * width + x)] =
          static_cast<std::uint8_t>(VisibilityState::Visible);
    }
  }
  return cells;
}

} // namespace

TEST(VisibilityMaskEncoder, VisibleInteriorIsFullyLitInBothChannels) {
  const int width = 8;
  const int height = 8;
  const auto cells = half_visible_grid(width, height);

  std::vector<unsigned char> texels;
  encode_visibility_mask_region(
      cells, width, height, MaskRegion::whole(width, height), texels);
  ASSERT_EQ(texels.size(), static_cast<std::size_t>(width * height * 4));

  EXPECT_EQ(channel_at(texels, width, 1, 4, k_seen_channel), 255);
  EXPECT_EQ(channel_at(texels, width, 1, 4, k_known_channel), 255);
}

TEST(VisibilityMaskEncoder, UnexploredInteriorStaysDark) {
  const int width = 8;
  const int height = 8;
  const auto cells = half_visible_grid(width, height);

  std::vector<unsigned char> texels;
  encode_visibility_mask_region(
      cells, width, height, MaskRegion::whole(width, height), texels);

  EXPECT_EQ(channel_at(texels, width, 7, 4, k_seen_channel), 0);
  EXPECT_EQ(channel_at(texels, width, 7, 4, k_known_channel), 0);
}

TEST(VisibilityMaskEncoder, BoundaryRampsInsteadOfSteppingAtTheTileEdge) {
  const int width = 8;
  const int height = 8;
  const auto cells = half_visible_grid(width, height);

  std::vector<unsigned char> texels;
  encode_visibility_mask_region(
      cells, width, height, MaskRegion::whole(width, height), texels);

  const int frontier = channel_at(texels, width, 3, 4, k_known_channel);
  const int beyond = channel_at(texels, width, 4, 4, k_known_channel);
  EXPECT_GT(frontier, 0);
  EXPECT_LT(frontier, 255);
  EXPECT_GT(beyond, 0);
  EXPECT_LT(beyond, frontier);
}

TEST(VisibilityMaskEncoder, ExploredTilesAreKnownButNotSeen) {
  const int width = 4;
  const int height = 1;
  std::vector<std::uint8_t> cells(4,
                                  static_cast<std::uint8_t>(VisibilityState::Explored));

  std::vector<unsigned char> texels;
  encode_visibility_mask_region(
      cells, width, height, MaskRegion::whole(width, height), texels);

  EXPECT_EQ(channel_at(texels, width, 1, 0, k_seen_channel), 0);
  EXPECT_GT(channel_at(texels, width, 1, 0, k_known_channel), 200);
}

TEST(VisibilityMaskEncoder, RejectsCellCountsThatDoNotMatchTheGrid) {
  std::vector<std::uint8_t> cells(3,
                                  static_cast<std::uint8_t>(VisibilityState::Visible));

  std::vector<unsigned char> texels;
  encode_visibility_mask_region(cells, 4, 4, MaskRegion::whole(4, 4), texels);

  EXPECT_TRUE(texels.empty());
}

TEST(VisibilityMaskEncoder, FogMaskBlursTheFogField) {
  const int width = 5;
  const int height = 1;
  const std::vector<float> fog{1.0F, 1.0F, 1.0F, 0.0F, 0.0F};
  const std::vector<float> seen{0.0F, 0.0F, 0.0F, 0.0F, 1.0F};

  std::vector<unsigned char> texels;
  encode_fog_mask_region(
      fog, seen, width, height, MaskRegion::whole(width, height), texels);
  ASSERT_EQ(texels.size(), static_cast<std::size_t>(width * height * 4));

  EXPECT_EQ(channel_at(texels, width, 0, 0, 0), 255);
  const int frontier = channel_at(texels, width, 2, 0, 0);
  const int beyond = channel_at(texels, width, 3, 0, 0);
  EXPECT_LT(frontier, 255);
  EXPECT_GT(frontier, beyond);
  EXPECT_GT(beyond, 0);
}

TEST(VisibilityMaskEncoder, FogMaskCarriesLiveSightSoFogCanBeHeldBack) {
  const int width = 5;
  const int height = 1;
  const std::vector<float> fog{1.0F, 1.0F, 1.0F, 0.0F, 0.0F};
  const std::vector<float> seen{0.0F, 0.0F, 0.0F, 1.0F, 1.0F};

  std::vector<unsigned char> texels;
  encode_fog_mask_region(
      fog, seen, width, height, MaskRegion::whole(width, height), texels);

  EXPECT_GT(channel_at(texels, width, 4, 0, 1), 200);
  EXPECT_GT(channel_at(texels, width, 2, 0, 1), 0);
  EXPECT_EQ(channel_at(texels, width, 0, 0, 1), 0);
}

TEST(VisibilityMaskEncoder, FogMaskRejectsMismatchedFields) {
  std::vector<unsigned char> texels;
  encode_fog_mask_region({1.0F, 1.0F}, {0.0F}, 2, 1, MaskRegion::whole(2, 1), texels);

  EXPECT_TRUE(texels.empty());
}

TEST(VisibilityMaskEncoder, RegionEncodeMatchesTheWholeGridEncode) {
  const int width = 12;
  const int height = 12;
  const auto cells = half_visible_grid(width, height);

  std::vector<unsigned char> whole;
  encode_visibility_mask_region(
      cells, width, height, MaskRegion::whole(width, height), whole);

  const MaskRegion region{4, 3, 5, 6};
  std::vector<unsigned char> slice;
  encode_visibility_mask_region(cells, width, height, region, slice);
  ASSERT_EQ(slice.size(), static_cast<std::size_t>(region.width * region.height * 4));

  for (int row = 0; row < region.height; ++row) {
    for (int column = 0; column < region.width; ++column) {
      for (int channel = 0; channel < 4; ++channel) {
        const std::size_t slice_index =
            static_cast<std::size_t>(row * region.width + column) * 4 + channel;
        const std::size_t whole_index =
            static_cast<std::size_t>((region.z + row) * width + region.x + column) * 4 +
            channel;
        EXPECT_EQ(slice[slice_index], whole[whole_index])
            << "row " << row << " column " << column << " channel " << channel;
      }
    }
  }
}

TEST(VisibilityMaskEncoder, DirtyRegionGrowsByTheBlurReach) {
  MaskRegion region;
  EXPECT_TRUE(region.is_empty());

  region.include(5, 5, 32, 32);
  EXPECT_EQ(region.x, 4);
  EXPECT_EQ(region.z, 4);
  EXPECT_EQ(region.width, 3);
  EXPECT_EQ(region.height, 3);

  region.include(9, 5, 32, 32);
  EXPECT_EQ(region.x, 4);
  EXPECT_EQ(region.width, 7);

  MaskRegion corner;
  corner.include(0, 0, 32, 32);
  EXPECT_EQ(corner.x, 0);
  EXPECT_EQ(corner.z, 0);
  EXPECT_EQ(corner.width, 2);
  EXPECT_EQ(corner.height, 2);
}

TEST(VisibilityMaskEncoder, RegionOutsideTheGridEncodesNothing) {
  const int width = 8;
  const int height = 8;
  const auto cells = half_visible_grid(width, height);

  std::vector<unsigned char> texels;
  encode_visibility_mask_region(cells, width, height, MaskRegion{}, texels);

  EXPECT_TRUE(texels.empty());
}
