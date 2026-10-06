#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <gtest/gtest.h>

#include "tools/map_editor/element_edit_json.h"
#include "tools/map_editor/element_ops.h"
#include "tools/map_editor/map_data.h"

namespace {

using MapEditor::ElementKind;
using MapEditor::ElementSnapshot;
namespace Ops = MapEditor::ElementOps;

const QByteArray k_generated_map = R"({
  "name": "Generated",
  "grid": {"width": 650, "height": 650, "tile_size": 1},
  "generation": {"version": 1, "generator": "soi_mapgen", "seed": 42,
                 "preset": "river_crossing", "width": 650, "height": 650,
                 "parameters": {"water.rivers": 1}, "locks": ["terrain"]},
  "settlements": [{"id": "east_town", "tier": "town", "x": 540, "z": 110,
                   "generated": "settlements"}],
  "landmarks": [{"id": "shrine", "kind": "shrine", "x": 300, "z": 400,
                 "generated": "landmarks"}],
  "dressing": [{"id": "ford", "kind": "ford_wreck", "x": 330, "z": 320,
                "generated": "dressing"}],
  "terrain": [{"type": "hill", "x": 180, "z": 470, "width": 90, "depth": 60,
               "height": 2.5, "generated": "terrain"}],
  "rivers": [{"start": [0, 330], "end": [649, 300], "width": 14,
              "generated": "water"}],
  "lakes": [{"x": 12, "z": 120, "width": 40, "depth": 36, "generated": "water"}],
  "roads": [{"start": [110, 0], "end": [540, 649], "style": "default",
             "generated": "roads"}],
  "bridges": [{"start": [318, 306], "end": [317, 320], "width": 8, "height": 0.5,
               "generated": "roads"}],
  "forests": [{"id": "wood_a", "x": 300, "z": 120, "radius": 26,
               "generated": "forests"},
              {"id": "authored_wood", "x": 400, "z": 520, "radius": 20}],
  "structures": [{"type": "barracks", "x": 540, "z": 110, "player_id": 2,
                  "settlement": "east_town", "generated": "settlements"},
                 {"type": "home", "x": 100, "z": 100, "player_id": 1}],
  "world_props": [{"type": "boulder", "x": 331, "z": 322, "scale": 1,
                   "dressing": "ford", "generated": "dressing"}],
  "spawns": [{"type": "swordsman", "x": 120, "z": 560, "player_id": 1,
              "generated": "spawns"}]
})";

auto object_of(const QByteArray& json) -> QJsonObject {
  return QJsonDocument::fromJson(json).object();
}

auto count_tagged(const QJsonArray& array) -> int {
  int count = 0;
  for (const QJsonValue value : array) {
    count += value.toObject().contains(QStringLiteral("generated")) ? 1 : 0;
  }
  return count;
}

TEST(MapEditorGeneratedDocumentTest, ProvenanceAndIntentSurviveLoadAndSave) {
  MapEditor::MapData data;
  QString error;
  ASSERT_TRUE(data.load_from_bytes(k_generated_map, &error)) << error.toStdString();
  EXPECT_EQ(data.generation().value(QStringLiteral("seed")).toInt(), 42);

  const QJsonObject saved = object_of(data.to_json_bytes());
  EXPECT_EQ(saved.value(QStringLiteral("generation")).toObject(),
            object_of(k_generated_map).value(QStringLiteral("generation")).toObject());
  for (const char* key : {"settlements", "landmarks", "dressing"}) {
    EXPECT_EQ(count_tagged(saved.value(QLatin1String(key)).toArray()), 1) << key;
  }
  for (const char* key :
       {"terrain", "rivers", "lakes", "roads", "bridges", "world_props", "spawns"}) {
    EXPECT_EQ(count_tagged(saved.value(QLatin1String(key)).toArray()), 1) << key;
  }
  EXPECT_EQ(count_tagged(saved.value(QStringLiteral("forests")).toArray()), 1)
      << "forests used to drop every key but id, x, z and radius";
  EXPECT_EQ(count_tagged(saved.value(QStringLiteral("structures")).toArray()), 1);

  MapEditor::MapData reloaded;
  ASSERT_TRUE(reloaded.load_from_bytes(data.to_json_bytes(), &error));
  EXPECT_EQ(reloaded.to_json_bytes(), data.to_json_bytes());
}

TEST(MapEditorGeneratedDocumentTest, ReplacingTheDocumentIsOneUndoStep) {
  MapEditor::MapData data;
  QString error;
  MapEditor::ForestElement forest;
  forest.id = QStringLiteral("before_wood");
  data.add_forest(forest);
  data.set_modified(false);
  const QByteArray before = data.to_json_bytes();

  ASSERT_TRUE(
      data.replace_document(k_generated_map, QStringLiteral("Generate map"), &error))
      << error.toStdString();
  EXPECT_TRUE(data.is_modified());
  EXPECT_EQ(data.grid().width, 650);
  EXPECT_EQ(data.forests().size(), 2);
  EXPECT_EQ(data.undo_description(), QStringLiteral("Generate map"));
  const QByteArray after = data.to_json_bytes();

  data.undo();
  EXPECT_EQ(data.to_json_bytes(), before);
  EXPECT_FALSE(data.can_undo());
  EXPECT_EQ(data.grid().width, 100) << "a document without a grid falls back to 100";

  data.redo();
  EXPECT_EQ(data.to_json_bytes(), after);
}

TEST(MapEditorGeneratedDocumentTest, AnUnreadableCandidateLeavesTheDocumentAlone) {
  MapEditor::MapData data;
  const QByteArray before = data.to_json_bytes();
  QString error;
  EXPECT_FALSE(data.replace_document("{ not json", QStringLiteral("Generate"), &error));
  EXPECT_FALSE(error.isEmpty());
  EXPECT_FALSE(
      data.replace_document(R"({"buildings": []})", QStringLiteral("Gen"), &error));
  EXPECT_FALSE(data.can_undo());
  EXPECT_EQ(data.to_json_bytes(), before);
}

TEST(MapEditorGeneratedDocumentTest, LoadingASecondMapDropsTheFirstMapsZones) {
  MapEditor::MapData data;
  const QByteArray with_zones = R"({
    "grid": {"width": 100, "height": 100, "tile_size": 1},
    "undead_zones": [{"id": "crypt", "x": 10, "z": 10}],
    "fog_zones": [{"x": 5, "z": 5, "width": 4, "height": 4}]
  })";
  ASSERT_TRUE(data.load_from_bytes(with_zones));
  ASSERT_TRUE(data.load_from_bytes(with_zones));
  EXPECT_EQ(data.undead_zones().size(), 1);
  EXPECT_EQ(data.fog_zones().size(), 1);
  ASSERT_TRUE(data.load_from_bytes(R"({"grid": {"width": 100, "height": 100}})"));
  EXPECT_TRUE(data.undead_zones().isEmpty());
  EXPECT_TRUE(data.fog_zones().isEmpty());
}

TEST(MapEditorGeneratedDocumentTest, EditingAGeneratedElementMakesItAuthored) {
  MapEditor::MapData data;
  ASSERT_TRUE(data.load_from_bytes(k_generated_map));
  const int kind = static_cast<int>(ElementKind::Structure);
  const ElementSnapshot generated = Ops::snapshot(data, kind, 0);
  const ElementSnapshot authored = Ops::snapshot(data, kind, 1);
  ASSERT_TRUE(Ops::is_generated(generated));
  ASSERT_FALSE(Ops::is_generated(authored));
  EXPECT_EQ(Ops::count_generated({generated, authored}), 1);

  const ElementSnapshot moved = Ops::translated(generated, QPointF(4.0, 0.0));
  data.execute_command(
      Ops::make_update(data, 0, generated, moved, QStringLiteral("Move")));

  const auto& edited = data.structures()[0];
  EXPECT_FLOAT_EQ(edited.x, 544.0F);
  for (const char* key : Ops::k_generation_ownership_keys) {
    EXPECT_FALSE(edited.extra_fields.contains(QLatin1String(key))) << key;
  }
  EXPECT_EQ(data.structures()[1].extra_fields,
            std::get<MapEditor::StructureElement>(authored).extra_fields)
      << "only the edited element changes";

  data.undo();
  EXPECT_EQ(
      data.structures()[0].extra_fields.value(QStringLiteral("settlement")).toString(),
      QStringLiteral("east_town"));
  EXPECT_TRUE(Ops::is_generated(Ops::snapshot(data, kind, 0)));
}

TEST(MapEditorGeneratedDocumentTest, ForestExtrasRoundTripThroughTheEditDialog) {
  MapEditor::ForestElement forest;
  forest.id = QStringLiteral("wood");
  forest.radius = 18.0F;
  forest.extra_fields[QStringLiteral("generated")] = QStringLiteral("forests");
  forest.extra_fields[QStringLiteral("name")] = QStringLiteral("Silva Nigra");

  const ElementSnapshot before{forest};
  const auto document = MapEditor::ElementEditJson::to_document(before);
  ASSERT_TRUE(document.has_value());
  EXPECT_EQ(document->json.value(QStringLiteral("name")).toString(),
            QStringLiteral("Silva Nigra"));
  const auto result = MapEditor::ElementEditJson::from_json(before, document->json, {});
  EXPECT_EQ(std::get<MapEditor::ForestElement>(result.element).extra_fields,
            forest.extra_fields);
}

} // namespace
