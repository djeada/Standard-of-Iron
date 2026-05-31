#include <QJsonArray>
#include <QJsonObject>

#include <gtest/gtest.h>

#include "tools/map_editor/hill_projection_model.h"
#include "tools/map_editor/map_json_keys.h"

namespace {

namespace HillProjection = MapEditor::HillProjection;
namespace MapJsonKeys = MapEditor::MapJsonKeys;

auto contains_cell(const QVector<QPoint>& cells, const QPoint& cell) -> bool {
  return cells.contains(cell);
}

auto contains_point(const QJsonArray& entrances, double x, double z) -> bool {
  for (const QJsonValue& value : entrances) {
    const QJsonObject obj = value.toObject();
    if (!obj.contains(MapJsonKeys::x) || !obj.contains(MapJsonKeys::z)) {
      continue;
    }
    if (obj.value(MapJsonKeys::x).toDouble() == x &&
        obj.value(MapJsonKeys::z).toDouble() == z) {
      return true;
    }
  }
  return false;
}

TEST(HillProjectionModelTest, UsesFixedEightyByEightyProjectionGrid) {
  const QJsonObject hill{{MapJsonKeys::type, "hill"},
                         {MapJsonKeys::x, 12.0},
                         {MapJsonKeys::z, 18.0},
                         {MapJsonKeys::radius, 8.0}};

  const HillProjection::Model model = HillProjection::build_model(hill);
  EXPECT_EQ(model.grid_width, 80);
  EXPECT_EQ(model.grid_height, 80);
}

TEST(HillProjectionModelTest, AdjacentEntranceCellsCollapseToSingleJsonEntry) {
  const HillProjection::Model model{
      80, 80, 0.0, 0.0, 40.0, 40.0, 0.0, 10.0, 10.0, 10.0, false, {}, {}, QJsonArray{}};
  const QVector<QPoint> entrance_cells{
      QPoint(10, 10), QPoint(11, 10), QPoint(12, 10), QPoint(12, 11)};

  const QJsonArray entrances =
      HillProjection::entrances_from_cells(model, entrance_cells);
  ASSERT_EQ(entrances.size(), 1);
  const QJsonObject grouped = entrances.first().toObject();
  EXPECT_TRUE(grouped.contains(MapJsonKeys::x));
  EXPECT_TRUE(grouped.contains(MapJsonKeys::z));
  ASSERT_TRUE(grouped.contains(MapJsonKeys::radius));
  EXPECT_GT(grouped.value(MapJsonKeys::radius).toDouble(), 0.0);
  EXPECT_FALSE(grouped.contains("cells"));
}

TEST(HillProjectionModelTest, RadiusEntrancesExpandToMultipleEditableCells) {
  const QJsonObject hill{{MapJsonKeys::type, "hill"},
                         {MapJsonKeys::x, 40.0},
                         {MapJsonKeys::z, 40.0},
                         {MapJsonKeys::width, 8.0},
                         {MapJsonKeys::depth, 8.0},
                         {MapJsonKeys::entrances,
                          QJsonArray{QJsonObject{{MapJsonKeys::x, 30.0},
                                                 {MapJsonKeys::z, 30.0},
                                                 {MapJsonKeys::radius, 1.2}}}}};

  const HillProjection::Model model = HillProjection::build_model(hill);
  EXPECT_GT(model.entrance_cells.size(), 1);
}

TEST(HillProjectionModelTest, GroupedEntranceRadiusRoundTripsToMultipleCells) {
  const HillProjection::Model model{
      80, 80, 0.0, 0.0, 40.0, 40.0, 0.0, 10.0, 10.0, 10.0, false, {}, {}, QJsonArray{}};
  const QVector<QPoint> grouped_cells{QPoint(10, 10), QPoint(11, 10)};

  const QJsonArray entrances =
      HillProjection::entrances_from_cells(model, grouped_cells);
  ASSERT_EQ(entrances.size(), 1);

  QJsonObject hill{{MapJsonKeys::type, "hill"},
                   {MapJsonKeys::x, 40.0},
                   {MapJsonKeys::z, 40.0},
                   {MapJsonKeys::width, 8.0},
                   {MapJsonKeys::depth, 8.0},
                   {MapJsonKeys::entrances, entrances}};

  const HillProjection::Model reloaded = HillProjection::build_model(hill);
  EXPECT_GT(reloaded.entrance_cells.size(), 1);
}

TEST(HillProjectionModelTest, RadiusBasedHillProjectionUsesRuntimeHalfExtents) {
  const QJsonObject hill{{MapJsonKeys::type, "hill"},
                         {MapJsonKeys::x, 40.0},
                         {MapJsonKeys::z, 40.0},
                         {MapJsonKeys::radius, 2.0}};

  const HillProjection::Model model = HillProjection::build_model(hill);
  EXPECT_TRUE(contains_cell(model.hill_cells, QPoint(40, 40)));
  EXPECT_TRUE(contains_cell(model.hill_cells, QPoint(42, 40)));
  EXPECT_FALSE(contains_cell(model.hill_cells, QPoint(43, 40)));
}

TEST(HillProjectionModelTest, ProjectionApplyUpdatesHillSizeAndCenter) {
  const QJsonObject hill{{MapJsonKeys::type, "hill"},
                         {MapJsonKeys::x, 40.0},
                         {MapJsonKeys::z, 40.0},
                         {MapJsonKeys::width, 10.0},
                         {MapJsonKeys::depth, 10.0}};

  const HillProjection::Model model = HillProjection::build_model(hill);
  const QVector<QPoint> hill_cells{
      QPoint(20, 20), QPoint(21, 20), QPoint(22, 20), QPoint(20, 21), QPoint(22, 21)};
  const QVector<QPoint> entrance_cells{QPoint(20, 20), QPoint(22, 21)};

  const QJsonObject updated = HillProjection::apply_projection_to_hill_json(
      hill, model, hill_cells, entrance_cells);
  EXPECT_DOUBLE_EQ(updated.value(MapJsonKeys::width).toDouble(), 1.0);
  EXPECT_DOUBLE_EQ(updated.value(MapJsonKeys::depth).toDouble(), 0.5);
  EXPECT_DOUBLE_EQ(updated.value(MapJsonKeys::x).toDouble(), model.origin_x + 21.0);
  EXPECT_DOUBLE_EQ(updated.value(MapJsonKeys::z).toDouble(), model.origin_z + 20.5);
  ASSERT_TRUE(updated.value(MapJsonKeys::entrances).isArray());
  const QJsonArray entrances = updated.value(MapJsonKeys::entrances).toArray();
  ASSERT_EQ(entrances.size(), 2);
  EXPECT_TRUE(contains_point(entrances, model.origin_x + 20.0, model.origin_z + 20.0));
  EXPECT_TRUE(contains_point(entrances, model.origin_x + 22.0, model.origin_z + 21.0));
}

TEST(HillProjectionModelTest, CircularHillBodyRoundTripsBackToRadius) {
  const QJsonObject hill{{MapJsonKeys::type, "hill"},
                         {MapJsonKeys::x, 40.0},
                         {MapJsonKeys::z, 40.0},
                         {MapJsonKeys::radius, 6.0},
                         {MapJsonKeys::height, 3.0},
                         {MapJsonKeys::rotation, 30.0}};

  const HillProjection::Model model = HillProjection::build_model(hill);
  const QJsonObject updated = HillProjection::apply_projection_to_hill_json(
      hill, model, model.hill_cells, model.entrance_cells);
  EXPECT_DOUBLE_EQ(updated.value(MapJsonKeys::x).toDouble(), 40.0);
  EXPECT_DOUBLE_EQ(updated.value(MapJsonKeys::z).toDouble(), 40.0);
  EXPECT_NEAR(updated.value(MapJsonKeys::radius).toDouble(), 6.0, 1e-3);
  EXPECT_FALSE(updated.contains(MapJsonKeys::width));
  EXPECT_FALSE(updated.contains(MapJsonKeys::depth));
}

TEST(HillProjectionModelTest, MountainProjectionRoundTripsBodyWithoutEntrances) {
  const QJsonObject mountain{
      {MapJsonKeys::type, "mountain"},
      {MapJsonKeys::x, 40.0},
      {MapJsonKeys::z, 40.0},
      {MapJsonKeys::radius, 6.0},
      {MapJsonKeys::rotation, 45.0},
      {MapJsonKeys::entrances,
       QJsonArray{QJsonObject{{MapJsonKeys::x, 42.0}, {MapJsonKeys::z, 42.0}}}}};

  const HillProjection::Model model = HillProjection::build_model(mountain);
  EXPECT_TRUE(contains_cell(model.hill_cells, QPoint(40, 40)));
  EXPECT_TRUE(contains_cell(model.hill_cells, QPoint(47, 47)));
  EXPECT_FALSE(contains_cell(model.hill_cells, QPoint(40, 52)));

  const QJsonObject updated = HillProjection::apply_projection_to_hill_json(
      mountain, model, model.hill_cells, {});
  EXPECT_DOUBLE_EQ(updated.value(MapJsonKeys::x).toDouble(), 40.0);
  EXPECT_DOUBLE_EQ(updated.value(MapJsonKeys::z).toDouble(), 40.0);
  EXPECT_NEAR(updated.value(MapJsonKeys::radius).toDouble(), 6.0, 1e-3);
  EXPECT_FALSE(updated.contains(MapJsonKeys::width));
  EXPECT_FALSE(updated.contains(MapJsonKeys::depth));
  EXPECT_FALSE(updated.contains(MapJsonKeys::entrances));
}

} // namespace
