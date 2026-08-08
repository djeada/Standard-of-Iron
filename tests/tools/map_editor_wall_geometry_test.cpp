#include <QPointF>
#include <QVector>

#include <gtest/gtest.h>

#include "tools/map_editor/wall_geometry.h"

namespace {

namespace WG = MapEditor::WallGeometry;

auto wall(float x0, float z0, float x1, float z1) -> MapEditor::LinearElement {
  MapEditor::LinearElement elem;
  elem.type = QStringLiteral("wall");
  elem.start = QVector2D(x0, z0);
  elem.end = QVector2D(x1, z1);
  elem.width = WG::k_lattice;
  elem.player_id = 2;
  elem.nation = QStringLiteral("carthage");
  return elem;
}

TEST(MapEditorWallGeometryTest, EverythingLandsOnTheTwoCellWallLattice) {
  EXPECT_FLOAT_EQ(WG::snap(0.0F), 0.0F);
  EXPECT_FLOAT_EQ(WG::snap(1.0F), 2.0F);
  EXPECT_FLOAT_EQ(WG::snap(2.9F), 2.0F);
  EXPECT_FLOAT_EQ(WG::snap(523.4F), 524.0F);
  EXPECT_FLOAT_EQ(WG::snap(-3.0F), -4.0F);
}

TEST(MapEditorWallGeometryTest, AGateClickedOnARunTakesThatRunsAxisAndLine) {
  const QVector<MapEditor::LinearElement> walls{wall(508, 298, 542, 298)};

  const WG::GatePlacement across = WG::plan_gate(walls, QPointF(523.4, 299.1));
  EXPECT_EQ(across.wall_index, 0);
  EXPECT_TRUE(across.horizontal);
  EXPECT_FLOAT_EQ(across.rotation, 0.0F);
  EXPECT_FLOAT_EQ(across.x, 524.0F) << "snapped along the run";
  EXPECT_FLOAT_EQ(across.z, 298.0F) << "pulled onto the run's own line";

  const QVector<MapEditor::LinearElement> uprights{wall(508, 300, 508, 326)};
  const WG::GatePlacement along = WG::plan_gate(uprights, QPointF(509.2, 313.1));
  EXPECT_EQ(along.wall_index, 0);
  EXPECT_FALSE(along.horizontal);
  EXPECT_FLOAT_EQ(along.rotation, 90.0F);
  EXPECT_FLOAT_EQ(along.x, 508.0F);
  EXPECT_FLOAT_EQ(along.z, 314.0F);
}

TEST(MapEditorWallGeometryTest, AGateClickedAwayFromEveryRunStandsFree) {
  const QVector<MapEditor::LinearElement> walls{wall(508, 298, 542, 298)};

  const WG::GatePlacement plan = WG::plan_gate(walls, QPointF(523.0, 340.0));
  EXPECT_EQ(plan.wall_index, -1);
  EXPECT_FLOAT_EQ(plan.x, 524.0F);
  EXPECT_FLOAT_EQ(plan.z, 340.0F);
}

TEST(MapEditorWallGeometryTest, TheRunBreaksIntoTwoPiecesAroundTheGateOpening) {
  const MapEditor::LinearElement run = wall(508, 298, 542, 298);

  const auto low = WG::trim_run_to_gate(run, true, 524.0F, true);
  const auto high = WG::trim_run_to_gate(run, true, 524.0F, false);
  ASSERT_TRUE(low.has_value());
  ASSERT_TRUE(high.has_value());

  EXPECT_FLOAT_EQ(low->start.x(), 508.0F);
  EXPECT_FLOAT_EQ(low->end.x(), 520.0F);
  EXPECT_FLOAT_EQ(high->start.x(), 528.0F);
  EXPECT_FLOAT_EQ(high->end.x(), 542.0F);

  EXPECT_FLOAT_EQ(high->start.x() - low->end.x() - WG::k_lattice, WG::k_gate_span);

  EXPECT_EQ(low->player_id, run.player_id);
  EXPECT_EQ(high->nation, run.nation);
}

TEST(MapEditorWallGeometryTest, AGateAtARunsEndLeavesOnlyThePieceThatSurvives) {
  const MapEditor::LinearElement run = wall(508, 298, 520, 298);

  EXPECT_FALSE(WG::trim_run_to_gate(run, true, 508.0F, true).has_value())
      << "nothing is left on the low side of a gate sitting at the low end";
  const auto high = WG::trim_run_to_gate(run, true, 508.0F, false);
  ASSERT_TRUE(high.has_value());
  EXPECT_FLOAT_EQ(high->start.x(), 512.0F);
  EXPECT_FLOAT_EQ(high->end.x(), 520.0F);
}

TEST(MapEditorWallGeometryTest, AGateSwallowingAShortRunLeavesNothingBehind) {
  const MapEditor::LinearElement run = wall(520, 298, 528, 298);

  EXPECT_FALSE(WG::trim_run_to_gate(run, true, 524.0F, true).has_value());
  EXPECT_FALSE(WG::trim_run_to_gate(run, true, 524.0F, false).has_value());
}

} // namespace
