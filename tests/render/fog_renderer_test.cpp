#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

#include "game/map/visibility_service.h"
#include "render/ground/fog_renderer.h"

namespace {

using Game::Map::VisibilityState;
using Render::GL::FogRenderer;

auto filled_grid(int width,
                 int height,
                 VisibilityState state) -> std::vector<std::uint8_t> {
  return std::vector<std::uint8_t>(static_cast<std::size_t>(width * height),
                                   static_cast<std::uint8_t>(state));
}

} // namespace

TEST(FogRenderer, ExploredGroundGetsNoFogPatches) {
  FogRenderer fog;
  fog.update_mask(32, 32, 1.0F, filled_grid(32, 32, VisibilityState::Explored));

  EXPECT_EQ(fog.patch_count(), 0U);
}

TEST(FogRenderer, VisibleGroundGetsNoFogPatches) {
  FogRenderer fog;
  fog.update_mask(32, 32, 1.0F, filled_grid(32, 32, VisibilityState::Visible));

  EXPECT_EQ(fog.patch_count(), 0U);
}

TEST(FogRenderer, UnexploredGroundIsCoveredByChunkPatches) {
  FogRenderer fog;
  fog.update_mask(28, 28, 1.0F, filled_grid(28, 28, VisibilityState::Unseen));

  EXPECT_EQ(fog.patch_count(), 4U);
}

TEST(FogRenderer, PatchCountScalesWithChunksNotTiles) {
  FogRenderer small;
  small.update_mask(56, 56, 1.0F, filled_grid(56, 56, VisibilityState::Unseen));

  FogRenderer large;
  large.update_mask(112, 112, 1.0F, filled_grid(112, 112, VisibilityState::Unseen));

  EXPECT_EQ(small.patch_count(), 16U);
  EXPECT_EQ(large.patch_count(), 64U);
}

TEST(FogRenderer, FirstMaskStartsSettledSoNothingDissolvesOnLoad) {
  FogRenderer fog;
  fog.update_mask(14, 14, 1.0F, filled_grid(14, 14, VisibilityState::Explored));

  EXPECT_TRUE(fog.is_settled());
  EXPECT_EQ(fog.fog_amount_at(7, 7), 0.0F);
}

TEST(FogRenderer, NewlyExploredTilesDissolveRatherThanPop) {
  FogRenderer fog;
  fog.update_mask(14, 14, 1.0F, filled_grid(14, 14, VisibilityState::Unseen));
  ASSERT_EQ(fog.fog_amount_at(7, 7), 1.0F);

  auto revealed = filled_grid(14, 14, VisibilityState::Unseen);
  revealed[7 * 14 + 7] = static_cast<std::uint8_t>(VisibilityState::Visible);
  fog.update_mask(14, 14, 1.0F, revealed);

  EXPECT_FALSE(fog.is_settled());
  EXPECT_EQ(fog.fog_amount_at(7, 7), 1.0F);

  fog.advance_reveal(0.05F);
  const float mid = fog.fog_amount_at(7, 7);
  EXPECT_LT(mid, 1.0F);
  EXPECT_GT(mid, 0.0F);

  fog.advance_reveal(5.0F);
  EXPECT_EQ(fog.fog_amount_at(7, 7), 0.0F);
  EXPECT_TRUE(fog.is_settled());
}

TEST(FogRenderer, ExplorationIsAdditiveSoLosingSightDoesNotRefog) {
  FogRenderer fog;
  auto cells = filled_grid(14, 14, VisibilityState::Unseen);
  cells[7 * 14 + 7] = static_cast<std::uint8_t>(VisibilityState::Visible);
  fog.update_mask(14, 14, 1.0F, cells);
  fog.advance_reveal(5.0F);
  ASSERT_EQ(fog.fog_amount_at(7, 7), 0.0F);

  cells[7 * 14 + 7] = static_cast<std::uint8_t>(VisibilityState::Explored);
  fog.update_mask(14, 14, 1.0F, cells);
  fog.advance_reveal(5.0F);

  EXPECT_EQ(fog.fog_amount_at(7, 7), 0.0F);
}

TEST(FogRenderer, EmptyGridClearsEverything) {
  FogRenderer fog;
  fog.update_mask(14, 14, 1.0F, filled_grid(14, 14, VisibilityState::Unseen));
  ASSERT_GT(fog.patch_count(), 0U);

  fog.update_mask(0, 0, 1.0F, {});

  EXPECT_EQ(fog.patch_count(), 0U);
}
