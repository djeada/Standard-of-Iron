#include "render/selection_ring_layout.h"

#include <gtest/gtest.h>

#include <cmath>

namespace {

TEST(SelectionRingLayout, SingleUnitUsesTransformOrigin) {
  Render::GL::SelectionRingLayoutInput input;
  input.position = QVector3D(4.0F, 0.0F, -3.0F);
  input.ring_size = 1.2F;

  auto const placements = Render::GL::build_selection_ring_layout(input);

  ASSERT_EQ(placements.size(), 1u);
  EXPECT_FLOAT_EQ(placements.front().world_x, 4.0F);
  EXPECT_FLOAT_EQ(placements.front().world_z, -3.0F);
  EXPECT_FLOAT_EQ(placements.front().ring_size, 1.2F);
}

TEST(SelectionRingLayout, HealthRatioLimitsVisibleSoldierRings) {
  Render::GL::SelectionRingLayoutInput input;
  input.spawn_type = Game::Units::SpawnType::MountedKnight;
  input.individuals_per_unit = 6;
  input.max_units_per_row = 3;
  input.health_ratio = 0.5F;
  input.ring_size = 2.0F;

  auto const placements = Render::GL::build_selection_ring_layout(input);

  EXPECT_EQ(placements.size(), 3u);
}

TEST(SelectionRingLayout, UnitYawRotatesFormationOffsets) {
  Render::GL::SelectionRingLayoutInput facing_forward;
  facing_forward.individuals_per_unit = 2;
  facing_forward.max_units_per_row = 2;
  facing_forward.seed = 1234U;
  facing_forward.position = QVector3D(10.0F, 0.0F, 20.0F);

  Render::GL::SelectionRingLayoutInput facing_sideways = facing_forward;
  facing_sideways.rotation = QVector3D(0.0F, 90.0F, 0.0F);

  auto const forward = Render::GL::build_selection_ring_layout(facing_forward);
  auto const sideways =
      Render::GL::build_selection_ring_layout(facing_sideways);

  ASSERT_EQ(forward.size(), 2u);
  ASSERT_EQ(sideways.size(), 2u);

  float const forward_x_extent =
      std::abs(forward[0].world_x - forward[1].world_x);
  float const forward_z_extent =
      std::abs(forward[0].world_z - forward[1].world_z);
  float const sideways_x_extent =
      std::abs(sideways[0].world_x - sideways[1].world_x);
  float const sideways_z_extent =
      std::abs(sideways[0].world_z - sideways[1].world_z);

  EXPECT_GT(forward_x_extent, forward_z_extent);
  EXPECT_GT(sideways_z_extent, sideways_x_extent);
}

TEST(SelectionRingLayout, MountedFormationUsesDeepStaggeredRows) {
  Render::GL::SelectionRingLayoutInput input;
  input.spawn_type = Game::Units::SpawnType::MountedKnight;
  input.individuals_per_unit = 6;
  input.max_units_per_row = 3;
  input.seed = 1234U;

  auto const placements = Render::GL::build_selection_ring_layout(input);

  ASSERT_EQ(placements.size(), 6u);

  float const row_shift_x = placements[3].world_x - placements[0].world_x;
  float const row_shift_z = placements[3].world_z - placements[0].world_z;

  EXPECT_GT(row_shift_x, 0.35F);
  EXPECT_GT(row_shift_z, Render::GL::cavalry_formation_spacing() * 0.95F);
}

TEST(SelectionRingLayout, MultiSoldierRingsUseCompactVisualSize) {
  float const size = Render::GL::Detail::selection_ring_visual_size(
      Game::Units::SpawnType::Archer, 20, 1.2F);

  EXPECT_FLOAT_EQ(size, 0.3F);
}

TEST(SelectionRingLayout, LimitsColumnsToVisibleMembers) {
  Render::GL::SelectionRingLayoutInput input;
  input.individuals_per_unit = 3;
  input.max_units_per_row = 5;

  auto const placements = Render::GL::build_selection_ring_layout(input);

  ASSERT_EQ(placements.size(), 3u);
  EXPECT_LT(std::abs(placements[2].world_x - placements[0].world_x), 2.0F);
}

TEST(SelectionRingLayout, SingleSoldierRingKeepsConfiguredSize) {
  float const size = Render::GL::Detail::selection_ring_visual_size(
      Game::Units::SpawnType::Healer, 1, 1.2F);

  EXPECT_FLOAT_EQ(size, 1.2F);
}

} // namespace
