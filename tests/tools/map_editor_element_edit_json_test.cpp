#include <QJsonArray>
#include <QJsonObject>

#include <gtest/gtest.h>

#include "tools/map_editor/element_edit_json.h"
#include "tools/map_editor/map_json_keys.h"

namespace {

namespace EditJson = MapEditor::ElementEditJson;
namespace MapJsonKeys = MapEditor::MapJsonKeys;
using MapEditor::ElementSnapshot;

template <typename Element>
auto round_trip(const Element& element,
                const QVector<MapEditor::LinearElement>& linear = {}) -> Element {
  const ElementSnapshot before{element};
  const auto document = EditJson::to_document(before);
  EXPECT_TRUE(document.has_value());
  const EditJson::EditResult result =
      EditJson::from_json(before, document->json, linear);
  const auto* after = std::get_if<Element>(&result.element);
  EXPECT_NE(after, nullptr);
  return after != nullptr ? *after : Element{};
}

auto arc_hill() -> MapEditor::TerrainElement {
  MapEditor::TerrainElement hill;
  hill.type = QStringLiteral("hill");
  hill.x = 40.0F;
  hill.z = 52.0F;
  hill.radius = 0.0F;
  hill.width = 40.0F;
  hill.depth = 36.0F;
  hill.height = 4.5F;
  hill.rotation = 30.0F;
  hill.shape = QStringLiteral("arc");
  hill.thickness = 10.0F;
  hill.has_arc = true;
  hill.arc = 120.0F;
  hill.has_arc_start = true;
  hill.arc_start = 15.0F;
  hill.taper = 0.35F;
  hill.cells = QJsonArray{QJsonArray{1, 2}, QJsonArray{2, 2}};
  hill.points = QJsonArray{QJsonArray{0.0, 0.0}, QJsonArray{4.0, 1.0}};
  hill.entrances = QJsonArray{QJsonObject{{"x", 41.0}, {"z", 60.0}, {"radius", 3.0}}};
  hill.extra_fields[QStringLiteral("authored_note")] = QStringLiteral("keep");
  return hill;
}

TEST(MapEditorElementEditJsonTest, ShapedHillKeepsItsShapeThroughTheEditDialog) {
  const MapEditor::TerrainElement before = arc_hill();
  const MapEditor::TerrainElement after = round_trip(before);

  EXPECT_EQ(after.shape, before.shape);
  EXPECT_FLOAT_EQ(after.thickness, before.thickness);
  EXPECT_TRUE(after.has_arc);
  EXPECT_FLOAT_EQ(after.arc, before.arc);
  EXPECT_TRUE(after.has_arc_start);
  EXPECT_FLOAT_EQ(after.arc_start, before.arc_start);
  EXPECT_FLOAT_EQ(after.taper, before.taper);
  EXPECT_EQ(after.cells, before.cells);
  EXPECT_EQ(after.points, before.points);
  EXPECT_EQ(after.entrances, before.entrances);
  EXPECT_FLOAT_EQ(after.width, before.width);
  EXPECT_FLOAT_EQ(after.depth, before.depth);
  EXPECT_FLOAT_EQ(after.rotation, before.rotation);
  EXPECT_EQ(after.extra_fields, before.extra_fields);
  EXPECT_TRUE(after.extra_fields.value(MapJsonKeys::shape).isUndefined())
      << "shape must land in the element, not in its extra fields";
}

TEST(MapEditorElementEditJsonTest, EditDocumentAlwaysOffersRotationAndHillProjection) {
  MapEditor::TerrainElement hill;
  hill.type = QStringLiteral("hill");
  const auto document = EditJson::to_document(ElementSnapshot{hill});

  ASSERT_TRUE(document.has_value());
  EXPECT_TRUE(document->json.contains(MapJsonKeys::rotation));
  EXPECT_TRUE(document->hill_projection);
  EXPECT_EQ(document->title, QStringLiteral("Edit Terrain: hill"));
}

TEST(MapEditorElementEditJsonTest, MountainsNeverCarryEntrances) {
  MapEditor::TerrainElement mountain = arc_hill();
  mountain.type = QStringLiteral("mountain");
  const auto document = EditJson::to_document(ElementSnapshot{mountain});
  ASSERT_TRUE(document.has_value());
  EXPECT_FALSE(document->json.contains(MapJsonKeys::entrances));

  QJsonObject edited = document->json;
  edited[MapJsonKeys::entrances] = mountain.entrances;
  const auto result = EditJson::from_json(ElementSnapshot{mountain}, edited, {});
  EXPECT_TRUE(std::get<MapEditor::TerrainElement>(result.element).entrances.isEmpty());
}

TEST(MapEditorElementEditJsonTest, StructureKeepsOrderAndUnknownKeys) {
  MapEditor::StructureElement barracks;
  barracks.type = QStringLiteral("barracks");
  barracks.x = 12.0F;
  barracks.z = 8.0F;
  barracks.rotation = 90.0F;
  barracks.player_id = 2;
  barracks.max_population = 140;
  barracks.nation = QStringLiteral("carthage");
  barracks.spawn_order = 7;
  barracks.structure_order = 3;
  barracks.extra_fields[QStringLiteral("garrison")] = 4;

  const MapEditor::StructureElement after = round_trip(barracks);
  EXPECT_EQ(after.type, barracks.type);
  EXPECT_FLOAT_EQ(after.rotation, barracks.rotation);
  EXPECT_EQ(after.player_id, barracks.player_id);
  EXPECT_EQ(after.max_population, barracks.max_population);
  EXPECT_EQ(after.nation, barracks.nation);
  EXPECT_EQ(after.spawn_order, barracks.spawn_order);
  EXPECT_EQ(after.structure_order, barracks.structure_order);
  EXPECT_EQ(after.extra_fields, barracks.extra_fields);
}

TEST(MapEditorElementEditJsonTest, TroopSpawnRoundTripsOptionalFields) {
  MapEditor::TroopSpawnElement troop;
  troop.type = QStringLiteral("spearman");
  troop.x = 3.0F;
  troop.z = 4.0F;
  troop.player_id = 1;
  troop.max_population = 80;
  troop.behavior = QStringLiteral("guard");
  troop.guard_radius = 14.0F;
  troop.patrol_waypoints = QJsonArray{QJsonArray{1.0, 2.0}};
  troop.spawn_order = 5;

  const MapEditor::TroopSpawnElement after = round_trip(troop);
  EXPECT_EQ(after.player_id, 1);
  EXPECT_EQ(after.max_population, 80);
  EXPECT_EQ(after.behavior, troop.behavior);
  EXPECT_FLOAT_EQ(after.guard_radius, 14.0F);
  EXPECT_EQ(after.patrol_waypoints, troop.patrol_waypoints);
  EXPECT_EQ(after.spawn_order, 5);

  MapEditor::TroopSpawnElement unowned;
  unowned.type = QStringLiteral("archer");
  const MapEditor::TroopSpawnElement unowned_after = round_trip(unowned);
  EXPECT_EQ(unowned_after.player_id, -1);
  EXPECT_EQ(unowned_after.max_population, -1);
}

TEST(MapEditorElementEditJsonTest, BridgeIsRaisedToItsMinimumsWithANote) {
  MapEditor::LinearElement bridge;
  bridge.type = QStringLiteral("bridge");
  bridge.start = QVector2D(0.0F, 0.0F);
  bridge.end = QVector2D(10.0F, 0.0F);
  bridge.width = 1.0F;
  bridge.height = 0.0F;
  bridge.structure_order = 2;

  const auto result =
      EditJson::from_json(ElementSnapshot{bridge},
                          EditJson::to_document(ElementSnapshot{bridge})->json,
                          {});
  const auto& after = std::get<MapEditor::LinearElement>(result.element);
  EXPECT_FLOAT_EQ(after.height, MapEditor::k_min_bridge_height);
  EXPECT_GE(after.width, MapEditor::k_min_bridge_width);
  EXPECT_EQ(after.structure_order, 2);
  EXPECT_EQ(result.notes.size(), 2);
}

TEST(MapEditorElementEditJsonTest, WallEditsStayAxisAligned) {
  MapEditor::LinearElement wall;
  wall.type = QStringLiteral("wall");
  wall.start = QVector2D(2.0F, 2.0F);
  wall.end = QVector2D(20.0F, 2.0F);
  wall.player_id = 1;
  wall.nation = QStringLiteral("roman_republic");

  QJsonObject edited = EditJson::to_document(ElementSnapshot{wall})->json;
  edited[MapJsonKeys::end] = QJsonArray{20.0, 7.0};
  const auto result = EditJson::from_json(ElementSnapshot{wall}, edited, {});
  const auto& after = std::get<MapEditor::LinearElement>(result.element);
  EXPECT_FLOAT_EQ(after.end.y(), 2.0F);
  EXPECT_EQ(after.player_id, 1);
  EXPECT_EQ(after.nation, wall.nation);
}

TEST(MapEditorElementEditJsonTest, ZonesForestsAndWildlifeRoundTrip) {
  MapEditor::UndeadZoneElement zone;
  zone.id = QStringLiteral("crypt");
  zone.anchor_type = QStringLiteral("ruins");
  zone.radius = 11.0F;
  zone.leash_radius = 20.0F;
  zone.waves = QJsonArray{QJsonObject{{"trigger", "initial"}}};
  zone.clear_reward = QJsonObject{{"gold", 50}};
  const auto zone_after = round_trip(zone);
  EXPECT_EQ(zone_after.id, zone.id);
  EXPECT_EQ(zone_after.anchor_type, zone.anchor_type);
  EXPECT_FLOAT_EQ(zone_after.leash_radius, 20.0F);
  EXPECT_EQ(zone_after.waves, zone.waves);
  EXPECT_EQ(zone_after.clear_reward, zone.clear_reward);
  EXPECT_EQ(EditJson::to_document(ElementSnapshot{zone})->schema_sub_type,
            QStringLiteral("ruins"));

  MapEditor::ForestElement forest;
  forest.id = QStringLiteral("north_wood");
  forest.radius = 18.0F;
  EXPECT_EQ(round_trip(forest).id, forest.id);
  EXPECT_FLOAT_EQ(round_trip(forest).radius, 18.0F);

  MapEditor::WildlifeAreaElement wolves;
  wolves.species = QStringLiteral("wolves");
  wolves.radius = 22.0F;
  EXPECT_EQ(round_trip(wolves).species, wolves.species);
}

TEST(MapEditorElementEditJsonTest, EmptySnapshotHasNoDocument) {
  EXPECT_FALSE(EditJson::to_document(ElementSnapshot{}).has_value());
}

TEST(MapEditorElementEditJsonTest, PrettifiesIdentifiers) {
  EXPECT_EQ(EditJson::prettify_identifier(QStringLiteral("horse_spearman")),
            QStringLiteral("Horse Spearman"));
  EXPECT_EQ(EditJson::prettify_identifier(QStringLiteral("__")), QString());
}

TEST(MapEditorElementEditJsonTest, AddedForestsAndZonesGetUniqueIds) {
  MapEditor::MapData data;
  MapEditor::ForestElement forest;
  data.add_forest(forest);
  data.add_forest(forest);
  forest.id = QStringLiteral("forest_1");
  data.add_forest(forest);

  ASSERT_EQ(data.forests().size(), 3);
  EXPECT_EQ(data.forests()[0].id, QStringLiteral("forest_1"));
  EXPECT_EQ(data.forests()[1].id, QStringLiteral("forest_2"));
  EXPECT_EQ(data.forests()[2].id, QStringLiteral("forest_3"));

  MapEditor::UndeadZoneElement zone;
  zone.id = QStringLiteral("crypt");
  data.add_undead_zone(zone);
  data.add_undead_zone(zone);
  ASSERT_EQ(data.undead_zones().size(), 2);
  EXPECT_EQ(data.undead_zones()[0].id, QStringLiteral("crypt"));
  EXPECT_EQ(data.undead_zones()[1].id, QStringLiteral("zone_1"));
}

} // namespace
