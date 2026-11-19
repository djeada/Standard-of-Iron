#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <gtest/gtest.h>

#include "tools/map_editor/batch_edit.h"
#include "tools/map_editor/json_text.h"
#include "tools/map_editor/map_data.h"

namespace {

auto read_file(const QString& path) -> QByteArray {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

auto changed_lines(const QByteArray& before, const QByteArray& after) -> int {
  const QList<QByteArray> old_lines = before.split('\n');
  const QList<QByteArray> new_lines = after.split('\n');
  int changed = 0;
  for (const QByteArray& line : new_lines) {
    if (!old_lines.contains(line)) {
      ++changed;
    }
  }
  return changed;
}

const QByteArray k_small_map = R"({
  "name": "Fixture",
  "coord_system": "grid",
  "grid": {
    "width": 64,
    "height": 64,
    "tile_size": 1.0
  },
  "time_of_day": "morning",
  "biome": {
    "ground_type": "forest_mud",
    "irregularity_amplitude": 0.075,
    "plant_density": 1.18
  },
  "terrain": [
    {
      "id": "north_hill",
      "type": "hill",
      "x": 20.1234,
      "z": 30.5,
      "width": 18.0,
      "depth": 12.0,
      "height": 4.25,
      "entrances": []
    }
  ],
  "world_props": [
    {
      "id": "rack",
      "type": "weapon_rack",
      "x": 12,
      "z": 9.5678,
      "rotation": 1.569
    }
  ],
  "firecamps": [
    {
      "id": "camp_fire",
      "x": 10.25,
      "z": 11.75,
      "intensity": 1.05,
      "radius": 3.5
    }
  ],
  "undead_zones": []
}
)";

} // namespace

TEST(MapEditorLosslessSave, EveryShippedMapSavesByteForByte) {
  const QDir maps(QStringLiteral("assets/maps"));
  ASSERT_TRUE(maps.exists()) << maps.absolutePath().toStdString();
  const QStringList files =
      maps.entryList({QStringLiteral("map_*.json")}, QDir::Files, QDir::Name);
  ASSERT_FALSE(files.isEmpty());
  for (const QString& name : files) {
    const QByteArray source = read_file(maps.filePath(name));
    MapEditor::MapData data;
    QString error;
    ASSERT_TRUE(data.load_from_bytes(source, &error))
        << name.toStdString() << ": " << error.toStdString();
    EXPECT_TRUE(data.to_json_bytes() == source)
        << name.toStdString()
        << ": opening and saving a map must not change a byte of it";
  }
}

TEST(MapEditorLosslessSave, AnEditRewritesOnlyWhatItTouched) {
  MapEditor::MapData data;
  ASSERT_TRUE(data.load_from_bytes(k_small_map));

  const QJsonArray ops =
      QJsonDocument::fromJson(R"([{"op":"move","kind":"terrain","id":"north_hill",
                                   "to":[24,30.5]}])")
          .array();
  const auto report = MapEditor::BatchEdit::apply(data, ops);
  ASSERT_TRUE(report.ok()) << report.errors.join('\n').toStdString();

  const QByteArray saved = data.to_json_bytes();
  EXPECT_EQ(changed_lines(k_small_map, saved), 1) << saved.toStdString();
  EXPECT_TRUE(saved.contains("\"x\": 24,"));
  EXPECT_TRUE(saved.contains("\"time_of_day\": \"morning\""));
  EXPECT_TRUE(saved.contains("\"irregularity_amplitude\": 0.075"));
  EXPECT_TRUE(saved.contains("\"rotation\": 1.569"));
  EXPECT_TRUE(saved.contains("\"z\": 9.5678"));
  EXPECT_TRUE(saved.contains("\"firecamps\""));
  EXPECT_TRUE(saved.contains("\"undead_zones\": []"));
}

TEST(MapEditorLosslessSave, LegacyFirecampsAreNeitherLostNorDuplicated) {
  MapEditor::MapData data;
  ASSERT_TRUE(data.load_from_bytes(k_small_map));

  const QJsonArray ops =
      QJsonDocument::fromJson(R"([{"op":"set","kind":"prop","id":"rack",
                                   "fields":{"x":14}}])")
          .array();
  ASSERT_TRUE(MapEditor::BatchEdit::apply(data, ops).ok());

  const QJsonObject root = QJsonDocument::fromJson(data.to_json_bytes()).object();
  EXPECT_EQ(root.value("firecamps").toArray().size(), 1);
  EXPECT_EQ(root.value("world_props").toArray().size(), 1);
  EXPECT_DOUBLE_EQ(root.value("firecamps").toArray()[0].toObject().value("x").toDouble(),
                   10.25);
}

TEST(MapEditorLosslessSave, ReloadingASavedEditKeepsItStable) {
  MapEditor::MapData data;
  ASSERT_TRUE(data.load_from_bytes(k_small_map));
  const QJsonArray ops = QJsonDocument::fromJson(
                             R"([{"op":"add","kind":"forest",
                                  "json":{"id":"wood","x":30,"z":40,"radius":9}},
                                 {"op":"map","biome":{"plant_density":1.31}}])")
                             .array();
  ASSERT_TRUE(MapEditor::BatchEdit::apply(data, ops).ok());
  const QByteArray first = data.to_json_bytes();

  MapEditor::MapData reloaded;
  ASSERT_TRUE(reloaded.load_from_bytes(first));
  EXPECT_TRUE(reloaded.to_json_bytes() == first);
  EXPECT_EQ(reloaded.forests().size(), 1);
}

TEST(MapEditorBatchEdit, RootOperationsEditDataTheEditorDoesNotModel) {
  MapEditor::MapData data;
  ASSERT_TRUE(data.load_from_bytes(k_small_map));
  const auto run = [&](const char* json) {
    return MapEditor::BatchEdit::apply(data, QJsonDocument::fromJson(json).array());
  };

  ASSERT_TRUE(run(R"([{"op":"root","key":"dressing","append":[
                        {"id":"mill_copse","kind":"copse","x":20,"z":22},
                        {"id":"ford_cairn","kind":"cairn","x":40,"z":12}]}])")
                  .ok());
  ASSERT_TRUE(run(R"([{"op":"root","key":"dressing","remove_ids":["mill_copse"]}])").ok());
  ASSERT_TRUE(run(R"([{"op":"root","key":"scouted_areas","value":[{"x":1,"z":2}]}])").ok());

  MapEditor::MapData reloaded;
  ASSERT_TRUE(reloaded.load_from_bytes(data.to_json_bytes()));
  const QJsonArray dressing = reloaded.extra_root_field("dressing").toArray();
  ASSERT_EQ(dressing.size(), 1);
  EXPECT_EQ(dressing[0].toObject().value("id").toString(), QStringLiteral("ford_cairn"));
  EXPECT_EQ(reloaded.extra_root_field("scouted_areas").toArray().size(), 1);

  EXPECT_FALSE(run(R"([{"op":"root","key":"dressing","append":[{"id":"ford_cairn"}]}])").ok())
      << "a second piece with the same id would be composed twice";
  EXPECT_FALSE(run(R"([{"op":"root","key":"dressing","remove_ids":["nope"]}])").ok());
  EXPECT_FALSE(run(R"([{"op":"root","key":"forests","value":[]}])").ok())
      << "forests are edited as elements, never overwritten wholesale";
  EXPECT_FALSE(run(R"([{"op":"root","key":"dressing"}])").ok());
}

TEST(MapEditorBatchEdit, ReportsWhatItCannotApplyAndNamesTheStep) {
  MapEditor::MapData data;
  ASSERT_TRUE(data.load_from_bytes(k_small_map));
  const QJsonArray ops =
      QJsonDocument::fromJson(R"([{"op":"remove","kind":"terrain","id":"missing"},
                                  {"op":"move","kind":"prop","index":0},
                                  {"op":"paint","kind":"prop","index":0}])")
          .array();
  const auto report = MapEditor::BatchEdit::apply(data, ops);
  EXPECT_EQ(report.applied, 0);
  ASSERT_EQ(report.errors.size(), 3);
  EXPECT_TRUE(report.errors[0].startsWith(QStringLiteral("step 1")));
  EXPECT_TRUE(data.to_json_bytes() == k_small_map);
}

TEST(MapEditorBatchEdit, InventoryListsEveryElementWithItsIdAndPosition) {
  MapEditor::MapData data;
  ASSERT_TRUE(data.load_from_bytes(k_small_map));
  const QJsonArray rows = MapEditor::BatchEdit::inventory(data);
  ASSERT_EQ(rows.size(), 3);
  const QJsonObject hill = rows[0].toObject();
  EXPECT_EQ(hill.value("kind").toString(), QStringLiteral("terrain"));
  EXPECT_EQ(hill.value("id").toString(), QStringLiteral("north_hill"));
  EXPECT_NEAR(hill.value("x").toDouble(), 20.12, 0.01);
  EXPECT_EQ(MapEditor::BatchEdit::find_by_id(
                data,
                MapEditor::BatchEdit::kind_from_name(QStringLiteral("prop")),
                QStringLiteral("rack")),
            0);
}

TEST(MapEditorJsonText, KeepsKeyOrderNumberTextAndLayout) {
  const QByteArray source =
      "{\n    \"zeta\": 1.50,\n    \"alpha\": [\n    ],\n    \"mid\": \"a\\u00e9\"\n}";
  MapEditor::JsonText::Node node;
  QString error;
  ASSERT_TRUE(MapEditor::JsonText::parse(source, node, &error)) << error.toStdString();
  EXPECT_TRUE(MapEditor::JsonText::write(
                  node, MapEditor::JsonText::detect_style(source)) == source);
  EXPECT_DOUBLE_EQ(
      MapEditor::JsonText::to_value(node).toObject().value("zeta").toDouble(), 1.5);
}
