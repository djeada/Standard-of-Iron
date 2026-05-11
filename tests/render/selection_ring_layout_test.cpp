#include "game/units/troop_config.h"
#include "render/humanoid/prepare.h"
#include "render/selection_ring_layout.h"

#include <gtest/gtest.h>

#include <QMatrix4x4>
#include <cmath>

namespace {

auto build_expected_soldier_positions(
    const Render::GL::SelectionRingLayoutInput &input)
    -> std::vector<Render::GL::SelectionRingPlacement> {
  int const total_units = std::max(1, input.individuals_per_unit);
  int const cols = std::max(1, std::min(input.max_units_per_row, total_units));
  int const rows = std::max(1, (total_units + cols - 1) / cols);
  auto const category =
      input.is_builder_constructing
          ? Render::GL::FormationCalculatorFactory::UnitCategory::
                BuilderConstruction
          : Render::GL::Detail::selection_ring_category(input.spawn_type);
  auto const *calculator =
      Render::GL::FormationCalculatorFactory::get_calculator(
          Render::GL::Detail::selection_ring_nation(input.nation_id), category);
  EXPECT_NE(calculator, nullptr);

  std::vector<Render::GL::SelectionRingPlacement> placements;
  placements.reserve(static_cast<std::size_t>(total_units));
  for (int idx = 0; idx < total_units; ++idx) {
    Render::Humanoid::SoldierLayoutInputs soldier_inputs{};
    soldier_inputs.idx = idx;
    soldier_inputs.row = idx / cols;
    soldier_inputs.col = idx % cols;
    soldier_inputs.rows = rows;
    soldier_inputs.cols = cols;
    soldier_inputs.formation_spacing = input.formation_spacing;
    soldier_inputs.seed = input.seed;

    auto const layout =
        Render::Humanoid::build_soldier_layout(*calculator, soldier_inputs);

    QMatrix4x4 model;
    model.translate(input.position);
    model.rotate(input.rotation.y() + layout.yaw_offset, 0.0F, 1.0F, 0.0F);
    model.scale(input.scale);
    model.translate(layout.offset_x, 0.0F, layout.offset_z);

    QVector3D const world = model.map(QVector3D(0.0F, 0.0F, 0.0F));
    placements.push_back({world.x(), world.z(), input.ring_size});
  }

  return placements;
}

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

TEST(SelectionRingLayout, SwordsmanRingsFollowSameLayoutAsHumanoidFormation) {
  Render::GL::SelectionRingLayoutInput input;
  input.spawn_type = Game::Units::SpawnType::Knight;
  input.individuals_per_unit = 6;
  input.max_units_per_row = 3;
  input.formation_spacing =
      Game::Units::TroopConfig::instance().get_formation_spacing(
          input.spawn_type);
  input.seed = 0x12345678U;
  input.position = QVector3D(3.0F, 0.0F, -4.0F);

  auto const rings = Render::GL::build_selection_ring_layout(input);
  auto const expected = build_expected_soldier_positions(input);

  ASSERT_EQ(rings.size(), expected.size());
  for (std::size_t i = 0; i < rings.size(); ++i) {
    EXPECT_NEAR(rings[i].world_x, expected[i].world_x, 1.0e-4F) << "idx=" << i;
    EXPECT_NEAR(rings[i].world_z, expected[i].world_z, 1.0e-4F) << "idx=" << i;
  }
}

TEST(SelectionRingLayout, SpearmanRingsFollowSameLayoutAsHumanoidFormation) {
  Render::GL::SelectionRingLayoutInput input;
  input.spawn_type = Game::Units::SpawnType::Spearman;
  input.individuals_per_unit = 6;
  input.max_units_per_row = 3;
  input.formation_spacing =
      Game::Units::TroopConfig::instance().get_formation_spacing(
          input.spawn_type);
  input.seed = 0x5A17B00BU;
  input.position = QVector3D(-2.0F, 0.0F, 6.0F);

  auto const rings = Render::GL::build_selection_ring_layout(input);
  auto const expected = build_expected_soldier_positions(input);

  ASSERT_EQ(rings.size(), expected.size());
  for (std::size_t i = 0; i < rings.size(); ++i) {
    EXPECT_NEAR(rings[i].world_x, expected[i].world_x, 1.0e-4F) << "idx=" << i;
    EXPECT_NEAR(rings[i].world_z, expected[i].world_z, 1.0e-4F) << "idx=" << i;
  }
}

TEST(SelectionRingLayout,
     BuilderConstructionRingsFollowSameLayoutAsHumanoidFormation) {
  Render::GL::SelectionRingLayoutInput input;
  input.spawn_type = Game::Units::SpawnType::Builder;
  input.individuals_per_unit = 8;
  input.max_units_per_row = 8;
  input.formation_spacing =
      Game::Units::TroopConfig::instance().get_formation_spacing(
          input.spawn_type);
  input.seed = 0x00C0FFEEU;
  input.position = QVector3D(1.0F, 0.0F, 2.0F);
  input.is_builder_constructing = true;

  auto const rings = Render::GL::build_selection_ring_layout(input);
  auto const expected = build_expected_soldier_positions(input);

  ASSERT_EQ(rings.size(), expected.size());
  for (std::size_t i = 0; i < rings.size(); ++i) {
    EXPECT_NEAR(rings[i].world_x, expected[i].world_x, 1.0e-4F) << "idx=" << i;
    EXPECT_NEAR(rings[i].world_z, expected[i].world_z, 1.0e-4F) << "idx=" << i;
  }
}

} // namespace
