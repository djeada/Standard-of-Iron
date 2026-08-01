#include <QString>

#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

#include "game/map/explored_mask_codec.h"
#include "game/map/visibility_service.h"

namespace {

using Game::Map::decode_explored_mask;
using Game::Map::encode_explored_mask;
using Game::Map::explored_mask_from_cells;
using Game::Map::ExploredMask;
using Game::Map::VisibilityState;

auto make_mask(int width,
               int height,
               const std::vector<std::uint8_t>& explored) -> ExploredMask {
  ExploredMask mask;
  mask.width = width;
  mask.height = height;
  mask.explored = explored;
  return mask;
}

} // namespace

TEST(ExploredMaskCodec, RoundTripsAPartiallyExploredGrid) {
  std::vector<std::uint8_t> explored(64, 0U);
  for (int z = 0; z < 4; ++z) {
    for (int x = 0; x < 5; ++x) {
      explored[static_cast<std::size_t>(z * 8 + x)] = 1U;
    }
  }
  const auto mask = make_mask(8, 8, explored);

  const auto decoded = decode_explored_mask(encode_explored_mask(mask), 8, 8);

  ASSERT_TRUE(decoded.is_valid());
  EXPECT_EQ(decoded.explored, mask.explored);
}

TEST(ExploredMaskCodec, RoundTripsAGridThatStartsExplored) {
  std::vector<std::uint8_t> explored(16, 1U);
  explored[15] = 0U;
  const auto mask = make_mask(4, 4, explored);

  const auto decoded = decode_explored_mask(encode_explored_mask(mask), 4, 4);

  ASSERT_TRUE(decoded.is_valid());
  EXPECT_EQ(decoded.explored, mask.explored);
}

TEST(ExploredMaskCodec, RoundTripsRunsLongerThanASingleRunLength) {

  const auto mask = make_mask(300, 300, std::vector<std::uint8_t>(90000, 0U));

  const auto decoded = decode_explored_mask(encode_explored_mask(mask), 300, 300);

  ASSERT_TRUE(decoded.is_valid());
  EXPECT_EQ(decoded.explored, mask.explored);
}

TEST(ExploredMaskCodec, RoundTripsFullyExploredRunsLongerThanTheRunCap) {
  const auto mask = make_mask(300, 300, std::vector<std::uint8_t>(90000, 1U));

  const auto decoded = decode_explored_mask(encode_explored_mask(mask), 300, 300);

  ASSERT_TRUE(decoded.is_valid());
  EXPECT_EQ(decoded.explored, mask.explored);
}

TEST(ExploredMaskCodec, CompressesLargeUniformMasks) {
  const auto mask = make_mask(256, 256, std::vector<std::uint8_t>(65536, 0U));

  const QString encoded = encode_explored_mask(mask);

  EXPECT_FALSE(encoded.isEmpty());
  EXPECT_LT(encoded.size(), 64);
}

TEST(ExploredMaskCodec, RejectsMasksWhoseCellCountDoesNotMatch) {
  const auto mask = make_mask(8, 8, std::vector<std::uint8_t>(10, 1U));

  EXPECT_TRUE(encode_explored_mask(mask).isEmpty());
}

TEST(ExploredMaskCodec, RejectsPayloadsThatDoNotFillTheGrid) {
  const auto mask = make_mask(4, 4, std::vector<std::uint8_t>(16, 1U));
  const QString encoded = encode_explored_mask(mask);

  const auto decoded = decode_explored_mask(encoded, 8, 8);

  EXPECT_FALSE(decoded.is_valid());
}

TEST(ExploredMaskCodec, RejectsGarbage) {
  EXPECT_FALSE(
      decode_explored_mask(QStringLiteral("not base64 at all!"), 4, 4).is_valid());
  EXPECT_FALSE(decode_explored_mask(QString(), 4, 4).is_valid());
}

TEST(ExploredMaskCodec, TreatsVisibleAndExploredAlikeWhenCapturing) {
  std::vector<std::uint8_t> cells{
      static_cast<std::uint8_t>(VisibilityState::Unseen),
      static_cast<std::uint8_t>(VisibilityState::Explored),
      static_cast<std::uint8_t>(VisibilityState::Visible),
      static_cast<std::uint8_t>(VisibilityState::Unseen),
  };

  const auto mask = explored_mask_from_cells(cells, 2, 2);

  ASSERT_TRUE(mask.is_valid());
  EXPECT_EQ(mask.explored, (std::vector<std::uint8_t>{0U, 1U, 1U, 0U}));
}
