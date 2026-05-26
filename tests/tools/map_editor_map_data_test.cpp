#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QVector2D>

#include <cmath>
#include <gtest/gtest.h>

#include "tools/map_editor/map_data.h"
#include "tools/map_editor/map_json_keys.h"

namespace {

namespace MapJsonKeys = MapEditor::MapJsonKeys;

constexpr auto legacy_coord_system_key = "coordSystem";
constexpr auto legacy_max_troops_key = "maxTroopsPerPlayer";

auto write_json(const QString& path, const QJsonObject& json) -> void {
  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  file.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
}

auto read_json(const QString& path) -> QJsonObject {
  QFile file(path);
  EXPECT_TRUE(file.open(QIODevice::ReadOnly));
  auto document = QJsonDocument::fromJson(file.readAll());
  EXPECT_TRUE(document.isObject());
  return document.object();
}

auto repo_root() -> QString {
  QDir dir = QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir();
  EXPECT_TRUE(dir.cdUp());
  EXPECT_TRUE(dir.cdUp());
  return dir.absolutePath();
}

} // namespace

TEST(MapEditorMapDataTest, LoadSaveKeepsSnakeCaseSchemaAndPreservesExtraRootFields) {
  QTemporaryDir const temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  const QString input_path = temp_dir.filePath("input.json");
  const QString output_path = temp_dir.filePath("output.json");

  QJsonObject const input{
      {"name", "Schema Compatibility"},
      {MapJsonKeys::description, "Round-trips real map fields."},
      {MapJsonKeys::coord_system, "world"},
      {MapJsonKeys::max_troops_per_player, 1400},
      {MapJsonKeys::time_of_day, "afternoon"},
      {MapJsonKeys::world_props,
       QJsonArray{QJsonObject{{"type", "dead_tree"}, {"x", 10}, {"z", 12}}}},
      {MapJsonKeys::grid,
       QJsonObject{{MapJsonKeys::width, 64},
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
  QTemporaryDir const temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  const QString input_path = temp_dir.filePath("input.json");
  const QString output_path = temp_dir.filePath("output.json");

  QJsonObject const input{{"name", "Legacy Keys"},
                          {legacy_coord_system_key, "grid"},
                          {legacy_max_troops_key, 900},
                          {MapJsonKeys::grid,
                           QJsonObject{{MapJsonKeys::width, 32},
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
  QTemporaryDir const temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  const QString input_path = temp_dir.filePath("input.json");
  const QString output_path = temp_dir.filePath("output.json");

  QJsonObject const input{{"name", "Legacy Firecamps"},
                          {MapJsonKeys::grid,
                           QJsonObject{{MapJsonKeys::width, 32},
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

TEST(MapEditorMapDataTest, RoadWaypointsDefineEditableEndpointsAndStaySyncedOnSave) {
  QTemporaryDir const temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  const QString input_path = temp_dir.filePath("input.json");
  const QString output_path = temp_dir.filePath("output.json");

  QJsonObject const input{
      {"name", "Waypoint Road"},
      {MapJsonKeys::coord_system, "grid"},
      {MapJsonKeys::max_troops_per_player, 1000},
      {MapJsonKeys::grid,
       QJsonObject{{MapJsonKeys::width, 100},
                   {MapJsonKeys::height, 100},
                   {MapJsonKeys::tile_size, 1.0}}},
      {MapJsonKeys::roads,
       QJsonArray{QJsonObject{
           {MapJsonKeys::start, QJsonArray{0, 0}},
           {MapJsonKeys::end, QJsonArray{0, 0}},
           {MapJsonKeys::width, 3.0},
           {MapJsonKeys::style, "default"},
           {MapJsonKeys::waypoints,
            QJsonArray{QJsonArray{8, 10}, QJsonArray{20, 25}, QJsonArray{30, 35}}}}}}};
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

  const QJsonArray waypoints = saved_road.value(MapJsonKeys::waypoints).toArray();
  ASSERT_EQ(waypoints.size(), 3);
  const QJsonArray first_waypoint = waypoints.first().toArray();
  const QJsonArray last_waypoint = waypoints.last().toArray();
  EXPECT_DOUBLE_EQ(first_waypoint[0].toDouble(), 12.0);
  EXPECT_DOUBLE_EQ(first_waypoint[1].toDouble(), 14.0);
  EXPECT_DOUBLE_EQ(last_waypoint[0].toDouble(), 40.0);
  EXPECT_DOUBLE_EQ(last_waypoint[1].toDouble(), 45.0);
}

TEST(MapEditorMapDataTest, FirecampPersistentFalseRoundTrips) {
  QTemporaryDir const temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  const QString input_path = temp_dir.filePath("input.json");
  const QString output_path = temp_dir.filePath("output.json");

  QJsonObject const input{{"name", "Persistent Test"},
                          {MapJsonKeys::grid,
                           QJsonObject{{MapJsonKeys::width, 32},
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
  QTemporaryDir const temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  const QString input_path = temp_dir.filePath("input.json");
  const QString output_path = temp_dir.filePath("output.json");

  QJsonObject const input{{"name", "Scale Test"},
                          {MapJsonKeys::grid,
                           QJsonObject{{MapJsonKeys::width, 32},
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

TEST(MapEditorMapDataTest,
     TroopSpawnsRoundTripEditableFieldsOrderAndUnsupportedEntries) {
  QTemporaryDir const temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  const QString input_path = temp_dir.filePath("input.json");
  const QString output_path = temp_dir.filePath("output.json");

  QJsonObject const input{
      {"name", "Troop Spawns"},
      {MapJsonKeys::grid,
       QJsonObject{{MapJsonKeys::width, 64},
                   {MapJsonKeys::height, 48},
                   {MapJsonKeys::tile_size, 1.0}}},
      {MapJsonKeys::spawns,
       QJsonArray{QJsonObject{{MapJsonKeys::type, "barracks"},
                              {MapJsonKeys::x, 4},
                              {MapJsonKeys::z, 6},
                              {MapJsonKeys::player_id, 1},
                              {MapJsonKeys::max_population, 120}},
                  QJsonObject{{MapJsonKeys::type, "spearman"},
                              {MapJsonKeys::x, 10},
                              {MapJsonKeys::z, 12},
                              {MapJsonKeys::player_id, 2},
                              {"hidden", true},
                              {"description", "Front line"}},
                  QJsonObject{{MapJsonKeys::type, "archer"},
                              {MapJsonKeys::x, 14},
                              {MapJsonKeys::z, 16},
                              {MapJsonKeys::player_id, 0},
                              {MapJsonKeys::max_population, 80},
                              {MapJsonKeys::nation, "roman_republic"}},
                  QJsonObject{{MapJsonKeys::type, "defense_tower"},
                              {MapJsonKeys::x, 30},
                              {MapJsonKeys::z, 32},
                              {"team_id", 3}}}}};
  write_json(input_path, input);

  MapEditor::MapData data;
  ASSERT_TRUE(data.load_from_json(input_path));
  ASSERT_EQ(data.structures().size(), 1);
  ASSERT_EQ(data.troop_spawns().size(), 2);

  MapEditor::TroopSpawnElement spearman = data.troop_spawns().first();
  EXPECT_EQ(spearman.type, "spearman");
  EXPECT_TRUE(spearman.extra_fields.value("hidden").toBool());
  EXPECT_EQ(spearman.extra_fields.value("description").toString(), "Front line");
  spearman.x = 20.0F;
  spearman.z = 22.0F;
  data.update_troop_spawn(0, spearman);

  MapEditor::TroopSpawnElement builder;
  builder.type = "builder";
  builder.x = 40.0F;
  builder.z = 42.0F;
  builder.player_id = 4;
  builder.extra_fields["hidden"] = false;
  data.add_troop_spawn(builder);

  ASSERT_TRUE(data.save_to_json(output_path));

  const QJsonArray spawns = read_json(output_path).value(MapJsonKeys::spawns).toArray();
  ASSERT_EQ(spawns.size(), 5);

  const QJsonObject saved_structure = spawns[0].toObject();
  EXPECT_EQ(saved_structure.value(MapJsonKeys::type).toString(), "barracks");

  const QJsonObject saved_spearman = spawns[1].toObject();
  EXPECT_EQ(saved_spearman.value(MapJsonKeys::type).toString(), "spearman");
  EXPECT_DOUBLE_EQ(saved_spearman.value(MapJsonKeys::x).toDouble(), 20.0);
  EXPECT_DOUBLE_EQ(saved_spearman.value(MapJsonKeys::z).toDouble(), 22.0);
  EXPECT_TRUE(saved_spearman.value("hidden").toBool());
  EXPECT_EQ(saved_spearman.value("description").toString(), "Front line");

  const QJsonObject saved_archer = spawns[2].toObject();
  EXPECT_EQ(saved_archer.value(MapJsonKeys::type).toString(), "archer");
  EXPECT_TRUE(saved_archer.contains(MapJsonKeys::player_id));
  EXPECT_EQ(saved_archer.value(MapJsonKeys::player_id).toInt(), 0);
  EXPECT_EQ(saved_archer.value(MapJsonKeys::nation).toString(), "roman_republic");

  const QJsonObject saved_tower = spawns[3].toObject();
  EXPECT_EQ(saved_tower.value(MapJsonKeys::type).toString(), "defense_tower");
  EXPECT_EQ(saved_tower.value("team_id").toInt(), 3);

  const QJsonObject saved_builder = spawns[4].toObject();
  EXPECT_EQ(saved_builder.value(MapJsonKeys::type).toString(), "builder");
  EXPECT_EQ(saved_builder.value(MapJsonKeys::player_id).toInt(), 4);
  EXPECT_FALSE(saved_builder.value("hidden").toBool(true));
}

TEST(MapEditorMapDataTest, RealMapRoundTripsSpawnTypeSequenceWithoutDuplicates) {
  const QString input_path =
      QDir(repo_root()).filePath("assets/maps/map_crossing_rhone.json");
  QFile input_file(input_path);
  ASSERT_TRUE(input_file.open(QIODevice::ReadOnly));
  const QJsonObject original_root =
      QJsonDocument::fromJson(input_file.readAll()).object();
  const QJsonArray original_spawns = original_root.value(MapJsonKeys::spawns).toArray();
  ASSERT_FALSE(original_spawns.isEmpty());

  QTemporaryDir const temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString output_path = temp_dir.filePath("roundtrip.json");

  MapEditor::MapData data;
  ASSERT_TRUE(data.load_from_json(input_path));
  ASSERT_TRUE(data.save_to_json(output_path));

  const QJsonArray saved_spawns =
      read_json(output_path).value(MapJsonKeys::spawns).toArray();
  ASSERT_EQ(saved_spawns.size(), original_spawns.size());
  for (qsizetype i = 0; i < original_spawns.size(); ++i) {
    EXPECT_EQ(saved_spawns[i].toObject().value(MapJsonKeys::type).toString(),
              original_spawns[i].toObject().value(MapJsonKeys::type).toString())
        << "spawn index " << i;
  }
}

TEST(MapEditorMapDataTest, PreservesUndeadZonesRootFieldOnRoundTrip) {
  QTemporaryDir const temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  const QString input_path = temp_dir.filePath("input.json");
  const QString output_path = temp_dir.filePath("output.json");

  QJsonObject const input{
      {"name", "Undead Root"},
      {MapJsonKeys::grid,
       QJsonObject{{MapJsonKeys::width, 32},
                   {MapJsonKeys::height, 32},
                   {MapJsonKeys::tile_size, 1.0}}},
      {"undead_zones",
       QJsonArray{
           QJsonObject{{"id", "sepulcher_ruin"},
                       {"anchor_type", "ruins"},
                       {"x", 16},
                       {"z", 16},
                       {"waves",
                        QJsonArray{QJsonObject{
                            {"trigger", "initial"},
                            {"units", QJsonObject{{"skeleton_swordsman", 2}}}}}}}}}};
  write_json(input_path, input);

  MapEditor::MapData data;
  ASSERT_TRUE(data.load_from_json(input_path));
  ASSERT_TRUE(data.save_to_json(output_path));

  const QJsonObject output = read_json(output_path);
  ASSERT_TRUE(output.value("undead_zones").isArray());
  const QJsonObject saved_zone =
      output.value("undead_zones").toArray().first().toObject();
  EXPECT_EQ(saved_zone.value("id").toString(), "sepulcher_ruin");
  EXPECT_EQ(saved_zone.value("anchor_type").toString(), "ruins");
}

TEST(MapEditorMapDataTest, SaveReportsWriteErrors) {
  QTemporaryDir const temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  MapEditor::MapData const data;
  QString error_message;

  const QString missing_dir_path = temp_dir.filePath("missing/subdir/output.json");
  EXPECT_FALSE(data.save_to_json(missing_dir_path, &error_message));
  EXPECT_FALSE(error_message.trimmed().isEmpty());
}

TEST(MapEditorMapDataTest, LoadAndSaveRoundFloatingPointValuesToTwoDecimals) {
  QTemporaryDir const temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  const QString input_path = temp_dir.filePath("input.json");
  const QString output_path = temp_dir.filePath("output.json");

  QJsonObject const input{
      {"name", "Precision Clamp"},
      {MapJsonKeys::grid,
       QJsonObject{{MapJsonKeys::width, 64},
                   {MapJsonKeys::height, 64},
                   {MapJsonKeys::tile_size, 1.23456}}},
      {MapJsonKeys::terrain,
       QJsonArray{QJsonObject{
           {MapJsonKeys::type, "hill"},
           {MapJsonKeys::x, 10.1299},
           {MapJsonKeys::z, 20.1251},
           {MapJsonKeys::height, 3.9999},
           {MapJsonKeys::entrances,
            QJsonArray{QJsonObject{{"x", 11.5555}, {"z", 20.4444}}}},
       }}},
      {MapJsonKeys::spawns,
       QJsonArray{QJsonObject{{MapJsonKeys::type, "defense_tower"},
                              {MapJsonKeys::x, 30.5678},
                              {MapJsonKeys::z, 40.1234},
                              {"strength", 77.7777}}}},
      {"custom_meta",
       QJsonObject{{"wind", 4.3219}, {"gust", QJsonArray{1.1111, 2.9999}}}}};
  write_json(input_path, input);

  MapEditor::MapData data;
  ASSERT_TRUE(data.load_from_json(input_path));
  ASSERT_EQ(data.terrain_elements().size(), 1);
  EXPECT_FLOAT_EQ(data.terrain_elements().first().x, 10.13F);
  EXPECT_FLOAT_EQ(data.terrain_elements().first().z, 20.13F);
  EXPECT_FLOAT_EQ(data.terrain_elements().first().height, 4.0F);

  ASSERT_TRUE(data.save_to_json(output_path));
  const QJsonObject output = read_json(output_path);
  const QJsonObject terrain_entry =
      output.value(MapJsonKeys::terrain).toArray().first().toObject();
  EXPECT_DOUBLE_EQ(terrain_entry.value(MapJsonKeys::x).toDouble(), 10.13);
  EXPECT_DOUBLE_EQ(terrain_entry.value(MapJsonKeys::z).toDouble(), 20.13);
  EXPECT_DOUBLE_EQ(terrain_entry.value(MapJsonKeys::height).toDouble(), 4.0);

  const QJsonObject entrance =
      terrain_entry.value(MapJsonKeys::entrances).toArray().first().toObject();
  EXPECT_DOUBLE_EQ(entrance.value("x").toDouble(), 11.56);
  EXPECT_DOUBLE_EQ(entrance.value("z").toDouble(), 20.44);

  const QJsonObject custom_meta = output.value("custom_meta").toObject();
  EXPECT_DOUBLE_EQ(custom_meta.value("wind").toDouble(), 4.32);
  const QJsonArray gust = custom_meta.value("gust").toArray();
  ASSERT_EQ(gust.size(), 2);
  EXPECT_DOUBLE_EQ(gust[0].toDouble(), 1.11);
  EXPECT_DOUBLE_EQ(gust[1].toDouble(), 3.0);

  const QJsonObject raw_spawn =
      output.value(MapJsonKeys::spawns).toArray().first().toObject();
  EXPECT_DOUBLE_EQ(raw_spawn.value(MapJsonKeys::x).toDouble(), 30.57);
  EXPECT_DOUBLE_EQ(raw_spawn.value(MapJsonKeys::z).toDouble(), 40.12);
  EXPECT_DOUBLE_EQ(raw_spawn.value("strength").toDouble(), 77.78);
}

// ---------------------------------------------------------------------------
// compute_min_bridge_width tests
// ---------------------------------------------------------------------------

namespace {

auto make_river(float x1, float y1, float x2, float y2, float width)
    -> MapEditor::LinearElement {
  MapEditor::LinearElement elem;
  elem.type = "river";
  elem.start = QVector2D(x1, y1);
  elem.end = QVector2D(x2, y2);
  elem.width = width;
  return elem;
}

} // namespace

TEST(ComputeMinBridgeWidthTest, NoRiversReturnsAbsoluteMinimum) {
  QVector<MapEditor::LinearElement> elements;
  const float result = MapEditor::compute_min_bridge_width(
      QVector2D(0.0F, 0.0F), QVector2D(10.0F, 0.0F), elements);
  EXPECT_FLOAT_EQ(result, 1.0F);
}

TEST(ComputeMinBridgeWidthTest, NonRiverElementsIgnored) {
  QVector<MapEditor::LinearElement> elements;
  MapEditor::LinearElement road;
  road.type = "road";
  road.start = QVector2D(5.0F, -5.0F);
  road.end = QVector2D(5.0F, 5.0F);
  road.width = 6.0F;
  elements.append(road);

  const float result = MapEditor::compute_min_bridge_width(
      QVector2D(0.0F, 0.0F), QVector2D(10.0F, 0.0F), elements);
  EXPECT_FLOAT_EQ(result, 1.0F);
}

TEST(ComputeMinBridgeWidthTest, PerpendicularCrossingRequiresRiverWidth) {
  // Bridge runs along X axis; river runs along Y axis — crossing at 90 degrees.
  // Required width == river_width / sin(90°) == river_width.
  QVector<MapEditor::LinearElement> elements;
  elements.append(make_river(5.0F, -5.0F, 5.0F, 5.0F, 4.0F));

  const float result = MapEditor::compute_min_bridge_width(
      QVector2D(0.0F, 0.0F), QVector2D(10.0F, 0.0F), elements);
  EXPECT_NEAR(static_cast<double>(result), 4.0, 1e-4);
}

TEST(ComputeMinBridgeWidthTest, DiagonalCrossingIncreasesRequirement) {
  // Bridge runs along X axis; river runs diagonally at 45 degrees.
  // sin(45°) = sqrt(2)/2, so required width = river_width / sin(45°)
  //          = 4.0 * sqrt(2) ≈ 5.657.
  QVector<MapEditor::LinearElement> elements;
  elements.append(make_river(0.0F, 0.0F, 10.0F, 10.0F, 4.0F));

  const float result = MapEditor::compute_min_bridge_width(
      QVector2D(0.0F, 5.0F), QVector2D(10.0F, 5.0F), elements);
  const double expected = 4.0 * std::sqrt(2.0);
  EXPECT_NEAR(static_cast<double>(result), expected, 1e-3);
}

TEST(ComputeMinBridgeWidthTest, NonIntersectingRiverIgnored) {
  // River is beside the bridge and does not cross it.
  QVector<MapEditor::LinearElement> elements;
  elements.append(make_river(20.0F, -5.0F, 20.0F, 5.0F, 4.0F));

  const float result = MapEditor::compute_min_bridge_width(
      QVector2D(0.0F, 0.0F), QVector2D(10.0F, 0.0F), elements);
  EXPECT_FLOAT_EQ(result, 1.0F);
}

TEST(ComputeMinBridgeWidthTest, MultipleRiversUsesWidestRequirement) {
  // Two rivers cross the bridge; the wider river should dominate.
  QVector<MapEditor::LinearElement> elements;
  elements.append(make_river(3.0F, -5.0F, 3.0F, 5.0F, 2.0F));
  elements.append(make_river(7.0F, -5.0F, 7.0F, 5.0F, 6.0F));

  const float result = MapEditor::compute_min_bridge_width(
      QVector2D(0.0F, 0.0F), QVector2D(10.0F, 0.0F), elements);
  EXPECT_NEAR(static_cast<double>(result), 6.0, 1e-4);
}

TEST(ComputeMinBridgeWidthTest, ParallelRiverIgnored) {
  // Bridge and river are parallel (both along X) — no meaningful crossing.
  QVector<MapEditor::LinearElement> elements;
  elements.append(make_river(0.0F, 1.0F, 10.0F, 1.0F, 4.0F));

  const float result = MapEditor::compute_min_bridge_width(
      QVector2D(0.0F, 0.0F), QVector2D(10.0F, 0.0F), elements);
  EXPECT_FLOAT_EQ(result, 1.0F);
}

TEST(ComputeMinBridgeWidthTest, ZeroLengthBridgeReturnsAbsoluteMinimum) {
  QVector<MapEditor::LinearElement> elements;
  elements.append(make_river(0.0F, -5.0F, 0.0F, 5.0F, 4.0F));

  const float result = MapEditor::compute_min_bridge_width(
      QVector2D(0.0F, 0.0F), QVector2D(0.0F, 0.0F), elements);
  EXPECT_FLOAT_EQ(result, 1.0F);
}
