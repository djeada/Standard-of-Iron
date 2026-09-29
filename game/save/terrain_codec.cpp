#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QVector3D>
#include <qglobal.h>
#include <qstringliteral.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "../map/biome_settings.h"
#include "../map/terrain.h"
#include "serialization.h"
#include "terrain_biome_codec.h"

namespace Engine::Core {

namespace {

auto encode_cell_flags(const std::vector<std::uint8_t>& flags) -> QString {
  QString encoded;
  encoded.reserve(static_cast<qsizetype>(flags.size()));
  for (std::uint8_t const flag : flags) {
    encoded.append(flag != 0U ? QLatin1Char('1') : QLatin1Char('0'));
  }
  return encoded;
}

auto decode_cell_flags(const QString& encoded) -> std::vector<std::uint8_t> {
  std::vector<std::uint8_t> flags;
  flags.reserve(static_cast<std::size_t>(encoded.size()));
  for (const QChar character : encoded) {
    flags.push_back(character == QLatin1Char('1') ? 1U : 0U);
  }
  return flags;
}

void write_grid(const Game::Map::TerrainHeightMap* height_map,
                QJsonObject& terrain_obj) {
  terrain_obj["width"] = height_map->get_width();
  terrain_obj["height"] = height_map->get_height();
  terrain_obj["tile_size"] = height_map->get_tile_size();

  QJsonArray heights_array;
  const auto& heights = height_map->get_height_data();
  for (float const h : heights) {
    heights_array.append(h);
  }
  terrain_obj["heights"] = heights_array;

  QJsonArray terrain_types_array;
  const auto& terrain_types = height_map->getTerrainTypes();
  for (auto type : terrain_types) {
    terrain_types_array.append(static_cast<int>(type));
  }
  terrain_obj["terrain_types"] = terrain_types_array;
}

void write_hills(const Game::Map::TerrainHeightMap* height_map,
                 QJsonObject& terrain_obj) {
  const auto hills = height_map->hill_navigation();
  terrain_obj["hill_walkable"] = encode_cell_flags(hills.walkable);
  terrain_obj["hill_entrances"] = encode_cell_flags(hills.entrances);
  QJsonArray hill_entrance_lines_array;
  for (const auto& line : hills.entrance_centerlines) {
    QJsonObject line_obj;
    line_obj["startX"] = line.start.x();
    line_obj["startZ"] = line.start.z();
    line_obj["endX"] = line.end.x();
    line_obj["endZ"] = line.end.z();
    hill_entrance_lines_array.append(line_obj);
  }
  terrain_obj["hill_entrance_lines"] = hill_entrance_lines_array;
}

void write_rivers(const Game::Map::TerrainHeightMap* height_map,
                  QJsonObject& terrain_obj) {
  QJsonArray rivers_array;
  const auto& rivers = height_map->get_river_segments();
  for (const auto& river : rivers) {
    QJsonObject river_obj;
    river_obj["startX"] = river.start.x();
    river_obj["startY"] = river.start.y();
    river_obj["startZ"] = river.start.z();
    river_obj["endX"] = river.end.x();
    river_obj["endY"] = river.end.y();
    river_obj["endZ"] = river.end.z();
    river_obj["width"] = river.width;
    rivers_array.append(river_obj);
  }
  terrain_obj["rivers"] = rivers_array;
}

void write_lakes(const Game::Map::TerrainHeightMap* height_map,
                 QJsonObject& terrain_obj) {
  QJsonArray lakes_array;
  for (const auto& lake : height_map->get_lakes()) {
    QJsonObject lake_obj;
    lake_obj["centerX"] = lake.center.x();
    lake_obj["centerY"] = lake.center.y();
    lake_obj["centerZ"] = lake.center.z();
    lake_obj["width"] = lake.width;
    lake_obj["depth"] = lake.depth;
    lake_obj["rotation"] = lake.rotation_deg;
    lakes_array.append(lake_obj);
  }
  terrain_obj["lakes"] = lakes_array;
}

void write_bridges(const Game::Map::TerrainHeightMap* height_map,
                   QJsonObject& terrain_obj) {
  QJsonArray bridges_array;
  const auto& bridges = height_map->get_bridges();
  for (const auto& bridge : bridges) {
    QJsonObject bridge_obj;
    bridge_obj["startX"] = bridge.start.x();
    bridge_obj["startY"] = bridge.start.y();
    bridge_obj["startZ"] = bridge.start.z();
    bridge_obj["endX"] = bridge.end.x();
    bridge_obj["endY"] = bridge.end.y();
    bridge_obj["endZ"] = bridge.end.z();
    bridge_obj["width"] = bridge.width;
    bridge_obj["height"] = bridge.height;
    bridges_array.append(bridge_obj);
  }
  terrain_obj["bridges"] = bridges_array;
}

void write_roads(const std::vector<Game::Map::RoadSegment>& roads,
                 QJsonObject& terrain_obj) {
  QJsonArray roads_array;
  for (const auto& road : roads) {
    QJsonObject road_obj;
    road_obj["startX"] = road.start.x();
    road_obj["startY"] = road.start.y();
    road_obj["startZ"] = road.start.z();
    road_obj["endX"] = road.end.x();
    road_obj["endY"] = road.end.y();
    road_obj["endZ"] = road.end.z();
    road_obj["width"] = road.width;
    road_obj["style"] = road.style;
    roads_array.append(road_obj);
  }
  terrain_obj["roads"] = roads_array;
}

auto serialize_world_props_array(const std::vector<Game::Map::WorldProp>& props)
    -> QJsonArray {
  QJsonArray world_props_array;
  for (const auto& world_prop : props) {
    QJsonObject world_prop_obj;
    world_prop_obj["id"] = static_cast<qint64>(world_prop.id);
    world_prop_obj["type"] =
        QString(Game::Map::world_prop_type_to_string(world_prop.type));
    world_prop_obj["x"] = world_prop.x;
    world_prop_obj["z"] = world_prop.z;
    world_prop_obj["scale"] = world_prop.scale;
    world_prop_obj["rotation"] = world_prop.rotation;
    if (world_prop.type == Game::Map::WorldProp::Type::FireCamp) {
      world_prop_obj["intensity"] = world_prop.intensity;
      world_prop_obj["radius"] = world_prop.radius;
      world_prop_obj["persistent"] = world_prop.persistent;
    }
    world_props_array.append(world_prop_obj);
  }
  return world_props_array;
}

void write_world_props(const std::vector<Game::Map::WorldProp>& world_props,
                       const std::vector<Game::Map::WorldProp>& authored_world_props,
                       QJsonObject& terrain_obj) {
  QJsonArray const world_props_array = serialize_world_props_array(world_props);
  terrain_obj["world_props"] = world_props_array;
  terrain_obj["authored_world_props"] =
      serialize_world_props_array(authored_world_props);
}

auto read_heights(const QJsonObject& json) -> std::vector<float> {
  std::vector<float> heights;
  if (json.contains("heights")) {
    const auto heights_array = json["heights"].toArray();
    heights.reserve(heights_array.size());
    for (const auto val : heights_array) {
      heights.push_back(static_cast<float>(val.toDouble(0.0)));
    }
  }
  return heights;
}

auto read_terrain_types(const QJsonObject& json)
    -> std::vector<Game::Map::TerrainType> {
  std::vector<Game::Map::TerrainType> terrain_types;
  if (json.contains("terrain_types")) {
    const auto types_array = json["terrain_types"].toArray();
    terrain_types.reserve(types_array.size());
    for (const auto val : types_array) {
      terrain_types.push_back(static_cast<Game::Map::TerrainType>(val.toInt(0)));
    }
  }
  return terrain_types;
}

auto read_hill_navigation(const QJsonObject& json) -> Game::Map::HillNavigation {
  Game::Map::HillNavigation hills;
  hills.walkable = decode_cell_flags(json["hill_walkable"].toString());
  hills.entrances = decode_cell_flags(json["hill_entrances"].toString());
  if (json.contains("hill_entrance_lines")) {
    const auto lines_array = json["hill_entrance_lines"].toArray();
    hills.entrance_centerlines.reserve(lines_array.size());
    for (const auto val : lines_array) {
      const auto line_obj = val.toObject();
      Game::Map::HillEntranceCenterline line;
      line.start = QVector3D(static_cast<float>(line_obj["startX"].toDouble(0.0)),
                             0.0F,
                             static_cast<float>(line_obj["startZ"].toDouble(0.0)));
      line.end = QVector3D(static_cast<float>(line_obj["endX"].toDouble(0.0)),
                           0.0F,
                           static_cast<float>(line_obj["endZ"].toDouble(0.0)));
      hills.entrance_centerlines.push_back(line);
    }
  }
  return hills;
}

auto read_rivers(const QJsonObject& json) -> std::vector<Game::Map::RiverSegment> {
  std::vector<Game::Map::RiverSegment> rivers;
  if (json.contains("rivers")) {
    const auto rivers_array = json["rivers"].toArray();
    rivers.reserve(rivers_array.size());
    const Game::Map::RiverSegment default_river{};
    for (const auto val : rivers_array) {
      const auto river_obj = val.toObject();
      Game::Map::RiverSegment river;
      river.start = QVector3D(static_cast<float>(river_obj["startX"].toDouble(0.0)),
                              static_cast<float>(river_obj["startY"].toDouble(0.0)),
                              static_cast<float>(river_obj["startZ"].toDouble(0.0)));
      river.end = QVector3D(static_cast<float>(river_obj["endX"].toDouble(0.0)),
                            static_cast<float>(river_obj["endY"].toDouble(0.0)),
                            static_cast<float>(river_obj["endZ"].toDouble(0.0)));
      river.width = static_cast<float>(
          river_obj["width"].toDouble(static_cast<double>(default_river.width)));
      rivers.push_back(river);
    }
  }
  return rivers;
}

auto read_bridges(const QJsonObject& json) -> std::vector<Game::Map::Bridge> {
  std::vector<Game::Map::Bridge> bridges;
  if (json.contains("bridges")) {
    const auto bridges_array = json["bridges"].toArray();
    bridges.reserve(bridges_array.size());
    const Game::Map::Bridge default_bridge{};
    for (const auto val : bridges_array) {
      const auto bridge_obj = val.toObject();
      Game::Map::Bridge bridge;
      bridge.start = QVector3D(static_cast<float>(bridge_obj["startX"].toDouble(0.0)),
                               static_cast<float>(bridge_obj["startY"].toDouble(0.0)),
                               static_cast<float>(bridge_obj["startZ"].toDouble(0.0)));
      bridge.end = QVector3D(static_cast<float>(bridge_obj["endX"].toDouble(0.0)),
                             static_cast<float>(bridge_obj["endY"].toDouble(0.0)),
                             static_cast<float>(bridge_obj["endZ"].toDouble(0.0)));
      bridge.width = static_cast<float>(
          bridge_obj["width"].toDouble(static_cast<double>(default_bridge.width)));
      bridge.width = std::max(bridge.width, Game::Map::k_min_bridge_width);
      bridge.height = static_cast<float>(
          bridge_obj["height"].toDouble(static_cast<double>(default_bridge.height)));
      bridges.push_back(bridge);
    }
  }
  return bridges;
}

auto read_lakes(const QJsonObject& json) -> std::vector<Game::Map::Lake> {
  std::vector<Game::Map::Lake> lakes;
  if (json.contains("lakes")) {
    const auto lakes_array = json["lakes"].toArray();
    lakes.reserve(lakes_array.size());
    const Game::Map::Lake defaults{};
    for (const auto value : lakes_array) {
      const auto lake_obj = value.toObject();
      Game::Map::Lake lake;
      lake.center = QVector3D(static_cast<float>(lake_obj["centerX"].toDouble(0.0)),
                              static_cast<float>(lake_obj["centerY"].toDouble(0.0)),
                              static_cast<float>(lake_obj["centerZ"].toDouble(0.0)));
      lake.width = static_cast<float>(lake_obj["width"].toDouble(defaults.width));
      lake.depth = static_cast<float>(lake_obj["depth"].toDouble(defaults.depth));
      lake.rotation_deg =
          static_cast<float>(lake_obj["rotation"].toDouble(defaults.rotation_deg));
      lakes.push_back(lake);
    }
  }
  return lakes;
}

void read_roads(const QJsonObject& json, std::vector<Game::Map::RoadSegment>& roads) {
  roads.clear();
  if (json.contains("roads")) {
    const auto roads_array = json["roads"].toArray();
    roads.reserve(roads_array.size());
    const Game::Map::RoadSegment default_road{};
    for (const auto val : roads_array) {
      const auto road_obj = val.toObject();
      Game::Map::RoadSegment road;
      road.start = QVector3D(static_cast<float>(road_obj["startX"].toDouble(0.0)),
                             static_cast<float>(road_obj["startY"].toDouble(0.0)),
                             static_cast<float>(road_obj["startZ"].toDouble(0.0)));
      road.end = QVector3D(static_cast<float>(road_obj["endX"].toDouble(0.0)),
                           static_cast<float>(road_obj["endY"].toDouble(0.0)),
                           static_cast<float>(road_obj["endZ"].toDouble(0.0)));
      road.width = static_cast<float>(
          road_obj["width"].toDouble(static_cast<double>(default_road.width)));
      road.style = road_obj["style"].toString(default_road.style);
      roads.push_back(road);
    }
  }
}

void append_world_props(const QJsonValue& json_value,
                        std::vector<Game::Map::WorldProp>& out_world_props) {
  if (!json_value.isArray()) {
    return;
  }
  const auto world_props_array = json_value.toArray();
  out_world_props.reserve(out_world_props.size() + world_props_array.size());
  for (const auto val : world_props_array) {
    const auto world_prop_obj = val.toObject();
    Game::Map::WorldProp world_prop;
    if (!Game::Map::world_prop_type_from_string(world_prop_obj["type"].toString(),
                                                world_prop.type)) {
      qWarning() << "Unknown world prop type in save file:"
                 << world_prop_obj["type"].toString() << "- skipping";
      continue;
    }
    world_prop.id =
        static_cast<std::uint64_t>(world_prop_obj["id"].toVariant().toULongLong());
    world_prop.x = static_cast<float>(world_prop_obj["x"].toDouble(0.0));
    world_prop.z = static_cast<float>(world_prop_obj["z"].toDouble(0.0));
    world_prop.scale = static_cast<float>(world_prop_obj["scale"].toDouble(1.0));
    world_prop.rotation = static_cast<float>(world_prop_obj["rotation"].toDouble(0.0));
    world_prop.intensity =
        static_cast<float>(world_prop_obj["intensity"].toDouble(1.0));
    world_prop.radius = static_cast<float>(world_prop_obj["radius"].toDouble(3.0));
    world_prop.persistent = world_prop_obj["persistent"].toBool(true);
    out_world_props.push_back(world_prop);
  }
}

void read_world_props(const QJsonObject& json,
                      std::vector<Game::Map::WorldProp>& world_props,
                      std::vector<Game::Map::WorldProp>& authored_world_props) {
  world_props.clear();
  if (json.contains("firecamps")) {
    const auto fire_camps_array = json["firecamps"].toArray();
    world_props.reserve(world_props.size() + fire_camps_array.size());
    for (const auto val : fire_camps_array) {
      const auto fire_camp_obj = val.toObject();
      Game::Map::WorldProp fire_camp;
      fire_camp.type = Game::Map::WorldProp::Type::FireCamp;
      fire_camp.x = static_cast<float>(fire_camp_obj["x"].toDouble(0.0));
      fire_camp.z = static_cast<float>(fire_camp_obj["z"].toDouble(0.0));
      fire_camp.intensity =
          static_cast<float>(fire_camp_obj["intensity"].toDouble(1.0));
      fire_camp.radius = static_cast<float>(fire_camp_obj["radius"].toDouble(3.0));
      fire_camp.persistent = fire_camp_obj["persistent"].toBool(true);
      world_props.push_back(fire_camp);
    }
  }

  if (json.contains("world_props")) {
    append_world_props(json["world_props"], world_props);
  }

  authored_world_props.clear();
  if (json.contains("authored_world_props")) {
    append_world_props(json["authored_world_props"], authored_world_props);
  } else {
    authored_world_props = world_props;
  }
}

} // namespace

auto Serialization::serialize_terrain(
    const Game::Map::TerrainHeightMap* height_map,
    const Game::Map::BiomeSettings& biome,
    const std::vector<Game::Map::RoadSegment>& roads,
    const std::vector<Game::Map::WorldProp>& world_props,
    const std::vector<Game::Map::WorldProp>& authored_world_props) -> QJsonObject {
  QJsonObject terrain_obj;

  if (height_map == nullptr) {
    return terrain_obj;
  }

  write_grid(height_map, terrain_obj);
  write_hills(height_map, terrain_obj);
  write_rivers(height_map, terrain_obj);
  write_lakes(height_map, terrain_obj);
  write_bridges(height_map, terrain_obj);
  write_roads(roads, terrain_obj);
  write_world_props(world_props, authored_world_props, terrain_obj);
  TerrainCodec::write_biome(biome, terrain_obj);

  return terrain_obj;
}

void Serialization::deserialize_terrain(
    Game::Map::TerrainHeightMap* height_map,
    Game::Map::BiomeSettings& biome,
    std::vector<Game::Map::RoadSegment>& roads,
    std::vector<Game::Map::WorldProp>& world_props,
    std::vector<Game::Map::WorldProp>& authored_world_props,
    const QJsonObject& json) {
  if ((height_map == nullptr) || json.isEmpty()) {
    return;
  }

  TerrainCodec::read_biome(json, biome);

  const auto heights = read_heights(json);
  const auto terrain_types = read_terrain_types(json);
  const auto hills = read_hill_navigation(json);
  const auto rivers = read_rivers(json);
  const auto bridges = read_bridges(json);
  const auto lakes = read_lakes(json);
  read_roads(json, roads);
  read_world_props(json, world_props, authored_world_props);

  height_map->restore_from_data(heights, terrain_types, rivers, bridges, lakes, hills);
}

} // namespace Engine::Core
