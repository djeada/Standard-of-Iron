#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cmath>
#include <gtest/gtest.h>

#include "tools/map_editor/batch_edit.h"
#include "tools/map_editor/element_edit_json.h"
#include "tools/map_editor/map_data.h"

namespace {

// A blank world-space map, as `map_cli new` writes one.
const QByteArray k_blank_map = R"({
  "name": "Town ops",
  "coord_system": "world",
  "grid": {"width": 200, "height": 200, "tile_size": 1},
  "biome": {"seed": 7},
  "camera": {"center": [0, 0, 0], "distance": 20, "tilt_deg": 45, "yaw": 225},
  "structures": [],
  "spawns": []
})";

auto apply(MapEditor::MapData& data, const char* json) -> MapEditor::BatchEdit::Report {
  const QJsonDocument document = QJsonDocument::fromJson(json);
  EXPECT_TRUE(document.isArray()) << json;
  return MapEditor::BatchEdit::apply(data, document.array());
}

auto saved(const MapEditor::MapData& data) -> QJsonObject {
  return QJsonDocument::fromJson(data.to_json_bytes()).object();
}

auto structures_of(const QJsonObject& map,
                   const QString& type) -> QVector<QJsonObject> {
  QVector<QJsonObject> out;
  for (const QJsonValue value : map.value(QStringLiteral("structures")).toArray()) {
    if (value.toObject().value(QStringLiteral("type")).toString() == type) {
      out.append(value.toObject());
    }
  }
  return out;
}

} // namespace

TEST(MapEditorTownOps, ATerraceOfPorticoesStandsOnTheMarketLotPitch) {
  MapEditor::MapData data;
  ASSERT_TRUE(data.load_from_bytes(k_blank_map));
  const auto report =
      apply(data,
            R"([{"op":"terrace","id":"stoa","from":[0,10],"to":[33,10],"facing":0,
                 "type":"marketplace","style":"portico","player_id":1,
                 "nation":"roman_republic"}])");
  ASSERT_TRUE(report.ok()) << report.errors.join('\n').toStdString();

  const auto lots = structures_of(saved(data), QStringLiteral("marketplace"));
  ASSERT_EQ(lots.size(), 6) << "33 units hold six 5.5-unit lots";
  for (int index = 0; index < lots.size(); ++index) {
    EXPECT_EQ(lots[index].value(QStringLiteral("style")).toString(),
              QStringLiteral("portico"));
    EXPECT_NEAR(lots[index].value(QStringLiteral("z")).toDouble(), 10.0, 1e-4);
    if (index > 0) {
      EXPECT_NEAR(lots[index].value(QStringLiteral("x")).toDouble() -
                      lots[index - 1].value(QStringLiteral("x")).toDouble(),
                  5.5,
                  1e-3)
          << "neighbouring porticoes have to meet to read as one stoa";
    }
  }
}

TEST(MapEditorTownOps, AStyleThatIsNotOneOfTheBuildingsIsRefused) {
  MapEditor::MapData data;
  ASSERT_TRUE(data.load_from_bytes(k_blank_map));
  const auto report = apply(data,
                            R"([{"op":"terrace","from":[0,10],"to":[33,10],"facing":0,
                 "type":"marketplace","style":"tenement"}])");
  EXPECT_FALSE(report.ok());
  EXPECT_TRUE(structures_of(saved(data), QStringLiteral("marketplace")).isEmpty());
}

TEST(MapEditorTownOps, AMasonryWallKeepsItsStyleOnEveryRunAndItsGates) {
  MapEditor::MapData data;
  ASSERT_TRUE(data.load_from_bytes(k_blank_map));
  const auto report = apply(
      data,
      R"([{"op":"wall","id":"circuit","points":[[-40,-40],[40,-40],[40,40],[-40,40]],
                 "style":"masonry","tower_spacing":44,"player_id":1,
                 "nation":"roman_republic"},
                {"op":"gate","id":"north_gate","at":[0,-40]}])");
  ASSERT_TRUE(report.ok()) << report.errors.join('\n').toStdString();

  const QJsonObject map = saved(data);
  const auto runs = structures_of(map, QStringLiteral("wall_segment"));
  ASSERT_GE(runs.size(), 5) << "four legs, one of them cut in two by the gate";
  for (const QJsonObject& run : runs) {
    EXPECT_EQ(run.value(QStringLiteral("style")).toString(), QStringLiteral("masonry"));
  }
  const auto gates = structures_of(map, QStringLiteral("wall_gate"));
  ASSERT_EQ(gates.size(), 1);
  EXPECT_EQ(gates.front().value(QStringLiteral("style")).toString(),
            QStringLiteral("masonry"))
      << "a gate cut into a stone wall is a stone gate";
  EXPECT_GE(structures_of(map, QStringLiteral("defense_tower")).size(), 4);
}

TEST(MapEditorTownOps, AWallWithNoStyleStaysTheNationsOwn) {
  MapEditor::MapData data;
  ASSERT_TRUE(data.load_from_bytes(k_blank_map));
  ASSERT_TRUE(apply(data,
                    R"([{"op":"wall","points":[[-20,0],[20,0]],"closed":false,
                         "tower_spacing":0}])")
                  .ok());
  const auto runs = structures_of(saved(data), QStringLiteral("wall_segment"));
  ASSERT_EQ(runs.size(), 1);
  EXPECT_FALSE(runs.front().contains(QStringLiteral("style")));
}

TEST(MapEditorTownOps, PavingTakesTheFlagstoneStyle) {
  MapEditor::MapData data;
  ASSERT_TRUE(data.load_from_bytes(k_blank_map));
  ASSERT_TRUE(apply(data,
                    R"([{"op":"paving","rect":[0,0,40,24],"style":"flagstone",
                         "strip":12}])")
                  .ok());
  const QJsonArray roads = saved(data).value(QStringLiteral("roads")).toArray();
  ASSERT_GE(roads.size(), 2);
  for (const QJsonValue road : roads) {
    EXPECT_EQ(road.toObject().value(QStringLiteral("style")).toString(),
              QStringLiteral("flagstone"));
  }
}

TEST(MapEditorTownOps, ALakeAddedThroughTheEditorKeepsItsLevelAndShape) {
  MapEditor::MapData data;
  ASSERT_TRUE(data.load_from_bytes(k_blank_map));
  ASSERT_TRUE(apply(data,
                    R"([{"op":"add","kind":"terrain","json":
                          {"type":"lake","x":10,"z":20,"width":60,"depth":40,
                           "height":-0.6,"shape":"basin"}},
                        {"op":"add","kind":"terrain","json":
                          {"type":"lake","x":-60,"z":-60,"width":30,"depth":30}}])")
                  .ok());

  const QJsonArray lakes = saved(data).value(QStringLiteral("lakes")).toArray();
  ASSERT_EQ(lakes.size(), 2);
  const QJsonObject basin = lakes[0].toObject();
  EXPECT_DOUBLE_EQ(basin.value(QStringLiteral("height")).toDouble(), -0.6);
  EXPECT_EQ(basin.value(QStringLiteral("shape")).toString(), QStringLiteral("basin"));
  const QJsonObject pond = lakes[1].toObject();
  EXPECT_FALSE(pond.contains(QStringLiteral("height")))
      << "a lake nobody gave a level lies with the ground: writing one would pin it";
  EXPECT_FALSE(pond.contains(QStringLiteral("shape")));

  // And once more through a reload, as opening the saved map would.
  MapEditor::MapData reloaded;
  ASSERT_TRUE(reloaded.load_from_bytes(data.to_json_bytes()));
  EXPECT_TRUE(reloaded.to_json_bytes() == data.to_json_bytes());
}

TEST(MapEditorTownOps, EditingALakeInTheDialogDoesNotLoseItsLevel) {
  MapEditor::TerrainElement lake;
  lake.type = QStringLiteral("lake");
  lake.x = 4.0F;
  lake.z = 8.0F;
  lake.width = 50.0F;
  lake.depth = 30.0F;
  lake.height = -0.6F;
  lake.level_authored = true;
  lake.shape = QStringLiteral("round");

  namespace EditJson = MapEditor::ElementEditJson;
  const MapEditor::ElementSnapshot before{lake};
  const auto document = EditJson::to_document(before);
  ASSERT_TRUE(document.has_value());
  EXPECT_NEAR(document->json.value(QStringLiteral("height")).toDouble(), -0.6, 1e-6);
  const auto result = EditJson::from_json(before, document->json, {});
  const auto* after = std::get_if<MapEditor::TerrainElement>(&result.element);
  ASSERT_NE(after, nullptr);
  EXPECT_TRUE(after->level_authored);
  EXPECT_NEAR(after->height, -0.6F, 1e-6F);
  EXPECT_EQ(after->shape, QStringLiteral("round"));
}

TEST(MapEditorTownOps, ARiverKeepsTheWaterLevelItWasGiven) {
  MapEditor::MapData data;
  ASSERT_TRUE(data.load_from_bytes(k_blank_map));
  ASSERT_TRUE(apply(data,
                    R"([{"op":"add","kind":"linear","json":
                          {"type":"river","start":[-80,0],"end":[80,0],"width":20,
                           "height":-0.6}},
                        {"op":"add","kind":"linear","json":
                          {"type":"river","start":[-80,40],"end":[80,40],"width":8}}])")
                  .ok());
  const QJsonArray rivers = saved(data).value(QStringLiteral("rivers")).toArray();
  ASSERT_EQ(rivers.size(), 2);
  EXPECT_DOUBLE_EQ(rivers[0].toObject().value(QStringLiteral("height")).toDouble(),
                   -0.6);
  EXPECT_FALSE(rivers[1].toObject().contains(QStringLiteral("height")))
      << "a river with no level given runs with the ground";
}
