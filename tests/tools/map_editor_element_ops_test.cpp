#include <gtest/gtest.h>

#include "tools/map_editor/element_ops.h"

namespace {

using MapEditor::ElementKind;
using MapEditor::ElementSnapshot;
namespace Ops = MapEditor::ElementOps;

auto make_structure(float x, float z, int player_id) -> MapEditor::StructureElement {
  MapEditor::StructureElement elem;
  elem.type = QStringLiteral("barracks");
  elem.x = x;
  elem.z = z;
  elem.player_id = player_id;
  return elem;
}

auto make_linear(QVector2D start, QVector2D end) -> MapEditor::LinearElement {
  MapEditor::LinearElement elem;
  elem.type = QStringLiteral("road");
  elem.start = start;
  elem.end = end;
  return elem;
}

TEST(MapEditorElementOpsTest, SnapshotRoundTripsThroughMapData) {
  MapEditor::MapData data;
  data.add_structure(make_structure(4.0F, 6.0F, 2));

  const ElementSnapshot snap =
      Ops::snapshot(data, static_cast<int>(ElementKind::Structure), 0);

  EXPECT_EQ(Ops::kind_of(snap), static_cast<int>(ElementKind::Structure));
  EXPECT_EQ(Ops::type_name(snap), QStringLiteral("barracks"));
  ASSERT_TRUE(Ops::position(snap).has_value());
  EXPECT_DOUBLE_EQ(Ops::position(snap)->x(), 4.0);
  EXPECT_DOUBLE_EQ(Ops::position(snap)->y(), 6.0);
  EXPECT_EQ(Ops::player_id(snap), 2);
}

TEST(MapEditorElementOpsTest, InvalidIndexYieldsEmptySnapshot) {
  MapEditor::MapData data;

  EXPECT_EQ(Ops::kind_of(Ops::snapshot(data, 0, 0)), -1);
  EXPECT_EQ(Ops::kind_of(Ops::snapshot(data, 99, 0)), -1);
  EXPECT_FALSE(Ops::position(Ops::snapshot(data, 3, -1)).has_value());
  EXPECT_EQ(Ops::make_remove(data, 3, 0), nullptr);
}

TEST(MapEditorElementOpsTest, LinearElementAnchorIsItsCentre) {
  const ElementSnapshot snap =
      ElementSnapshot{make_linear(QVector2D(0.0F, 0.0F), QVector2D(10.0F, 4.0F))};

  ASSERT_TRUE(Ops::position(snap).has_value());
  EXPECT_DOUBLE_EQ(Ops::position(snap)->x(), 5.0);
  EXPECT_DOUBLE_EQ(Ops::position(snap)->y(), 2.0);

  const ElementSnapshot moved = Ops::moved_to(snap, QPointF(15.0, 2.0));
  const auto& line = std::get<MapEditor::LinearElement>(moved);
  EXPECT_FLOAT_EQ(line.start.x(), 10.0F);
  EXPECT_FLOAT_EQ(line.end.x(), 20.0F);
  EXPECT_FLOAT_EQ(line.start.y(), 0.0F);
  EXPECT_FLOAT_EQ(line.end.y(), 4.0F);
  EXPECT_TRUE(Ops::has_moved(snap, moved));
}

TEST(MapEditorElementOpsTest, SnapToGridRoundsEveryCoordinate) {
  const ElementSnapshot snap =
      ElementSnapshot{make_linear(QVector2D(1.4F, 2.6F), QVector2D(8.5F, 3.2F))};
  const auto& snapped = std::get<MapEditor::LinearElement>(Ops::snapped_to_grid(snap));

  EXPECT_FLOAT_EQ(snapped.start.x(), 1.0F);
  EXPECT_FLOAT_EQ(snapped.start.y(), 3.0F);
  EXPECT_FLOAT_EQ(snapped.end.x(), 9.0F);
  EXPECT_FLOAT_EQ(snapped.end.y(), 3.0F);

  const ElementSnapshot already_on_grid =
      ElementSnapshot{make_structure(2.0F, 3.0F, 0)};
  EXPECT_FALSE(Ops::has_moved(already_on_grid, Ops::snapped_to_grid(already_on_grid)));
}

TEST(MapEditorElementOpsTest, PlayerIdOnlyAppliesToOwnedElements) {
  const ElementSnapshot structure = ElementSnapshot{make_structure(0.0F, 0.0F, 1)};
  EXPECT_TRUE(Ops::supports_player_id(structure));
  EXPECT_EQ(Ops::player_id(Ops::with_player_id(structure, 3)), 3);

  MapEditor::WorldPropElement prop;
  prop.type = QStringLiteral("tent");
  const ElementSnapshot prop_snap{prop};
  EXPECT_FALSE(Ops::supports_player_id(prop_snap));
  EXPECT_EQ(Ops::player_id(prop_snap), -1);

  MapEditor::LinearElement wall = make_linear(QVector2D(), QVector2D(1.0F, 0.0F));
  wall.type = QStringLiteral("wall");
  EXPECT_TRUE(Ops::supports_player_id(ElementSnapshot{wall}));
  EXPECT_FALSE(Ops::supports_player_id(
      ElementSnapshot{make_linear(QVector2D(), QVector2D(1.0F, 0.0F))}));
}

TEST(MapEditorElementOpsTest, AddRemoveAndUpdateCommandsUndoCleanly) {
  MapEditor::MapData data;
  const ElementSnapshot original = ElementSnapshot{make_structure(3.0F, 3.0F, 1)};

  data.execute_command(Ops::make_add(data, original));
  ASSERT_EQ(data.structures().size(), 1);

  const ElementSnapshot moved = Ops::translated(original, QPointF(2.0, -1.0));
  data.execute_command(Ops::make_update(data, 0, original, moved, "Move barracks"));
  EXPECT_FLOAT_EQ(data.structures()[0].x, 5.0F);
  EXPECT_FLOAT_EQ(data.structures()[0].z, 2.0F);

  data.undo();
  EXPECT_FLOAT_EQ(data.structures()[0].x, 3.0F);

  data.execute_command(
      Ops::make_remove(data, static_cast<int>(ElementKind::Structure), 0));
  EXPECT_TRUE(data.structures().isEmpty());

  data.undo();
  ASSERT_EQ(data.structures().size(), 1);
  EXPECT_FLOAT_EQ(data.structures()[0].x, 3.0F);
}

TEST(MapEditorElementOpsTest, UpdateAcrossDifferentKindsIsRejected) {
  MapEditor::MapData data;
  const ElementSnapshot structure = ElementSnapshot{make_structure(0.0F, 0.0F, 0)};
  const ElementSnapshot line =
      ElementSnapshot{make_linear(QVector2D(), QVector2D(1.0F, 1.0F))};

  EXPECT_EQ(Ops::make_update(data, 0, structure, line, "mismatch"), nullptr);
}

TEST(MapEditorElementOpsTest, RemovingManyElementsUndoesBackToTheOriginalOrder) {
  MapEditor::MapData data;
  for (int i = 0; i < 4; ++i) {
    MapEditor::StructureElement elem = make_structure(float(i), 0.0F, 0);
    elem.type = QStringLiteral("barracks_%1").arg(i);
    data.add_structure(elem);
  }

  data.execute_command(Ops::make_remove_many(
      data, {MapEditor::ElementRef{3, 0}, MapEditor::ElementRef{3, 2}}));

  ASSERT_EQ(data.structures().size(), 2);
  EXPECT_EQ(data.structures()[0].type, QStringLiteral("barracks_1"));
  EXPECT_EQ(data.structures()[1].type, QStringLiteral("barracks_3"));

  data.undo();
  ASSERT_EQ(data.structures().size(), 4);
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(data.structures()[i].type, QStringLiteral("barracks_%1").arg(i));
  }
}

TEST(MapEditorElementOpsTest, AddingManyElementsIsOneUndoStep) {
  MapEditor::MapData data;
  QVector<MapEditor::ElementSnapshot> snaps;
  snaps.append(MapEditor::ElementSnapshot{make_structure(1.0F, 1.0F, 0)});
  snaps.append(MapEditor::ElementSnapshot{make_structure(2.0F, 2.0F, 0)});
  snaps.append(MapEditor::ElementSnapshot{
      make_linear(QVector2D(0.0F, 0.0F), QVector2D(4.0F, 0.0F))});

  data.execute_command(Ops::make_add_many(data, snaps));
  EXPECT_EQ(data.structures().size(), 2);
  EXPECT_EQ(data.linear_elements().size(), 1);

  data.undo();
  EXPECT_TRUE(data.structures().isEmpty());
  EXPECT_TRUE(data.linear_elements().isEmpty());
}

TEST(MapEditorElementOpsTest, UpdatingManyElementsIsOneUndoStep) {
  MapEditor::MapData data;
  data.add_structure(make_structure(1.0F, 1.0F, 0));
  data.add_structure(make_structure(5.0F, 5.0F, 0));

  const QVector<MapEditor::ElementRef> refs{MapEditor::ElementRef{3, 0},
                                            MapEditor::ElementRef{3, 1}};
  const QVector<MapEditor::ElementSnapshot> before = Ops::snapshots(data, refs);
  QVector<MapEditor::ElementSnapshot> after;
  for (const MapEditor::ElementSnapshot& snap : before) {
    after.append(Ops::translated(snap, QPointF(3.0, 0.0)));
  }

  data.execute_command(Ops::make_update_many(data, refs, before, after, "Move 2"));
  EXPECT_FLOAT_EQ(data.structures()[0].x, 4.0F);
  EXPECT_FLOAT_EQ(data.structures()[1].x, 8.0F);

  data.undo();
  EXPECT_FLOAT_EQ(data.structures()[0].x, 1.0F);
  EXPECT_FLOAT_EQ(data.structures()[1].x, 5.0F);
}

TEST(MapEditorElementOpsTest, BulkHelpersRejectMismatchedOrEmptyInput) {
  MapEditor::MapData data;
  data.add_structure(make_structure(0.0F, 0.0F, 0));

  EXPECT_EQ(Ops::make_remove_many(data, {}), nullptr);
  EXPECT_EQ(Ops::make_add_many(data, {}), nullptr);
  EXPECT_EQ(Ops::make_update_many(data,
                                  {MapEditor::ElementRef{3, 0}},
                                  Ops::snapshots(data, {MapEditor::ElementRef{3, 0}}),
                                  {},
                                  "mismatch"),
            nullptr);

  EXPECT_EQ(Ops::make_remove_many(data, {MapEditor::ElementRef{3, 7}}), nullptr);
}

TEST(MapEditorElementOpsTest, GroupAnchorUsesTheFirstPositionedElement) {
  QVector<MapEditor::ElementSnapshot> snaps;
  snaps.append(MapEditor::ElementSnapshot{});
  snaps.append(MapEditor::ElementSnapshot{make_structure(6.0F, 9.0F, 0)});

  const std::optional<QPointF> anchor = Ops::group_anchor(snaps);
  ASSERT_TRUE(anchor.has_value());
  EXPECT_DOUBLE_EQ(anchor->x(), 6.0);
  EXPECT_DOUBLE_EQ(anchor->y(), 9.0);
  EXPECT_FALSE(Ops::group_anchor({}).has_value());
}

TEST(MapEditorElementOpsTest, SummaryMentionsCategoryTypeAndPosition) {
  const QString text = Ops::summary(ElementSnapshot{make_structure(12.0F, 7.0F, 2)});

  EXPECT_TRUE(text.contains(QStringLiteral("Structure")));
  EXPECT_TRUE(text.contains(QStringLiteral("barracks")));
  EXPECT_TRUE(text.contains(QStringLiteral("12.0")));
  EXPECT_TRUE(text.contains(QStringLiteral("player 2")));
  EXPECT_TRUE(Ops::summary(ElementSnapshot{}).isEmpty());
}

} // namespace
