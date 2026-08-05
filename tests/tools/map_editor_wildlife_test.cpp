#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "tools/map_editor/element_ops.h"
#include "tools/map_editor/map_data.h"

namespace {

using MapEditor::ElementKind;
using MapEditor::WildlifeAreaElement;
namespace Ops = MapEditor::ElementOps;

auto write_map(const QString& path, const QJsonObject& root) -> bool {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }
  file.write(QJsonDocument(root).toJson());
  return true;
}

auto make_area(const QString& species, float x, float z) -> WildlifeAreaElement {
  WildlifeAreaElement elem;
  elem.species = species;
  elem.x = x;
  elem.z = z;
  elem.radius = 11.0F;
  return elem;
}

TEST(MapEditorWildlifeTest, AreasBehaveLikeAnyOtherPlacedElement) {
  MapEditor::MapData data;
  data.add_wildlife_area(make_area(QStringLiteral("wolves"), 12.0F, 20.0F));

  auto const kind = static_cast<int>(ElementKind::WildlifeArea);
  EXPECT_EQ(Ops::count(data, kind), 1);

  const auto snap = Ops::snapshot(data, kind, 0);
  EXPECT_EQ(Ops::kind_of(snap), kind);
  EXPECT_EQ(Ops::type_name(snap), QStringLiteral("wolves"));
  ASSERT_TRUE(Ops::position(snap).has_value());
  EXPECT_DOUBLE_EQ(Ops::position(snap)->x(), 12.0);

  const auto moved = Ops::translated(snap, QPointF(3.0, -4.0));
  Ops::apply(data, 0, moved);
  EXPECT_FLOAT_EQ(data.wildlife_areas()[0].x, 15.0F);
  EXPECT_FLOAT_EQ(data.wildlife_areas()[0].z, 16.0F);

  auto remove = Ops::make_remove(data, kind, 0);
  ASSERT_NE(remove, nullptr);
  remove->execute();
  EXPECT_TRUE(data.wildlife_areas().isEmpty());
  remove->undo();
  ASSERT_EQ(data.wildlife_areas().size(), 1);
  EXPECT_EQ(data.wildlife_areas()[0].species, QStringLiteral("wolves"));
}

TEST(MapEditorWildlifeTest, AuthoredWildlifeBlockRoundTrips) {
  QTemporaryDir const directory;
  ASSERT_TRUE(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("map.json"));

  QJsonObject sheep{{"enabled", true},
                    {"groups", 2},
                    {"roam_radius", 12.0},
                    {"spawn_areas",
                     QJsonArray{QJsonObject{{"x", 30}, {"z", 40}, {"radius", 9}},
                                QJsonObject{{"x", 60}, {"z", 20}, {"radius", 7}}}}};
  QJsonObject birds{{"enabled", true}, {"groups", 1}, {"flyover_interval_max", 40.0}};
  QJsonObject root{
      {"name", "Wildlife Map"},
      {"grid", QJsonObject{{"width", 100}, {"height", 100}, {"tile_size", 1.0}}},
      {"wildlife",
       QJsonObject{
           {"enabled", true}, {"seed", 7}, {"sheep", sheep}, {"birds", birds}}}};
  ASSERT_TRUE(write_map(path, root));

  MapEditor::MapData data;
  QString error;
  ASSERT_TRUE(data.load_from_json(path, &error)) << error.toStdString();

  ASSERT_EQ(data.wildlife_areas().size(), 2);
  EXPECT_EQ(data.wildlife_areas()[0].species, QStringLiteral("sheep"));
  EXPECT_FLOAT_EQ(data.wildlife_areas()[0].radius, 9.0F);

  const QJsonObject saved =
      QJsonDocument::fromJson(data.to_json_string().toUtf8()).object();
  const QJsonObject wildlife = saved.value("wildlife").toObject();

  EXPECT_TRUE(wildlife.value("enabled").toBool());
  EXPECT_EQ(wildlife.value("seed").toInt(), 7);

  const QJsonObject saved_sheep = wildlife.value("sheep").toObject();
  EXPECT_DOUBLE_EQ(saved_sheep.value("roam_radius").toDouble(), 12.0);
  ASSERT_EQ(saved_sheep.value("spawn_areas").toArray().size(), 2);
  EXPECT_DOUBLE_EQ(saved_sheep.value("spawn_areas")
                       .toArray()[1]
                       .toObject()
                       .value("radius")
                       .toDouble(),
                   7.0);

  const QJsonObject saved_birds = wildlife.value("birds").toObject();
  EXPECT_DOUBLE_EQ(saved_birds.value("flyover_interval_max").toDouble(), 40.0);
  EXPECT_FALSE(saved_birds.contains("spawn_areas"));
}

TEST(MapEditorWildlifeTest, PlacingAnAreaTurnsItsSpeciesOn) {
  MapEditor::MapData data;
  data.add_wildlife_area(make_area(QStringLiteral("wolves"), 8.0F, 9.0F));

  const QJsonObject saved =
      QJsonDocument::fromJson(data.to_json_string().toUtf8()).object();
  const QJsonObject wolves =
      saved.value("wildlife").toObject().value("wolves").toObject();

  EXPECT_TRUE(saved.value("wildlife").toObject().value("enabled").toBool());
  EXPECT_TRUE(wolves.value("enabled").toBool());
  EXPECT_EQ(wolves.value("groups").toInt(), 1);
  ASSERT_EQ(wolves.value("spawn_areas").toArray().size(), 1);
}

TEST(MapEditorWildlifeTest, AShippedMapKeepsItsAuthoredPopulation) {
  const QString path = QStringLiteral("assets/maps/map_forest.json");

  QFile source(path);
  ASSERT_TRUE(source.open(QIODevice::ReadOnly));
  const QJsonObject original = QJsonDocument::fromJson(source.readAll()).object();
  ASSERT_TRUE(original.contains(QStringLiteral("wildlife")));

  MapEditor::MapData data;
  QString error;
  ASSERT_TRUE(data.load_from_json(path, &error)) << error.toStdString();

  const QJsonObject before = original.value(QStringLiteral("wildlife")).toObject();
  const QJsonObject after = QJsonDocument::fromJson(data.to_json_string().toUtf8())
                                .object()
                                .value(QStringLiteral("wildlife"))
                                .toObject();

  for (const QString& species : MapEditor::wildlife_species_keys()) {
    const QJsonObject species_before = before.value(species).toObject();
    const QJsonObject species_after = after.value(species).toObject();
    for (const QString& key : species_before.keys()) {
      EXPECT_EQ(species_after.value(key), species_before.value(key))
          << species.toStdString() << "." << key.toStdString() << " was not preserved";
    }
  }
  EXPECT_EQ(after.value(QStringLiteral("seed")), before.value(QStringLiteral("seed")));
}

TEST(MapEditorWildlifeTest, MapsWithoutWildlifeStaySilentInTheFile) {
  MapEditor::MapData data;
  MapEditor::StructureElement barracks;
  barracks.type = QStringLiteral("barracks");
  data.add_structure(barracks);

  const QJsonObject saved =
      QJsonDocument::fromJson(data.to_json_string().toUtf8()).object();
  EXPECT_FALSE(saved.contains("wildlife"));
}

} // namespace
