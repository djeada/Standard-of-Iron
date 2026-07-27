#include <gtest/gtest.h>

#include "tools/map_editor/element_ops.h"
#include "tools/map_editor/json_schema.h"

namespace {

using MapEditor::ElementKind;

auto keys_of(const MapEditor::JsonSchema& schema) -> QStringList {
  QStringList keys;
  keys.reserve(schema.fields.size());
  for (const MapEditor::JsonFieldSpec& field : schema.fields) {
    keys << field.key;
  }
  return keys;
}

TEST(MapEditorJsonSchemaTest, EveryElementKindHasADocumentedSchema) {
  for (int kind = 0; kind < MapEditor::k_element_kind_count; ++kind) {
    const MapEditor::JsonSchema schema = MapEditor::schema_for_element(kind, QString());
    EXPECT_FALSE(schema.is_empty()) << "kind " << kind;
    EXPECT_FALSE(schema.title.isEmpty()) << "kind " << kind;

    for (const MapEditor::JsonFieldSpec& field : schema.fields) {
      EXPECT_FALSE(field.key.isEmpty());
      EXPECT_FALSE(field.type.isEmpty());
      EXPECT_FALSE(field.description.isEmpty()) << field.key.toStdString();
      EXPECT_FALSE(field.placeholder.isUndefined()) << field.key.toStdString();
    }
  }
}

TEST(MapEditorJsonSchemaTest, PositionalKeysAreRequiredOnPlacedElements) {
  for (int kind : {static_cast<int>(ElementKind::Terrain),
                   static_cast<int>(ElementKind::WorldProp),
                   static_cast<int>(ElementKind::Structure),
                   static_cast<int>(ElementKind::TroopSpawn)}) {
    const MapEditor::JsonSchema schema = MapEditor::schema_for_element(kind, QString());
    const MapEditor::JsonFieldSpec* x = schema.find(QStringLiteral("x"));
    const MapEditor::JsonFieldSpec* z = schema.find(QStringLiteral("z"));
    ASSERT_NE(x, nullptr) << "kind " << kind;
    ASSERT_NE(z, nullptr) << "kind " << kind;
    EXPECT_TRUE(x->required);
    EXPECT_TRUE(z->required);
  }
}

TEST(MapEditorJsonSchemaTest, TerrainSchemaTracksTheSubType) {
  const MapEditor::JsonSchema hill = MapEditor::schema_for_element(
      static_cast<int>(ElementKind::Terrain), QStringLiteral("hill"));
  const MapEditor::JsonSchema mountain = MapEditor::schema_for_element(
      static_cast<int>(ElementKind::Terrain), QStringLiteral("mountain"));

  EXPECT_NE(hill.find(QStringLiteral("entrances")), nullptr);
  EXPECT_EQ(mountain.find(QStringLiteral("entrances")), nullptr);
  EXPECT_EQ(mountain.find(QStringLiteral("height"))->default_value,
            QStringLiteral("8"));
}

TEST(MapEditorJsonSchemaTest, LinearSchemaExposesSubTypeSpecificKeys) {
  const MapEditor::JsonSchema bridge = MapEditor::schema_for_element(
      static_cast<int>(ElementKind::Linear), QStringLiteral("bridge"));
  const MapEditor::JsonSchema wall = MapEditor::schema_for_element(
      static_cast<int>(ElementKind::Linear), QStringLiteral("wall"));

  EXPECT_NE(bridge.find(QStringLiteral("height")), nullptr);
  EXPECT_EQ(bridge.find(QStringLiteral("player_id")), nullptr);
  EXPECT_NE(wall.find(QStringLiteral("player_id")), nullptr);
  EXPECT_TRUE(wall.find(QStringLiteral("start"))->required);
}

TEST(MapEditorJsonSchemaTest, FirecampAndStaticPropsDifferInKeys) {
  const MapEditor::JsonSchema firecamp = MapEditor::schema_for_element(
      static_cast<int>(ElementKind::WorldProp), QStringLiteral("firecamp"));
  const MapEditor::JsonSchema tent = MapEditor::schema_for_element(
      static_cast<int>(ElementKind::WorldProp), QStringLiteral("tent"));

  EXPECT_NE(firecamp.find(QStringLiteral("intensity")), nullptr);
  EXPECT_NE(firecamp.find(QStringLiteral("persistent")), nullptr);
  EXPECT_EQ(tent.find(QStringLiteral("intensity")), nullptr);
  EXPECT_NE(tent.find(QStringLiteral("scale")), nullptr);
}

TEST(MapEditorJsonSchemaTest, BiomeSchemaCoversGroundTypeAndPresetOverrides) {
  const MapEditor::JsonSchema schema = MapEditor::schema_for_biome();
  const QStringList keys = keys_of(schema);

  ASSERT_FALSE(schema.is_empty());
  EXPECT_TRUE(keys.contains(QStringLiteral("ground_type")));
  EXPECT_TRUE(keys.contains(QStringLiteral("snow_coverage")));
  EXPECT_TRUE(keys.contains(QStringLiteral("grass_primary")));
  EXPECT_TRUE(keys.contains(QStringLiteral("irregularity_amplitude")));

  const MapEditor::JsonFieldSpec* ground = schema.find(QStringLiteral("ground_type"));
  ASSERT_NE(ground, nullptr);
  EXPECT_TRUE(ground->allowed.contains(QStringLiteral("alpine_mix")));
  EXPECT_FALSE(ground->required);
}

TEST(MapEditorJsonSchemaTest, UnknownKindYieldsEmptySchema) {
  EXPECT_TRUE(MapEditor::schema_for_element(42, QString()).is_empty());
  EXPECT_EQ(MapEditor::JsonSchema{}.find(QStringLiteral("x")), nullptr);
}

} // namespace
