#include "tools/map_editor/map_data.h"
#include "tools/map_editor/map_json_keys.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <gtest/gtest.h>

namespace {

namespace MapJsonKeys = MapEditor::MapJsonKeys;

constexpr auto legacy_coord_system_key = "coordSystem";
constexpr auto legacy_max_troops_key = "maxTroopsPerPlayer";

auto write_json(const QString &path, const QJsonObject &json) -> void {
  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  file.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
}

auto read_json(const QString &path) -> QJsonObject {
  QFile file(path);
  EXPECT_TRUE(file.open(QIODevice::ReadOnly));
  auto document = QJsonDocument::fromJson(file.readAll());
  EXPECT_TRUE(document.isObject());
  return document.object();
}

} // namespace

TEST(MapEditorMapDataTest,
     LoadSaveKeepsSnakeCaseSchemaAndPreservesExtraRootFields) {
  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  const QString input_path = temp_dir.filePath("input.json");
  const QString output_path = temp_dir.filePath("output.json");

  QJsonObject input{
      {"name", "Schema Compatibility"},
      {MapJsonKeys::description, "Round-trips real map fields."},
      {MapJsonKeys::coord_system, "world"},
      {MapJsonKeys::max_troops_per_player, 1400},
      {MapJsonKeys::time_of_day, "afternoon"},
      {MapJsonKeys::world_props,
       QJsonArray{QJsonObject{{"type", "dead_tree"}, {"x", 10}, {"z", 12}}}},
      {MapJsonKeys::grid, QJsonObject{{MapJsonKeys::width, 64},
                                      {MapJsonKeys::height, 48},
                                      {MapJsonKeys::tile_size, 1.0}}},
      {MapJsonKeys::terrain,
       QJsonArray{QJsonObject{{MapJsonKeys::type, "hill"},
                              {MapJsonKeys::x, 8},
                              {MapJsonKeys::z, 9},
                              {MapJsonKeys::radius, 6},
                              {MapJsonKeys::height, 2.5}}}}};
  write_json(input_path, input);

  MapEditor::MapData data;
  ASSERT_TRUE(data.load_from_json(input_path));
  ASSERT_EQ(data.world_props().size(), 1);
  EXPECT_EQ(data.world_props().first().type, "dead_tree");
  ASSERT_TRUE(data.save_to_json(output_path));

  const QJsonObject output = read_json(output_path);
  EXPECT_EQ(output.value(MapJsonKeys::coord_system).toString(), "world");
  EXPECT_EQ(output.value(MapJsonKeys::max_troops_per_player).toInt(), 1400);
  EXPECT_FALSE(output.contains(legacy_coord_system_key));
  EXPECT_FALSE(output.contains(legacy_max_troops_key));
  EXPECT_EQ(output.value(MapJsonKeys::time_of_day).toString(), "afternoon");
  ASSERT_TRUE(output.value(MapJsonKeys::world_props).isArray());
  EXPECT_EQ(output.value(MapJsonKeys::world_props).toArray().size(), 1);
}

TEST(MapEditorMapDataTest, LegacyCamelCaseRootsSaveBackAsSnakeCase) {
  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  const QString input_path = temp_dir.filePath("input.json");
  const QString output_path = temp_dir.filePath("output.json");

  QJsonObject input{
      {"name", "Legacy Keys"},
      {legacy_coord_system_key, "grid"},
      {legacy_max_troops_key, 900},
      {MapJsonKeys::grid, QJsonObject{{MapJsonKeys::width, 32},
                                      {MapJsonKeys::height, 24},
                                      {MapJsonKeys::tile_size, 1.0}}}};
  write_json(input_path, input);

  MapEditor::MapData data;
  ASSERT_TRUE(data.load_from_json(input_path));
  ASSERT_TRUE(data.save_to_json(output_path));

  const QJsonObject output = read_json(output_path);
  EXPECT_EQ(output.value(MapJsonKeys::coord_system).toString(), "grid");
  EXPECT_EQ(output.value(MapJsonKeys::max_troops_per_player).toInt(), 900);
  EXPECT_FALSE(output.contains(legacy_coord_system_key));
  EXPECT_FALSE(output.contains(legacy_max_troops_key));
}

TEST(MapEditorMapDataTest, LegacyFirecampsImportAsWorldPropsOnSave) {
  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  const QString input_path = temp_dir.filePath("input.json");
  const QString output_path = temp_dir.filePath("output.json");

  QJsonObject input{
      {"name", "Legacy Firecamps"},
      {MapJsonKeys::grid, QJsonObject{{MapJsonKeys::width, 32},
                                      {MapJsonKeys::height, 24},
                                      {MapJsonKeys::tile_size, 1.0}}},
      {MapJsonKeys::firecamps,
       QJsonArray{QJsonObject{{MapJsonKeys::x, 10},
                              {MapJsonKeys::z, 12},
                              {MapJsonKeys::intensity, 1.5},
                              {MapJsonKeys::radius, 4.0}}}}};
  write_json(input_path, input);

  MapEditor::MapData data;
  ASSERT_TRUE(data.load_from_json(input_path));
  ASSERT_EQ(data.world_props().size(), 1);
  EXPECT_EQ(data.world_props().first().type, "firecamp");
  EXPECT_FLOAT_EQ(data.world_props().first().radius, 4.0F);
  ASSERT_TRUE(data.save_to_json(output_path));

  const QJsonObject output = read_json(output_path);
  EXPECT_FALSE(output.contains(MapJsonKeys::firecamps));
  ASSERT_TRUE(output.value(MapJsonKeys::world_props).isArray());
  const QJsonObject saved_prop =
      output.value(MapJsonKeys::world_props).toArray().first().toObject();
  EXPECT_EQ(saved_prop.value(MapJsonKeys::type).toString(), "firecamp");
  EXPECT_DOUBLE_EQ(saved_prop.value(MapJsonKeys::intensity).toDouble(), 1.5);
  EXPECT_DOUBLE_EQ(saved_prop.value(MapJsonKeys::radius).toDouble(), 4.0);
}

TEST(MapEditorMapDataTest,
     RoadWaypointsDefineEditableEndpointsAndStaySyncedOnSave) {
  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  const QString input_path = temp_dir.filePath("input.json");
  const QString output_path = temp_dir.filePath("output.json");

  QJsonObject input{
      {"name", "Waypoint Road"},
      {MapJsonKeys::coord_system, "grid"},
      {MapJsonKeys::max_troops_per_player, 1000},
      {MapJsonKeys::grid, QJsonObject{{MapJsonKeys::width, 100},
                                      {MapJsonKeys::height, 100},
                                      {MapJsonKeys::tile_size, 1.0}}},
      {MapJsonKeys::roads,
       QJsonArray{QJsonObject{{MapJsonKeys::start, QJsonArray{0, 0}},
                              {MapJsonKeys::end, QJsonArray{0, 0}},
                              {MapJsonKeys::width, 3.0},
                              {MapJsonKeys::style, "default"},
                              {MapJsonKeys::waypoints,
                               QJsonArray{QJsonArray{8, 10}, QJsonArray{20, 25},
                                          QJsonArray{30, 35}}}}}}};
  write_json(input_path, input);

  MapEditor::MapData data;
  ASSERT_TRUE(data.load_from_json(input_path));
  ASSERT_EQ(data.linear_elements().size(), 1);

  MapEditor::LinearElement road = data.linear_elements().first();
  EXPECT_FLOAT_EQ(road.start.x(), 8.0F);
  EXPECT_FLOAT_EQ(road.start.y(), 10.0F);
  EXPECT_FLOAT_EQ(road.end.x(), 30.0F);
  EXPECT_FLOAT_EQ(road.end.y(), 35.0F);

  road.start = QVector2D(12.0F, 14.0F);
  road.end = QVector2D(40.0F, 45.0F);
  data.update_linear_element(0, road);
  ASSERT_TRUE(data.save_to_json(output_path));

  const QJsonObject output = read_json(output_path);
  ASSERT_TRUE(output.value(MapJsonKeys::roads).isArray());
  const QJsonObject saved_road =
      output.value(MapJsonKeys::roads).toArray().first().toObject();

  const QJsonArray saved_start = saved_road.value(MapJsonKeys::start).toArray();
  const QJsonArray saved_end = saved_road.value(MapJsonKeys::end).toArray();
  ASSERT_EQ(saved_start.size(), 2);
  ASSERT_EQ(saved_end.size(), 2);
  EXPECT_DOUBLE_EQ(saved_start[0].toDouble(), 12.0);
  EXPECT_DOUBLE_EQ(saved_start[1].toDouble(), 14.0);
  EXPECT_DOUBLE_EQ(saved_end[0].toDouble(), 40.0);
  EXPECT_DOUBLE_EQ(saved_end[1].toDouble(), 45.0);

  const QJsonArray waypoints =
      saved_road.value(MapJsonKeys::waypoints).toArray();
  ASSERT_EQ(waypoints.size(), 3);
  const QJsonArray first_waypoint = waypoints.first().toArray();
  const QJsonArray last_waypoint = waypoints.last().toArray();
  EXPECT_DOUBLE_EQ(first_waypoint[0].toDouble(), 12.0);
  EXPECT_DOUBLE_EQ(first_waypoint[1].toDouble(), 14.0);
  EXPECT_DOUBLE_EQ(last_waypoint[0].toDouble(), 40.0);
  EXPECT_DOUBLE_EQ(last_waypoint[1].toDouble(), 45.0);
}

TEST(MapEditorMapDataTest, FirecampPersistentFalseRoundTrips) {
  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  const QString input_path = temp_dir.filePath("input.json");
  const QString output_path = temp_dir.filePath("output.json");

  QJsonObject input{
      {"name", "Persistent Test"},
      {MapJsonKeys::grid, QJsonObject{{MapJsonKeys::width, 32},
                                      {MapJsonKeys::height, 24},
                                      {MapJsonKeys::tile_size, 1.0}}},
      {MapJsonKeys::world_props,
       QJsonArray{QJsonObject{{MapJsonKeys::type, "firecamp"},
                              {MapJsonKeys::x, 5},
                              {MapJsonKeys::z, 5},
                              {MapJsonKeys::intensity, 1.0},
                              {MapJsonKeys::radius, 3.0},
                              {MapJsonKeys::persistent, false}}}}};
  write_json(input_path, input);

  MapEditor::MapData data;
  ASSERT_TRUE(data.load_from_json(input_path));
  ASSERT_EQ(data.world_props().size(), 1);
  EXPECT_FALSE(data.world_props().first().persistent);
  ASSERT_TRUE(data.save_to_json(output_path));

  const QJsonObject output = read_json(output_path);
  const QJsonObject saved_prop =
      output.value(MapJsonKeys::world_props).toArray().first().toObject();
  EXPECT_TRUE(saved_prop.contains(MapJsonKeys::persistent));
  EXPECT_FALSE(saved_prop.value(MapJsonKeys::persistent).toBool());
}

TEST(MapEditorMapDataTest, WorldPropsAlwaysWriteScaleRegardlessOfType) {
  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  const QString input_path = temp_dir.filePath("input.json");
  const QString output_path = temp_dir.filePath("output.json");

  QJsonObject input{
      {"name", "Scale Test"},
      {MapJsonKeys::grid, QJsonObject{{MapJsonKeys::width, 32},
                                      {MapJsonKeys::height, 24},
                                      {MapJsonKeys::tile_size, 1.0}}},
      {MapJsonKeys::world_props,
       QJsonArray{QJsonObject{{MapJsonKeys::type, "firecamp"},
                              {MapJsonKeys::x, 5},
                              {MapJsonKeys::z, 5},
                              {MapJsonKeys::scale, 2.0},
                              {MapJsonKeys::intensity, 1.0},
                              {MapJsonKeys::radius, 3.0}},
                  QJsonObject{{MapJsonKeys::type, "tent"},
                              {MapJsonKeys::x, 10},
                              {MapJsonKeys::z, 10},
                              {MapJsonKeys::scale, 1.5}}}}};
  write_json(input_path, input);

  MapEditor::MapData data;
  ASSERT_TRUE(data.load_from_json(input_path));
  ASSERT_EQ(data.world_props().size(), 2);
  ASSERT_TRUE(data.save_to_json(output_path));

  const QJsonObject output = read_json(output_path);
  const QJsonArray props = output.value(MapJsonKeys::world_props).toArray();
  ASSERT_EQ(props.size(), 2);

  const QJsonObject saved_firecamp = props[0].toObject();
  EXPECT_TRUE(saved_firecamp.contains(MapJsonKeys::scale));
  EXPECT_DOUBLE_EQ(saved_firecamp.value(MapJsonKeys::scale).toDouble(), 2.0);

  const QJsonObject saved_tent = props[1].toObject();
  EXPECT_TRUE(saved_tent.contains(MapJsonKeys::scale));
  EXPECT_DOUBLE_EQ(saved_tent.value(MapJsonKeys::scale).toDouble(), 1.5);
}

TEST(MapEditorMapDataTest, SaveReportsWriteErrors) {
  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  MapEditor::MapData data;
  QString error_message;

  const QString missing_dir_path =
      temp_dir.filePath("missing/subdir/output.json");
  EXPECT_FALSE(data.save_to_json(missing_dir_path, &error_message));
  EXPECT_FALSE(error_message.trimmed().isEmpty());
}
