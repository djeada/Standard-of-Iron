#include <QJsonArray>
#include <QJsonObject>

#include <gtest/gtest.h>

#include "game/map/terrain_footprint.h"
#include "tools/map_editor/hill_projection_model.h"
#include "tools/map_editor/map_json_keys.h"

namespace {

namespace HillProjection = MapEditor::HillProjection;
namespace MapJsonKeys = MapEditor::MapJsonKeys;

auto contains_cell(const QVector<QPoint>& cells, const QPoint& cell) -> bool {
  return cells.contains(cell);
}

auto contains_point(const QJsonArray& entrances, double x, double z) -> bool {
  for (const QJsonValue value : entrances) {
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

auto square_model(int size) -> HillProjection::Model {
  HillProjection::Model model;
  model.grid_width = size;
  model.grid_height = size;
  model.center_x = size * 0.5;
  model.center_z = size * 0.5;
  model.hill_half_width = 10.0;
  model.hill_half_depth = 10.0;
  model.base_radius = 10.0;
  return model;
}

auto filled_rect_cells(int x0, int z0, int x1, int z1) -> QVector<QPoint> {
  QVector<QPoint> cells;
  for (int z = z0; z <= z1; ++z) {
    for (int x = x0; x <= x1; ++x) {
      cells.append(QPoint(x, z));
    }
  }
  return cells;
}

} // namespace

TEST(HillProjectionModelTest, ProjectionGridFitsTheAuthoredFootprint) {
  const QJsonObject small{{MapJsonKeys::type, "hill"},
                          {MapJsonKeys::x, 12.0},
                          {MapJsonKeys::z, 18.0},
                          {MapJsonKeys::radius, 8.0}};
  const QJsonObject large{{MapJsonKeys::type, "hill"},
                          {MapJsonKeys::x, 400.0},
                          {MapJsonKeys::z, 400.0},
                          {MapJsonKeys::width, 120.0},
                          {MapJsonKeys::depth, 90.0}};

  const HillProjection::Model small_model = HillProjection::build_model(small);
  const HillProjection::Model large_model = HillProjection::build_model(large);

  EXPECT_EQ(small_model.grid_width, small_model.grid_height);
  EXPECT_GT(large_model.grid_width, small_model.grid_width);
  EXPECT_LE(large_model.grid_width, HillProjection::k_max_projection_size + 1);

  const int half_span = (large_model.grid_width - 1) / 2;
  EXPECT_DOUBLE_EQ(large_model.origin_x, 400.0 - half_span);
  EXPECT_TRUE(contains_cell(large_model.hill_cells, QPoint(half_span + 59, half_span)));
  EXPECT_FALSE(
      contains_cell(large_model.hill_cells, QPoint(half_span + 61, half_span)));
}

TEST(HillProjectionModelTest, AuthoredWidthIsAFullExtentLikeTheRuntime) {
  const QJsonObject hill{{MapJsonKeys::type, "hill"},
                         {MapJsonKeys::x, 40.0},
                         {MapJsonKeys::z, 40.0},
                         {MapJsonKeys::width, 20.0},
                         {MapJsonKeys::depth, 20.0}};

  const HillProjection::Model model = HillProjection::build_model(hill);
  const auto runtime = Game::Map::hill_footprint_cells(
      {.width = 20.0F, .depth = 20.0F, .radius = 0.0F, .tile_size = 1.0F});

  EXPECT_DOUBLE_EQ(model.hill_half_width, runtime.half_width);
  EXPECT_DOUBLE_EQ(model.hill_half_depth, runtime.half_depth);
  EXPECT_DOUBLE_EQ(model.hill_half_width, 10.0);
}

TEST(HillProjectionModelTest, RadiusBasedHillProjectionUsesRuntimeHalfExtents) {
  const QJsonObject hill{{MapJsonKeys::type, "hill"},
                         {MapJsonKeys::x, 40.0},
                         {MapJsonKeys::z, 40.0},
                         {MapJsonKeys::radius, 2.0}};

  const HillProjection::Model model = HillProjection::build_model(hill);
  const int half_span = (model.grid_width - 1) / 2;
  EXPECT_TRUE(contains_cell(model.hill_cells, QPoint(half_span, half_span)));
  EXPECT_TRUE(contains_cell(model.hill_cells, QPoint(half_span + 2, half_span)));
  EXPECT_FALSE(contains_cell(model.hill_cells, QPoint(half_span + 3, half_span)));
}

TEST(HillProjectionModelTest, CampaignRadiusHillsMatchTheRuntimeStretchAndSpin) {
  const QJsonObject hill{{MapJsonKeys::type, "hill"},
                         {MapJsonKeys::x, 300.0},
                         {MapJsonKeys::z, 220.0},
                         {MapJsonKeys::radius, 20.0}};
  const HillProjection::MapContext context{
      .tile_size = 1.0, .map_grid_width = 650, .map_grid_height = 650};

  const HillProjection::Model model = HillProjection::build_model(hill, context);
  const auto runtime = Game::Map::hill_footprint_cells({.width = 0.0F,
                                                        .depth = 0.0F,
                                                        .radius = 20.0F,
                                                        .rotation_deg = 0.0F,
                                                        .tile_size = 1.0F,
                                                        .grid_center_x = 300.0F,
                                                        .grid_center_z = 220.0F,
                                                        .campaign_scale = true});

  EXPECT_DOUBLE_EQ(model.hill_half_width, runtime.half_width);
  EXPECT_DOUBLE_EQ(model.runtime_rotation_deg, runtime.rotation_deg);
  EXPECT_GT(model.hill_half_width, model.hill_half_depth);
  EXPECT_NE(model.runtime_rotation_deg, model.rotation_deg);
}

TEST(HillProjectionModelTest, AdjacentEntranceCellsCollapseToSingleJsonEntry) {
  const HillProjection::Model model = square_model(80);
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
                         {MapJsonKeys::width, 16.0},
                         {MapJsonKeys::depth, 16.0},
                         {MapJsonKeys::entrances,
                          QJsonArray{QJsonObject{{MapJsonKeys::x, 32.0},
                                                 {MapJsonKeys::z, 40.0},
                                                 {MapJsonKeys::radius, 1.2}}}}};

  const HillProjection::Model model = HillProjection::build_model(hill);
  EXPECT_GT(model.entrance_cells.size(), 1);
}

TEST(HillProjectionModelTest, RimCellsAreTheOutlineOfTheBody) {
  const QVector<QPoint> body = filled_rect_cells(10, 10, 14, 14);
  const QVector<QPoint> rim = HillProjection::rim_cells(body);

  EXPECT_EQ(rim.size(), body.size() - 9);
  EXPECT_TRUE(contains_cell(rim, QPoint(10, 10)));
  EXPECT_TRUE(contains_cell(rim, QPoint(12, 14)));
  EXPECT_FALSE(contains_cell(rim, QPoint(12, 12)));
}

TEST(HillProjectionModelTest, RampsCentredAwayFromTheEdgeAreDropped) {
  const HillProjection::Model model = square_model(40);
  const QVector<QPoint> body = filled_rect_cells(10, 10, 20, 20);
  const QVector<QPoint> painted{QPoint(15, 15), QPoint(10, 15), QPoint(30, 30)};

  const QVector<QPoint> normalized =
      HillProjection::normalize_entrance_cells(model, body, painted);

  ASSERT_EQ(normalized.size(), 1);
  EXPECT_EQ(normalized.first(), QPoint(10, 15));

  const QStringList issues = HillProjection::entrance_issues(model, body, painted);
  ASSERT_EQ(issues.size(), 1);
  EXPECT_TRUE(
      issues.first().contains(QStringLiteral("centre away from the hill edge")));
}

TEST(HillProjectionModelTest, RampBlobStraddlingTheEdgeIsKeptAsOneRadiusEntry) {
  const HillProjection::Model model = square_model(40);
  const QVector<QPoint> body = filled_rect_cells(10, 10, 20, 20);
  const QVector<QPoint> painted{
      QPoint(9, 14), QPoint(10, 14), QPoint(10, 15), QPoint(11, 15), QPoint(10, 16)};

  const QVector<QPoint> normalized =
      HillProjection::normalize_entrance_cells(model, body, painted);
  EXPECT_EQ(normalized.size(), painted.size());

  const QJsonArray entrances = HillProjection::entrances_from_cells(model, normalized);
  ASSERT_EQ(entrances.size(), 1);
  const QJsonObject ramp = entrances.first().toObject();
  EXPECT_TRUE(ramp.contains(MapJsonKeys::x));
  EXPECT_TRUE(ramp.contains(MapJsonKeys::z));
  EXPECT_GT(ramp.value(MapJsonKeys::radius).toDouble(), 0.0);
  EXPECT_EQ(ramp.keys().size(), 3);

  const QVector<QPoint> expanded =
      HillProjection::entrance_cells_from_json(model, entrances);
  EXPECT_FALSE(expanded.isEmpty());
  EXPECT_TRUE(expanded.contains(QPoint(10, 15)));
}

TEST(HillProjectionModelTest, EntranceCountIsCappedAtThree) {
  const HillProjection::Model model = square_model(40);
  const QVector<QPoint> body = filled_rect_cells(10, 10, 20, 20);
  const QVector<QPoint> painted{
      QPoint(10, 12), QPoint(20, 12), QPoint(15, 10), QPoint(15, 20)};

  const QVector<QPoint> normalized =
      HillProjection::normalize_entrance_cells(model, body, painted);
  EXPECT_EQ(normalized.size(), HillProjection::k_max_entrances);

  const QStringList issues = HillProjection::entrance_issues(model, body, painted);
  ASSERT_EQ(issues.size(), 1);
  EXPECT_TRUE(issues.first().contains(QStringLiteral("only the 3 largest")));
}

TEST(HillProjectionModelTest, MissingEntranceIsGeneratedOnTheRim) {
  const HillProjection::Model model = square_model(40);
  const QVector<QPoint> body = filled_rect_cells(10, 10, 20, 20);

  const QVector<QPoint> normalized =
      HillProjection::normalize_entrance_cells(model, body, {});
  ASSERT_EQ(normalized.size(), HillProjection::k_min_entrances);
  EXPECT_TRUE(contains_cell(HillProjection::rim_cells(body), normalized.first()));

  const QStringList issues = HillProjection::entrance_issues(model, body, {});
  ASSERT_EQ(issues.size(), 1);
  EXPECT_TRUE(issues.first().contains(QStringLiteral("No entrance")));
}

TEST(HillProjectionModelTest, ApplyKeepsEntrancesOnTheRimAndWithinTheLimit) {
  const QJsonObject hill{
      {MapJsonKeys::type, "hill"},
      {MapJsonKeys::x, 40.0},
      {MapJsonKeys::z, 40.0},
      {MapJsonKeys::width, 20.0},
      {MapJsonKeys::depth, 20.0},
      {MapJsonKeys::entrances,
       QJsonArray{QJsonObject{{MapJsonKeys::x, 40.0}, {MapJsonKeys::z, 40.0}}}}};

  const HillProjection::Model model = HillProjection::build_model(hill);
  const QJsonObject updated = HillProjection::apply_projection_to_hill_json(
      hill, model, model.hill_cells, model.entrance_cells);

  ASSERT_TRUE(updated.value(MapJsonKeys::entrances).isArray());
  const QJsonArray entrances = updated.value(MapJsonKeys::entrances).toArray();
  ASSERT_EQ(entrances.size(), 1);
  const QJsonObject entrance = entrances.first().toObject();
  const double dx = entrance.value(MapJsonKeys::x).toDouble() - 40.0;
  const double dz = entrance.value(MapJsonKeys::z).toDouble() - 40.0;
  EXPECT_GT(std::sqrt(dx * dx + dz * dz), 8.0);
}

TEST(HillProjectionModelTest, ProjectionApplyWritesFullExtentsAndCenter) {
  const QJsonObject hill{{MapJsonKeys::type, "hill"},
                         {MapJsonKeys::x, 40.0},
                         {MapJsonKeys::z, 40.0},
                         {MapJsonKeys::width, 10.0},
                         {MapJsonKeys::depth, 10.0}};

  const HillProjection::Model model = HillProjection::build_model(hill);
  const int half_span = (model.grid_width - 1) / 2;
  const QVector<QPoint> hill_cells{QPoint(half_span, half_span),
                                   QPoint(half_span + 1, half_span),
                                   QPoint(half_span + 2, half_span),
                                   QPoint(half_span, half_span + 1),
                                   QPoint(half_span + 2, half_span + 1)};
  const QVector<QPoint> entrance_cells{QPoint(half_span, half_span),
                                       QPoint(half_span + 2, half_span + 1)};

  const QJsonObject updated = HillProjection::apply_projection_to_hill_json(
      hill, model, hill_cells, entrance_cells);
  EXPECT_DOUBLE_EQ(updated.value(MapJsonKeys::width).toDouble(), 2.0);
  EXPECT_DOUBLE_EQ(updated.value(MapJsonKeys::depth).toDouble(), 1.0);
  EXPECT_DOUBLE_EQ(updated.value(MapJsonKeys::x).toDouble(),
                   model.origin_x + half_span + 1.0);
  EXPECT_DOUBLE_EQ(updated.value(MapJsonKeys::z).toDouble(),
                   model.origin_z + half_span + 0.5);
  ASSERT_TRUE(updated.value(MapJsonKeys::entrances).isArray());
  const QJsonArray entrances = updated.value(MapJsonKeys::entrances).toArray();
  ASSERT_EQ(entrances.size(), 2);
  EXPECT_TRUE(contains_point(
      entrances, model.origin_x + half_span, model.origin_z + half_span));
  EXPECT_TRUE(contains_point(
      entrances, model.origin_x + half_span + 2.0, model.origin_z + half_span + 1.0));
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

TEST(HillProjectionModelTest, MountainProjectionMatchesRuntimeEllipse) {
  const QJsonObject mountain{
      {MapJsonKeys::type, "mountain"},
      {MapJsonKeys::x, 40.0},
      {MapJsonKeys::z, 40.0},
      {MapJsonKeys::radius, 6.0},
      {MapJsonKeys::rotation, 45.0},
      {MapJsonKeys::entrances,
       QJsonArray{QJsonObject{{MapJsonKeys::x, 42.0}, {MapJsonKeys::z, 42.0}}}}};

  const HillProjection::Model model = HillProjection::build_model(mountain);
  const auto runtime = Game::Map::mountain_footprint_cells(
      {.width = 0.0F, .depth = 0.0F, .radius = 6.0F, .tile_size = 1.0F});
  EXPECT_DOUBLE_EQ(model.hill_half_width, runtime.half_width);
  EXPECT_DOUBLE_EQ(model.hill_half_depth, runtime.half_depth);

  const QJsonObject updated = HillProjection::apply_projection_to_hill_json(
      mountain, model, model.hill_cells, {});
  EXPECT_DOUBLE_EQ(updated.value(MapJsonKeys::x).toDouble(), 40.0);
  EXPECT_DOUBLE_EQ(updated.value(MapJsonKeys::z).toDouble(), 40.0);
  EXPECT_NEAR(updated.value(MapJsonKeys::radius).toDouble(), 6.0, 1e-3);
  EXPECT_FALSE(updated.contains(MapJsonKeys::width));
  EXPECT_FALSE(updated.contains(MapJsonKeys::depth));
  EXPECT_FALSE(updated.contains(MapJsonKeys::entrances));
}
