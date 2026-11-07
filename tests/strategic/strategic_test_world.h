#pragma once

#include <QByteArray>
#include <QDir>
#include <QString>

#include "game/strategic/strategic_json.h"
#include "game/strategic/strategic_world.h"

namespace StrategicTest {

// A deliberately small world: two factions, two mainland regions joined by a
// mountain pass, and an island reached by a sea lane. Every rule the tests
// exercise has exactly one place where it can bite.
inline auto micro_world_json() -> QByteArray {
  return R"({
  "schema_version": 1,
  "id": "micro",
  "name": "Micro World",
  "player_faction": "carthage",
  "projection": {"lon_min": 0, "lon_max": 10, "lat_min": 40, "lat_max": 46},
  "factions": [
    {"id": "carthage", "name": "Carthage", "color": "#692a5c", "capital": "harbour"},
    {"id": "rome", "name": "Rome", "color": "#b8302a", "capital": "citadel"}
  ],
  "regions": [
    {"id": "west", "name": "West", "terrain": "plains", "importance": 3, "adjacent": ["east"],
     "districts": [{"id": "coastland", "name": "Coastland",
                    "outline": [[0.5, 40.5], [3.0, 40.5], [3.0, 43.0], [0.5, 43.0]]}]},
    {"id": "east", "name": "East", "terrain": "hills", "importance": 4, "adjacent": ["west"]},
    {"id": "isle", "name": "Isle", "terrain": "hills", "importance": 1, "tags": ["island"]}
  ],
  "sites": [
    {"id": "harbour", "name": "Harbour", "kind": "city", "region": "west", "district": "coastland",
     "lonlat": [1.0, 41.0], "population": "large", "fortification": 3, "port": "major",
     "resources": {"food": 4, "manpower": 3}},
    {"id": "market", "name": "Market", "kind": "town", "region": "west", "lonlat": [3.5, 42.0],
     "population": "small", "fortification": 1, "resources": {"food": 3}},
    {"id": "saddle", "name": "Saddle", "kind": "pass", "region": "east", "lonlat": [5.0, 43.0],
     "population": "none", "fortification": 0, "terrain": "mountains",
     "battlefield": {"map": "assets/maps/map_mountain.json", "preset": "mountain_pass"}},
    {"id": "citadel", "name": "Citadel", "kind": "city", "region": "east", "lonlat": [7.0, 43.5],
     "population": "medium", "fortification": 3, "resources": {"food": 3, "iron": 2},
     "battlefield": {"mission": "siege_of_citadel"}},
    {"id": "ford", "name": "Ford", "kind": "crossing", "region": "east", "lonlat": [7.5, 44.5],
     "population": "none", "fortification": 0, "terrain": "river", "resources": {"food": 1}},
    {"id": "anchorage", "name": "Anchorage", "kind": "town", "region": "isle", "lonlat": [2.0, 45.0],
     "population": "small", "fortification": 1, "port": "minor", "resources": {"food": 2}}
  ],
  "routes": [
    {"id": "coast_road", "from": "harbour", "to": "market", "kind": "major_road", "throughput": 4},
    {"id": "ascent", "from": "market", "to": "saddle", "kind": "mountain_pass", "throughput": 1,
     "chokepoint": true},
    {"id": "descent", "from": "saddle", "to": "citadel", "kind": "road", "throughput": 3},
    {"id": "ford_road", "from": "citadel", "to": "ford", "kind": "river_crossing", "throughput": 2},
    {"id": "strait", "from": "harbour", "to": "anchorage", "kind": "sea_lane", "throughput": 2}
  ],
  "commanders": [
    {"id": "barca", "name": "Barca", "faction": "carthage"},
    {"id": "consul", "name": "Consul", "faction": "rome"},
    {"id": "legate", "name": "Legate", "faction": "rome"}
  ],
  "initial_state": {
    "regions": {"west": {"controller": "carthage"}, "east": {"controller": "rome"},
                "isle": {"controller": "rome"}},
    "sites": {"market": {"controller": "neutral"}},
    "armies": [
      {"id": "host", "name": "Host", "faction": "carthage", "commander": "barca",
       "location": "harbour", "infantry": 30000, "cavalry": 6000, "elephants": 20,
       "objective": "citadel"},
      {"id": "legion", "name": "Legion", "faction": "rome", "commander": "consul",
       "location": "citadel", "infantry": 18000, "cavalry": 1500}
    ]
  },
  "events": [
    {"id": "march", "name": "The march", "year": -218, "season": "summer", "site": "saddle",
     "effects": [{"op": "move_army", "target": "host", "to": "saddle"},
                 {"op": "set_controller", "target": "saddle", "faction": "carthage"},
                 {"op": "set_army_strength", "target": "host", "infantry": 24000}]},
    {"id": "battle", "name": "The battle", "year": -218, "season": "autumn", "site": "citadel",
     "mission": "siege_of_citadel",
     "effects": [{"op": "move_army", "target": "host", "to": "citadel"},
                 {"op": "disband_army", "target": "legion"},
                 {"op": "set_commander", "target": "consul", "status": "dead"},
                 {"op": "set_controller", "target": "citadel", "faction": "carthage"},
                 {"op": "set_contested", "target": "east"},
                 {"op": "create_army", "target": "relief", "name": "Relief", "faction": "rome",
                  "commander": "legate", "to": "ford", "infantry": 12000, "cavalry": 800},
                 {"op": "set_route_status", "target": "ford_road", "status": "blocked"},
                 {"op": "set_flag", "target": "citadel_fallen"}]}
  ]
})";
}

inline auto micro_world() -> Game::Strategic::WorldDefinition {
  return *Game::Strategic::definition_from_json(micro_world_json());
}

inline auto repository_root() -> QString {
  QDir dir = QDir::current();
  for (int depth = 0; depth < 8; ++depth) {
    if (dir.exists(QStringLiteral("assets/world"))) {
      return dir.absolutePath();
    }
    if (!dir.cdUp()) {
      break;
    }
  }
  return QDir::currentPath();
}

} // namespace StrategicTest
