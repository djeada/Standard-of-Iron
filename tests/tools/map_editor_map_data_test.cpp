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
#include <numbers>

#include "game/map/terrain_footprint.h"
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
  EXPECT_FALSE(output.contains(MapJsonKeys::time_of_day));
  ASSERT_TRUE(output.value(MapJsonKeys::environment).isObject());
  EXPECT_DOUBLE_EQ(output.value(MapJsonKeys::environment)
                       .toObject()
                       .value(MapJsonKeys::start_time)
                       .toDouble(),
                   17.0);
  ASSERT_TRUE(output.value(MapJsonKeys::world_props).isArray());
  EXPECT_EQ(output.value(MapJsonKeys::world_props).toArray().size(), 1);
  ASSERT_TRUE(output.value(MapJsonKeys::terrain).isArray());
  const QJsonObject saved_hill =
      output.value(MapJsonKeys::terrain).toArray().first().toObject();
  EXPECT_DOUBLE_EQ(saved_hill.value(MapJsonKeys::radius).toDouble(), 6.0);
  EXPECT_FALSE(saved_hill.contains(MapJsonKeys::width));
  EXPECT_FALSE(saved_hill.contains(MapJsonKeys::depth));
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
     StructuresAndTroopSpawnsRoundTripInSeparateCanonicalCollections) {
  QTemporaryDir const temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  const QString input_path = temp_dir.filePath("input.json");
  const QString output_path = temp_dir.filePath("output.json");

  QJsonObject const input{{"name", "Troop Spawns"},
                          {MapJsonKeys::grid,
                           QJsonObject{{MapJsonKeys::width, 64},
                                       {MapJsonKeys::height, 48},
                                       {MapJsonKeys::tile_size, 1.0}}},
                          {MapJsonKeys::spawns,
                           QJsonArray{
                               QJsonObject{{MapJsonKeys::type, "spearman"},
                                           {MapJsonKeys::x, 10},
                                           {MapJsonKeys::z, 12},
                                           {MapJsonKeys::player_id, 2},
                                           {MapJsonKeys::behavior, "guard"},
                                           {MapJsonKeys::guard_radius, 18.0},
                                           {"hidden", true},
                                           {"description", "Front line"}},
                               QJsonObject{{MapJsonKeys::type, "archer"},
                                           {MapJsonKeys::x, 14},
                                           {MapJsonKeys::z, 16},
                                           {MapJsonKeys::player_id, 0},
                                           {MapJsonKeys::max_population, 80},
                                           {MapJsonKeys::nation, "roman_republic"}},
                           }},
                          {MapJsonKeys::structures,
                           QJsonArray{QJsonObject{{MapJsonKeys::type, "barracks"},
                                                  {MapJsonKeys::x, 4},
                                                  {MapJsonKeys::z, 6},
                                                  {MapJsonKeys::player_id, 1},
                                                  {MapJsonKeys::max_population, 120}},
                                      QJsonObject{{MapJsonKeys::type, "defense_tower"},
                                                  {MapJsonKeys::x, 30},
                                                  {MapJsonKeys::z, 32},
                                                  {"team_id", 3}}}}};
  write_json(input_path, input);

  MapEditor::MapData data;
  ASSERT_TRUE(data.load_from_json(input_path));
  ASSERT_EQ(data.structures().size(), 2);
  ASSERT_EQ(data.troop_spawns().size(), 2);

  MapEditor::TroopSpawnElement spearman = data.troop_spawns().first();
  EXPECT_EQ(spearman.type, "spearman");
  EXPECT_EQ(spearman.behavior, "guard");
  EXPECT_FLOAT_EQ(spearman.guard_radius, 18.0F);
  EXPECT_TRUE(spearman.extra_fields.value("hidden").toBool());
  EXPECT_EQ(spearman.extra_fields.value("description").toString(), "Front line");
  EXPECT_FALSE(spearman.extra_fields.contains(MapJsonKeys::behavior));
  EXPECT_FALSE(spearman.extra_fields.contains(MapJsonKeys::guard_radius));
  spearman.x = 20.0F;
  spearman.z = 22.0F;
  spearman.behavior = "patrol";
  spearman.patrol_waypoints =
      QJsonArray{QJsonObject{{MapJsonKeys::x, 24}, {MapJsonKeys::z, 22}},
                 QJsonObject{{MapJsonKeys::x, 24}, {MapJsonKeys::z, 28}}};
  data.update_troop_spawn(0, spearman);

  MapEditor::TroopSpawnElement builder;
  builder.type = "builder";
  builder.x = 40.0F;
  builder.z = 42.0F;
  builder.player_id = 4;
  builder.behavior = "hold";
  builder.extra_fields["hidden"] = false;
  data.add_troop_spawn(builder);

  ASSERT_TRUE(data.save_to_json(output_path));

  const QJsonArray spawns = read_json(output_path).value(MapJsonKeys::spawns).toArray();
  ASSERT_EQ(spawns.size(), 3);

  const QJsonObject saved_spearman = spawns[0].toObject();
  EXPECT_EQ(saved_spearman.value(MapJsonKeys::type).toString(), "spearman");
  EXPECT_DOUBLE_EQ(saved_spearman.value(MapJsonKeys::x).toDouble(), 20.0);
  EXPECT_DOUBLE_EQ(saved_spearman.value(MapJsonKeys::z).toDouble(), 22.0);
  EXPECT_EQ(saved_spearman.value(MapJsonKeys::behavior).toString(), "patrol");
  const QJsonArray saved_patrol_waypoints =
      saved_spearman.value(MapJsonKeys::patrol_waypoints).toArray();
  ASSERT_EQ(saved_patrol_waypoints.size(), 2);
  EXPECT_DOUBLE_EQ(
      saved_patrol_waypoints[0].toObject().value(MapJsonKeys::x).toDouble(), 24.0);
  EXPECT_DOUBLE_EQ(
      saved_patrol_waypoints[1].toObject().value(MapJsonKeys::z).toDouble(), 28.0);
  EXPECT_TRUE(saved_spearman.value("hidden").toBool());
  EXPECT_EQ(saved_spearman.value("description").toString(), "Front line");

  const QJsonObject saved_archer = spawns[1].toObject();
  EXPECT_EQ(saved_archer.value(MapJsonKeys::type).toString(), "archer");
  EXPECT_TRUE(saved_archer.contains(MapJsonKeys::player_id));
  EXPECT_EQ(saved_archer.value(MapJsonKeys::player_id).toInt(), 0);
  EXPECT_EQ(saved_archer.value(MapJsonKeys::nation).toString(), "roman_republic");

  const QJsonObject saved_builder = spawns[2].toObject();
  EXPECT_EQ(saved_builder.value(MapJsonKeys::type).toString(), "builder");
  EXPECT_EQ(saved_builder.value(MapJsonKeys::player_id).toInt(), 4);
  EXPECT_EQ(saved_builder.value(MapJsonKeys::behavior).toString(), "hold");
  EXPECT_FALSE(saved_builder.value("hidden").toBool(true));

  const QJsonArray structures =
      read_json(output_path).value(MapJsonKeys::structures).toArray();
  ASSERT_EQ(structures.size(), 2);
  EXPECT_EQ(structures[0].toObject().value(MapJsonKeys::type).toString(), "barracks");
  EXPECT_EQ(structures[0].toObject().value(MapJsonKeys::max_population).toInt(), 120);
  const QJsonObject saved_tower = structures[1].toObject();
  EXPECT_EQ(saved_tower.value(MapJsonKeys::type).toString(), "defense_tower");
  EXPECT_EQ(saved_tower.value("team_id").toInt(), 3);
}

TEST(MapEditorMapDataTest, RealMapRoundTripsSpawnTypeSequenceWithoutDuplicates) {
  const QString input_path =
      QDir(repo_root()).filePath("assets/maps/map_crossing_rhone.json");
  QFile input_file(input_path);
  ASSERT_TRUE(input_file.open(QIODevice::ReadOnly));
  const QJsonObject original_root =
      QJsonDocument::fromJson(input_file.readAll()).object();
  const QJsonArray original_spawns = original_root.value(MapJsonKeys::spawns).toArray();
  const QJsonArray original_structures =
      original_root.value(MapJsonKeys::structures).toArray();
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
  const QJsonArray saved_structures =
      read_json(output_path).value(MapJsonKeys::structures).toArray();
  ASSERT_EQ(saved_structures.size(), original_structures.size());
  for (qsizetype i = 0; i < original_structures.size(); ++i) {
    EXPECT_EQ(saved_structures[i].toObject().value(MapJsonKeys::type).toString(),
              original_structures[i].toObject().value(MapJsonKeys::type).toString())
        << "structure index " << i;
  }
}

TEST(MapEditorMapDataTest, RingRiverRoundTripsAsARingAndDrawsAsALoop) {
  QTemporaryDir const temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString input_path = temp_dir.filePath("moat.json");
  const QString output_path = temp_dir.filePath("moat_out.json");

  const QJsonObject moat{
      {"shape", "ring"}, {"x", 30}, {"z", 30}, {"radius", 10.0}, {"width", 6.0}};
  write_json(input_path,
             QJsonObject{{MapJsonKeys::grid,
                          QJsonObject{{MapJsonKeys::width, 64},
                                      {MapJsonKeys::height, 64},
                                      {MapJsonKeys::tile_size, 1.0}}},
                         {MapJsonKeys::rivers, QJsonArray{moat}}});

  MapEditor::MapData data;
  ASSERT_TRUE(data.load_from_json(input_path));
  ASSERT_EQ(data.linear_elements().size(), 1);
  const auto& river = data.linear_elements().first();
  EXPECT_EQ(river.type, QStringLiteral("river"));
  EXPECT_GE(river.waypoints.size(), 13) << "the editor draws the ring as a loop";
  EXPECT_EQ(river.waypoints.first(), river.waypoints.last());

  ASSERT_TRUE(data.save_to_json(output_path));
  const QJsonArray saved = read_json(output_path).value(MapJsonKeys::rivers).toArray();
  ASSERT_EQ(saved.size(), 1);
  const QJsonObject saved_moat = saved[0].toObject();
  EXPECT_EQ(saved_moat.value("shape").toString(), QStringLiteral("ring"));
  EXPECT_DOUBLE_EQ(saved_moat.value("radius").toDouble(), 10.0);
  EXPECT_DOUBLE_EQ(saved_moat.value(MapJsonKeys::width).toDouble(), 6.0);
  EXPECT_FALSE(saved_moat.contains(MapJsonKeys::waypoints))
      << "a ring is saved as a ring, not as the loop it was drawn as";
}

TEST(MapEditorMapDataTest, RejectsRetiredBuildingAndWallCollections) {
  QTemporaryDir const temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QJsonObject grid{{MapJsonKeys::width, 16},
                         {MapJsonKeys::height, 16},
                         {MapJsonKeys::tile_size, 1.0}};

  for (const QString& retired_key :
       {QStringLiteral("buildings"), QStringLiteral("walls")}) {
    const QString input_path = temp_dir.filePath(retired_key + ".json");
    write_json(input_path,
               QJsonObject{{MapJsonKeys::grid, grid}, {retired_key, QJsonArray{}}});
    MapEditor::MapData data;
    QString error;
    EXPECT_FALSE(data.load_from_json(input_path, &error));
    EXPECT_TRUE(error.contains(QStringLiteral("structures")));
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
      {MapJsonKeys::structures,
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

  const QJsonObject raw_building =
      output.value(MapJsonKeys::structures).toArray().first().toObject();
  EXPECT_DOUBLE_EQ(raw_building.value(MapJsonKeys::x).toDouble(), 30.57);
  EXPECT_DOUBLE_EQ(raw_building.value(MapJsonKeys::z).toDouble(), 40.12);
  EXPECT_DOUBLE_EQ(raw_building.value("strength").toDouble(), 77.78);
}

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
  QVector<MapEditor::LinearElement> const elements;
  const float result = MapEditor::compute_min_bridge_width(
      QVector2D(0.0F, 0.0F), QVector2D(10.0F, 0.0F), elements);
  EXPECT_FLOAT_EQ(result, MapEditor::k_min_bridge_width);
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
  EXPECT_FLOAT_EQ(result, MapEditor::k_min_bridge_width);
}

TEST(ComputeMinBridgeWidthTest, PerpendicularCrossingRequiresRiverWidth) {

  QVector<MapEditor::LinearElement> elements;
  elements.append(make_river(5.0F, -5.0F, 5.0F, 5.0F, 12.0F));

  const float result = MapEditor::compute_min_bridge_width(
      QVector2D(0.0F, 0.0F), QVector2D(10.0F, 0.0F), elements);
  EXPECT_NEAR(static_cast<double>(result), 12.0, 1e-4);
}

TEST(ComputeMinBridgeWidthTest, DiagonalCrossingIncreasesRequirement) {

  QVector<MapEditor::LinearElement> elements;
  elements.append(make_river(0.0F, 0.0F, 10.0F, 10.0F, 8.0F));

  const float result = MapEditor::compute_min_bridge_width(
      QVector2D(0.0F, 5.0F), QVector2D(10.0F, 5.0F), elements);
  const double expected = 8.0 * std::numbers::sqrt2;
  EXPECT_NEAR(static_cast<double>(result), expected, 1e-3);
}

TEST(ComputeMinBridgeWidthTest, NonIntersectingRiverIgnored) {

  QVector<MapEditor::LinearElement> elements;
  elements.append(make_river(20.0F, -5.0F, 20.0F, 5.0F, 4.0F));

  const float result = MapEditor::compute_min_bridge_width(
      QVector2D(0.0F, 0.0F), QVector2D(10.0F, 0.0F), elements);
  EXPECT_FLOAT_EQ(result, MapEditor::k_min_bridge_width);
}

TEST(ComputeMinBridgeWidthTest, MultipleRiversUsesWidestRequirement) {

  QVector<MapEditor::LinearElement> elements;
  elements.append(make_river(3.0F, -5.0F, 3.0F, 5.0F, 2.0F));
  elements.append(make_river(7.0F, -5.0F, 7.0F, 5.0F, 12.0F));

  const float result = MapEditor::compute_min_bridge_width(
      QVector2D(0.0F, 0.0F), QVector2D(10.0F, 0.0F), elements);
  EXPECT_NEAR(static_cast<double>(result), 12.0, 1e-4);
}

TEST(ComputeMinBridgeWidthTest, ParallelRiverIgnored) {

  QVector<MapEditor::LinearElement> elements;
  elements.append(make_river(0.0F, 1.0F, 10.0F, 1.0F, 4.0F));

  const float result = MapEditor::compute_min_bridge_width(
      QVector2D(0.0F, 0.0F), QVector2D(10.0F, 0.0F), elements);
  EXPECT_FLOAT_EQ(result, MapEditor::k_min_bridge_width);
}

TEST(ComputeMinBridgeWidthTest, ZeroLengthBridgeReturnsAbsoluteMinimum) {
  QVector<MapEditor::LinearElement> elements;
  elements.append(make_river(0.0F, -5.0F, 0.0F, 5.0F, 4.0F));

  const float result = MapEditor::compute_min_bridge_width(
      QVector2D(0.0F, 0.0F), QVector2D(0.0F, 0.0F), elements);
  EXPECT_FLOAT_EQ(result, MapEditor::k_min_bridge_width);
}

TEST(MapEditorMapDataTest, RoadAndRiverWaypointsSurviveLoadAndSave) {
  QTemporaryDir const temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  const QString input_path = temp_dir.filePath("waypoints_input.json");
  const QString output_path = temp_dir.filePath("waypoints_output.json");

  const QJsonArray road_waypoints{
      QJsonArray{2.0, 2.0}, QJsonArray{12.0, 4.0}, QJsonArray{30.0, 20.0}};
  QJsonObject const input{
      {"name", "Waypoints"},
      {MapJsonKeys::grid,
       QJsonObject{{MapJsonKeys::width, 64},
                   {MapJsonKeys::height, 64},
                   {MapJsonKeys::tile_size, 1.0}}},
      {MapJsonKeys::roads,
       QJsonArray{QJsonObject{{MapJsonKeys::start, QJsonArray{2.0, 2.0}},
                              {MapJsonKeys::end, QJsonArray{30.0, 20.0}},
                              {MapJsonKeys::width, 5.25},
                              {MapJsonKeys::style, "default"},
                              {MapJsonKeys::waypoints, road_waypoints}}}},
      {MapJsonKeys::rivers,
       QJsonArray{QJsonObject{{MapJsonKeys::start, QJsonArray{0.0, 40.0}},
                              {MapJsonKeys::end, QJsonArray{60.0, 44.0}},
                              {MapJsonKeys::width, 6.0},
                              {MapJsonKeys::waypoints,
                               QJsonArray{QJsonArray{0.0, 40.0},
                                          QJsonArray{30.0, 42.0},
                                          QJsonArray{60.0, 44.0}}}}}}};
  write_json(input_path, input);

  MapEditor::MapData data;
  ASSERT_TRUE(data.load_from_json(input_path));

  const MapEditor::LinearElement* road = nullptr;
  const MapEditor::LinearElement* river = nullptr;
  for (const auto& element : data.linear_elements()) {
    if (element.type == QLatin1String("road")) {
      road = &element;
    } else if (element.type == QLatin1String("river")) {
      river = &element;
    }
  }
  ASSERT_NE(road, nullptr);
  ASSERT_NE(river, nullptr);

  ASSERT_EQ(road->waypoints.size(), 3);
  EXPECT_DOUBLE_EQ(road->waypoints[1].x(), 12.0);
  EXPECT_FALSE(road->extra_fields.contains(MapJsonKeys::waypoints));
  EXPECT_EQ(MapEditor::linear_polyline(*road).size(), 3);
  EXPECT_EQ(river->waypoints.size(), 3);

  ASSERT_TRUE(data.save_to_json(output_path));
  const QJsonObject output = read_json(output_path);
  const QJsonObject saved_road =
      output.value(MapJsonKeys::roads).toArray()[0].toObject();
  EXPECT_EQ(saved_road.value(MapJsonKeys::waypoints).toArray(), road_waypoints);
  const QJsonObject saved_river =
      output.value(MapJsonKeys::rivers).toArray()[0].toObject();
  EXPECT_EQ(saved_river.value(MapJsonKeys::waypoints).toArray().size(), 3);
}

TEST(MapEditorMapDataTest, CampaignRoadWaypointsSurviveAnUntouchedRoundTrip) {
  QTemporaryDir const temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  const QString source_path = repo_root() + "/assets/maps/map_campania_campaign.json";
  MapEditor::MapData data;
  QString error;
  ASSERT_TRUE(data.load_from_json(source_path, &error)) << error.toStdString();

  int roads_with_waypoints = 0;
  for (const auto& element : data.linear_elements()) {
    if (element.type == QLatin1String("road") && !element.waypoints.isEmpty()) {
      ++roads_with_waypoints;
    }
  }
  EXPECT_GT(roads_with_waypoints, 0);

  const QString output_path = temp_dir.filePath("campania_roundtrip.json");
  ASSERT_TRUE(data.save_to_json(output_path));

  const QJsonArray original =
      read_json(source_path).value(MapJsonKeys::roads).toArray();
  const QJsonArray saved = read_json(output_path).value(MapJsonKeys::roads).toArray();
  ASSERT_EQ(saved.size(), original.size());

  for (qsizetype road = 0; road < original.size(); ++road) {
    const QJsonArray original_points =
        original[road].toObject().value(MapJsonKeys::waypoints).toArray();
    const QJsonArray saved_points =
        saved[road].toObject().value(MapJsonKeys::waypoints).toArray();
    ASSERT_EQ(saved_points.size(), original_points.size()) << "road " << road;
    for (qsizetype point = 0; point < original_points.size(); ++point) {
      const QJsonArray expected = original_points[point].toArray();
      const QJsonArray actual = saved_points[point].toArray();
      ASSERT_EQ(actual.size(), expected.size());
      for (qsizetype axis = 0; axis < expected.size(); ++axis) {
        const double normalized = std::round(expected[axis].toDouble() * 100.0) / 100.0;
        EXPECT_NEAR(actual[axis].toDouble(), normalized, 1e-9)
            << "road " << road << " point " << point;
      }
    }
  }
}

TEST(MapEditorMapDataTest, RadiusOnlyTerrainKeepsExtentsUnauthored) {
  QTemporaryDir const temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  const QString input_path = temp_dir.filePath("radius_terrain.json");
  const QString output_path = temp_dir.filePath("radius_terrain_out.json");

  QJsonObject const input{{"name", "Radius Terrain"},
                          {MapJsonKeys::grid,
                           QJsonObject{{MapJsonKeys::width, 200},
                                       {MapJsonKeys::height, 200},
                                       {MapJsonKeys::tile_size, 1.0}}},
                          {MapJsonKeys::terrain,
                           QJsonArray{QJsonObject{{MapJsonKeys::type, "mountain"},
                                                  {MapJsonKeys::x, 60},
                                                  {MapJsonKeys::z, 40},
                                                  {MapJsonKeys::radius, 20.53},
                                                  {MapJsonKeys::height, 8}},
                                      QJsonObject{{MapJsonKeys::type, "hill"},
                                                  {MapJsonKeys::x, 120},
                                                  {MapJsonKeys::z, 90},
                                                  {MapJsonKeys::width, 60.0},
                                                  {MapJsonKeys::depth, 60.0},
                                                  {MapJsonKeys::height, 3}}}}};
  write_json(input_path, input);

  MapEditor::MapData data;
  ASSERT_TRUE(data.load_from_json(input_path));
  ASSERT_EQ(data.terrain_elements().size(), 2);

  const MapEditor::TerrainElement& mountain = data.terrain_elements()[0];
  EXPECT_FLOAT_EQ(mountain.radius, 20.53F);
  EXPECT_FLOAT_EQ(mountain.width, 0.0F);
  EXPECT_FLOAT_EQ(mountain.depth, 0.0F);

  const auto mountain_footprint =
      Game::Map::mountain_footprint_cells({.width = mountain.width,
                                           .depth = mountain.depth,
                                           .radius = mountain.radius,
                                           .tile_size = 1.0F});
  EXPECT_GT(mountain_footprint.half_width, mountain_footprint.half_depth * 2.0F);
  EXPECT_FLOAT_EQ(mountain_footprint.half_width,
                  Game::Map::mountain_major_radius_cells(20.53F));

  const MapEditor::TerrainElement& hill = data.terrain_elements()[1];
  EXPECT_FLOAT_EQ(hill.width, 60.0F);
  EXPECT_FLOAT_EQ(hill.depth, 60.0F);

  ASSERT_TRUE(data.save_to_json(output_path));
  const QJsonArray saved = read_json(output_path).value(MapJsonKeys::terrain).toArray();
  ASSERT_EQ(saved.size(), 2);

  const QJsonObject saved_mountain = saved[0].toObject();
  EXPECT_FALSE(saved_mountain.contains(MapJsonKeys::width));
  EXPECT_FALSE(saved_mountain.contains(MapJsonKeys::depth));
  EXPECT_NEAR(saved_mountain.value(MapJsonKeys::radius).toDouble(), 20.53, 1e-6);

  const QJsonObject saved_hill = saved[1].toObject();
  EXPECT_FALSE(saved_hill.contains(MapJsonKeys::radius));
  EXPECT_NEAR(saved_hill.value(MapJsonKeys::width).toDouble(), 60.0, 1e-6);
  EXPECT_NEAR(saved_hill.value(MapJsonKeys::depth).toDouble(), 60.0, 1e-6);
}

TEST(MapEditorMapDataTest, ForestsAndUndeadClearRewardsSurviveARoundTrip) {
  QTemporaryDir const temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  const QString input_path = temp_dir.filePath("input.json");
  const QString output_path = temp_dir.filePath("output.json");

  QJsonObject const input{
      {"name", "Woods And Hoards"},
      {MapJsonKeys::grid,
       QJsonObject{{MapJsonKeys::width, 64},
                   {MapJsonKeys::height, 64},
                   {MapJsonKeys::tile_size, 1.0}}},
      {MapJsonKeys::biome,
       QJsonObject{{"procedural_boulders_enabled", false},
                   {"procedural_iron_ore_enabled", false}}},
      {"forests",
       QJsonArray{QJsonObject{{"id", "north_screen"},
                              {MapJsonKeys::x, 20},
                              {MapJsonKeys::z, 30},
                              {MapJsonKeys::radius, 14.5}}}},
      {MapJsonKeys::undead_zones,
       QJsonArray{
           QJsonObject{{"id", "barrow"},
                       {MapJsonKeys::x, 40},
                       {MapJsonKeys::z, 40},
                       {"clear_reward", QJsonObject{{"gold", 120}, {"iron", 60}}}}}},
      {MapJsonKeys::wildlife,
       QJsonObject{{"enabled", true},
                   {"wolves",
                    QJsonObject{{"enabled", true},
                                {"groups", 0},
                                {"waves",
                                 QJsonArray{QJsonObject{{"timing", 240.0},
                                                        {"pack_size", 5},
                                                        {MapJsonKeys::x, 12},
                                                        {MapJsonKeys::z, 50},
                                                        {"label", "The pack"}}}}}}}}};
  write_json(input_path, input);

  MapEditor::MapData data;
  ASSERT_TRUE(data.load_from_json(input_path));

  ASSERT_EQ(data.forests().size(), 1);
  EXPECT_EQ(data.forests().first().id, "north_screen");
  EXPECT_FLOAT_EQ(data.forests().first().radius, 14.5F);
  ASSERT_EQ(data.undead_zones().size(), 1);
  EXPECT_EQ(data.undead_zones().first().clear_reward.value("gold").toInt(), 120);

  ASSERT_TRUE(data.save_to_json(output_path));
  const QJsonObject output = read_json(output_path);

  ASSERT_TRUE(output.value("forests").isArray());
  const QJsonObject saved_forest = output.value("forests").toArray().first().toObject();
  EXPECT_EQ(saved_forest.value("id").toString(), "north_screen");
  EXPECT_DOUBLE_EQ(saved_forest.value(MapJsonKeys::radius).toDouble(), 14.5);

  const QJsonObject saved_zone =
      output.value(MapJsonKeys::undead_zones).toArray().first().toObject();
  ASSERT_TRUE(saved_zone.value("clear_reward").isObject());
  EXPECT_EQ(saved_zone.value("clear_reward").toObject().value("iron").toInt(), 60);

  ASSERT_TRUE(output.value(MapJsonKeys::biome).isObject());
  EXPECT_FALSE(output.value(MapJsonKeys::biome)
                   .toObject()
                   .value("procedural_boulders_enabled")
                   .toBool(true));

  const QJsonArray saved_wolf_waves = output.value(MapJsonKeys::wildlife)
                                          .toObject()
                                          .value("wolves")
                                          .toObject()
                                          .value("waves")
                                          .toArray();
  ASSERT_EQ(saved_wolf_waves.size(), 1);
  EXPECT_DOUBLE_EQ(saved_wolf_waves.first().toObject().value("timing").toDouble(),
                   240.0);
  EXPECT_EQ(saved_wolf_waves.first().toObject().value("pack_size").toInt(), 5);
}

TEST(MapEditorMapDataTest, AGateKeepsTheRotationThatDecidesWhichAxisItSpans) {
  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString input_path = temp_dir.filePath("gate_rotation.json");
  const QString output_path = temp_dir.filePath("gate_rotation_out.json");

  const QJsonObject root{
      {"name", "Gate Rotation"},
      {MapJsonKeys::grid,
       QJsonObject{{MapJsonKeys::width, 64},
                   {MapJsonKeys::height, 64},
                   {MapJsonKeys::tile_size, 1.0}}},
      {MapJsonKeys::structures,
       QJsonArray{QJsonObject{{MapJsonKeys::type, "wall_gate"},
                              {MapJsonKeys::x, 20},
                              {MapJsonKeys::z, 42},
                              {MapJsonKeys::rotation, 90.0},
                              {MapJsonKeys::player_id, 2}},
                  QJsonObject{{MapJsonKeys::type, "wall_segment"},
                              {MapJsonKeys::start, QJsonArray{20, 22}},
                              {MapJsonKeys::end, QJsonArray{20, 38}},
                              {MapJsonKeys::width, 2.0},
                              {MapJsonKeys::player_id, 2}}}}};
  write_json(input_path, root);

  MapEditor::MapData data;
  QString error;
  ASSERT_TRUE(data.load_from_json(input_path, &error)) << error.toStdString();

  ASSERT_EQ(data.structures().size(), 1);
  EXPECT_EQ(data.structures().first().type, "wall_gate");
  EXPECT_FLOAT_EQ(data.structures().first().rotation, 90.0F);
  ASSERT_EQ(data.linear_elements().size(), 1)
      << "the wall run stays a linear element, not a structure";

  ASSERT_TRUE(data.save_to_json(output_path, &error)) << error.toStdString();
  const QJsonObject output = read_json(output_path);
  const QJsonArray saved = output.value(MapJsonKeys::structures).toArray();
  const auto gate = std::find_if(saved.begin(), saved.end(), [](const QJsonValue& v) {
    return v.toObject().value(MapJsonKeys::type).toString() == "wall_gate";
  });
  ASSERT_NE(gate, saved.end());
  EXPECT_DOUBLE_EQ(gate->toObject().value(MapJsonKeys::rotation).toDouble(), 90.0);
}
