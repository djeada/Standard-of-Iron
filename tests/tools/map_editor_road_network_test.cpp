#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <gtest/gtest.h>
#include <string>

#include "map_data.h"
#include "road_network.h"

namespace {

using MapEditor::LinearElement;

auto road(QPointF a,
          QPointF b,
          const QString& type = QStringLiteral("road")) -> LinearElement {
  LinearElement element;
  element.type = type;
  element.start = QVector2D(static_cast<float>(a.x()), static_cast<float>(a.y()));
  element.end = QVector2D(static_cast<float>(b.x()), static_cast<float>(b.y()));
  element.waypoints = {a, b};
  return element;
}

auto repository_root() -> QDir {
  QDir dir = QDir::current();
  for (int depth = 0; depth < 8; ++depth) {
    if (dir.exists(QStringLiteral("assets/maps"))) {
      return dir;
    }
    if (!dir.cdUp()) {
      break;
    }
  }
  return QDir::current();
}

TEST(RoadNetwork, TJunctionsAndCrossingsJoinRoads) {
  const auto report = MapEditor::analyse_road_network({
      road({0, 50}, {100, 50}),
      road({50, 50}, {50, 0}),
      road({20, 80}, {20, 20}),
  });
  EXPECT_EQ(report.road_count, 3);
  EXPECT_TRUE(report.connected()) << report.summary().toStdString();
}

TEST(RoadNetwork, ABridgeCarriesTheRoadOverTheRiver) {
  QVector<LinearElement> elements = {
      road({0, 50}, {40, 50}),
      road({60, 50}, {100, 50}),
      road({50, 0}, {50, 100}, QStringLiteral("river")),
  };
  EXPECT_EQ(MapEditor::analyse_road_network(elements).groups.size(), 2);
  elements.append(road({40, 50}, {60, 50}, QStringLiteral("bridge")));
  const auto report = MapEditor::analyse_road_network(elements);
  EXPECT_TRUE(report.connected());
  EXPECT_EQ(report.connector_count, 1);
}

TEST(RoadNetwork, AFordInTheRiverIsNotASeparateRoadNetwork) {
  const auto report = MapEditor::analyse_road_network({
      road({0, 50}, {100, 50}),
      road({300, 20}, {300, 60}, QStringLiteral("ford")),
  });
  EXPECT_TRUE(report.connected()) << report.summary().toStdString();
}

TEST(RoadNetwork, AStrayBridgeIsNotOfferedAsASnapTarget) {
  const auto report = MapEditor::analyse_road_network({
      road({0, 50}, {100, 50}),
      road({150, 50}, {200, 50}),
      road({204, 50}, {210, 50}, QStringLiteral("bridge")),
  });
  EXPECT_EQ(report.groups.size(), 2);
  EXPECT_TRUE(report.near_misses.isEmpty())
      << "joining a bridge that leads nowhere connects nothing";
}

TEST(RoadNetwork, SnappingExtendsTheRoadSoSharedJunctionsSurvive) {
  QVector<LinearElement> elements = {
      road({0, 50}, {100, 50}),
      road({50, 46}, {50, 0}),
      road({50, 46}, {80, 0}),
  };
  const auto report = MapEditor::analyse_road_network(elements);
  ASSERT_EQ(report.near_misses.size(), 1);
  const auto& miss = report.near_misses.first();
  elements[miss.element] = MapEditor::snapped(elements[miss.element], miss);
  EXPECT_TRUE(MapEditor::analyse_road_network(elements).connected())
      << "the other road still meets the snapped one at the old junction";
}

TEST(RoadNetwork, NearMissesAreFoundAndSnappedClosed) {
  QVector<LinearElement> elements = {
      road({0, 50}, {100, 50}),
      road({50, 46}, {50, 0}),
      road({200, 200}, {260, 200}),
  };
  const auto report = MapEditor::analyse_road_network(elements);
  ASSERT_EQ(report.groups.size(), 3);
  ASSERT_EQ(report.near_misses.size(), 1)
      << "the far road is a separate network, not a slip";
  const auto& miss = report.near_misses.first();
  EXPECT_EQ(miss.element, 1);
  EXPECT_TRUE(miss.at_start);
  EXPECT_NEAR(miss.gap, 4.0, 1e-6);
  elements[1] = MapEditor::snapped(elements[1], miss);
  EXPECT_NEAR(elements[1].waypoints.first().y(), 50.0, 1e-6);
  EXPECT_NEAR(elements[1].waypoints.at(1).y(), 46.0, 1e-6)
      << "the old end stays as a bend";
  EXPECT_EQ(MapEditor::analyse_road_network(elements).groups.size(), 2);
}

TEST(RoadNetwork, ShippedBattlefieldsThatAreConnectedStayConnected) {
  const QDir root = repository_root();
  for (const char* name : {"map_battle_cannae.json",
                           "map_battle_trebia.json",
                           "map_battle_zama.json",
                           "map_crossing_alps.json",
                           "map_victumulae.json",
                           "map_sepulcher_bridge.json",
                           "map_iron_sepulcher_watch.json",
                           "map_pinewater_cut.json",
                           "map_battle_trasimene.json",
                           "map_campania_campaign.json",
                           "map_mountain.json",
                           "map_spanish_grove.json",
                           "map_rivers.json",
                           "map_amber_delta.json",
                           "map_copper_canyons.json",
                           "map_forest.json"}) {
    MapEditor::MapData data;
    QString error;
    ASSERT_TRUE(data.load_from_json(
        root.filePath(QStringLiteral("assets/maps/%1").arg(name)), &error))
        << name << ": " << error.toStdString();
    const auto report = MapEditor::analyse_road_network(data.linear_elements());
    std::string groups;
    for (const auto& group : report.groups) {
      groups += "\n  [";
      for (const int index : group) {
        const auto& e = data.linear_elements().at(index);
        groups += std::to_string(index) + ":" + e.type.toStdString() + "(" +
                  std::to_string(e.start.x()) + "," + std::to_string(e.start.y()) +
                  "->" + std::to_string(e.end.x()) + "," + std::to_string(e.end.y()) +
                  ") ";
      }
      groups += "]";
    }
    EXPECT_TRUE(report.connected())
        << name << ": " << report.summary().toStdString() << groups;
  }
}

TEST(RoadNetwork, AnAuthoredEndpointBeyondTheWaypointsIsKeptAsTheGameDrawsIt) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString path = dir.filePath(QStringLiteral("tail.json"));
  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  file.write(R"({"name": "tail", "grid": {"width": 100, "height": 100, "tile_size": 1},
    "roads": [{"start": [0, 50], "end": [90, 50], "width": 3,
               "waypoints": [[6, 50], [40, 50], [90, 50]]}]})");
  file.close();
  MapEditor::MapData data;
  ASSERT_TRUE(data.load_from_json(path));
  const auto& road = data.linear_elements().first();
  ASSERT_EQ(road.waypoints.size(), 4)
      << "the game draws start to the first waypoint; so must the editor";
  EXPECT_EQ(road.waypoints.first(), QPointF(0, 50));
  EXPECT_FLOAT_EQ(road.start.x(), 0.0F);
}

TEST(RoadNetwork, ShippedRoadsListTheirEndpointsAsWaypoints) {
  const QDir maps(repository_root().filePath(QStringLiteral("assets/maps")));
  for (const auto& name : maps.entryList({QStringLiteral("*.json")}, QDir::Files)) {
    QFile file(maps.filePath(name));
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    const auto roads = QJsonDocument::fromJson(file.readAll())
                           .object()
                           .value(QStringLiteral("roads"))
                           .toArray();
    for (int i = 0; i < roads.size(); ++i) {
      const QJsonObject road = roads[i].toObject();
      const QJsonArray waypoints = road.value(QStringLiteral("waypoints")).toArray();
      if (waypoints.isEmpty()) {
        continue;
      }
      EXPECT_EQ(waypoints.first().toArray(),
                road.value(QStringLiteral("start")).toArray())
          << name.toStdString() << " road " << i
          << ": the game draws start to the first waypoint";
      EXPECT_EQ(waypoints.last().toArray(), road.value(QStringLiteral("end")).toArray())
          << name.toStdString() << " road " << i
          << ": the game draws the last waypoint to end";
    }
  }
}

} // namespace
