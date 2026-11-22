#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "render/geom/instance_block.h"
#include "render/geom/texture_image.h"

namespace {

using Render::GL::InstanceBlock;
using Render::GL::TextureImage;

TEST(TextureImage, ResetSizesAndZeroesTheTexelsAndMarksTheWholeImage) {
  TextureImage image;
  image.reset(3, 2, TextureImage::Format::Rgba16F);
  EXPECT_EQ(image.width(), 3);
  EXPECT_EQ(image.height(), 2);
  EXPECT_EQ(TextureImage::bytes_per_texel(image.format()), 16U);
  EXPECT_EQ(image.last_change().width, 3);
  EXPECT_EQ(image.last_change().height, 2);
  const auto* floats = reinterpret_cast<const float*>(image.data());
  for (int i = 0; i < 3 * 2 * 4; ++i) {
    EXPECT_EQ(floats[i], 0.0F);
  }
}

TEST(TextureImage, ARegionUpdateWritesOnlyThatRegionAndRemembersIt) {
  TextureImage image;
  image.reset(4, 3, TextureImage::Format::Rgba8);
  const std::uint64_t storage = image.storage_version();
  const std::uint64_t before = image.version();

  const std::array<unsigned char, 8> two_texels{1, 2, 3, 4, 5, 6, 7, 8};
  image.update_region({1, 2, 2, 1}, two_texels.data());

  EXPECT_EQ(image.version(), before + 1);
  EXPECT_EQ(image.storage_version(), storage) << "a region update keeps the storage";
  EXPECT_EQ(image.last_change().x, 1);
  EXPECT_EQ(image.last_change().y, 2);
  EXPECT_EQ(image.last_change().width, 2);
  const auto* bytes = reinterpret_cast<const unsigned char*>(image.data());
  const std::size_t row_two = 2U * 4U * 4U;
  EXPECT_EQ(bytes[row_two + 4U], 1);
  EXPECT_EQ(bytes[row_two + 11U], 8);
  EXPECT_EQ(bytes[row_two + 0U], 0) << "texels outside the region must stay untouched";
  EXPECT_EQ(bytes[row_two + 12U], 0);
}

TEST(TextureImage, AnOutOfBoundsRegionIsIgnored) {
  TextureImage image;
  image.reset(2, 2, TextureImage::Format::Rgba8);
  const std::uint64_t before = image.version();
  const std::array<unsigned char, 16> texels{};
  image.update_region({1, 1, 2, 2}, texels.data());
  EXPECT_EQ(image.version(), before);
}

TEST(InstanceBlock, AssignCopiesTheItemsAndMovesTheVersion) {
  InstanceBlock block;
  EXPECT_TRUE(block.empty());
  const std::vector<float> items{1.0F, 2.0F, 3.0F};
  block.assign(items);
  EXPECT_EQ(block.size_bytes(), 3U * sizeof(float));
  EXPECT_EQ(block.version(), 1U);
  const auto* floats = reinterpret_cast<const float*>(block.data());
  EXPECT_EQ(floats[2], 3.0F);
}

TEST(InstanceBlock, EveryBlockHasItsOwnIdAndALifetimeThatEndsWithIt) {
  std::weak_ptr<const void> lifetime;
  std::uint64_t first_id = 0;
  {
    InstanceBlock first;
    InstanceBlock second;
    EXPECT_NE(first.id(), second.id());
    first_id = first.id();
    lifetime = first.lifetime();
    EXPECT_FALSE(lifetime.expired());
  }
  EXPECT_NE(first_id, 0U);
  EXPECT_TRUE(lifetime.expired()) << "a backend could not tell the block is gone";
}

TEST(InstanceBlock, ReservationOnlyGrows) {
  InstanceBlock block;
  block.reserve_bytes(64);
  block.reserve_bytes(16);
  EXPECT_EQ(block.reserved_bytes(), 64U);
}

} // namespace
