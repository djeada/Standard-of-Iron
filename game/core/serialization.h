#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <vector>

#include "../map/map_definition.h"

namespace Game::Map {
class TerrainHeightMap;
struct BiomeSettings;
} // namespace Game::Map

namespace Engine::Core {

class Serialization {
public:
  static auto serialize_entity(const class Entity* entity) -> QJsonObject;
  static void deserialize_entity(class Entity* entity, const QJsonObject& json);

  static auto serialize_world(const class World* world) -> QJsonDocument;
  static void deserialize_world(class World* world, const QJsonDocument& doc);

  static auto serialize_terrain(const Game::Map::TerrainHeightMap* height_map,
                                const Game::Map::BiomeSettings& biome,
                                const std::vector<Game::Map::RoadSegment>& roads,
                                const std::vector<Game::Map::WorldProp>& world_props)
      -> QJsonObject;
  static void deserialize_terrain(Game::Map::TerrainHeightMap* height_map,
                                  Game::Map::BiomeSettings& biome,
                                  std::vector<Game::Map::RoadSegment>& roads,
                                  std::vector<Game::Map::WorldProp>& world_props,
                                  const QJsonObject& json);

  static auto save_to_file(const QString& filename, const QJsonDocument& doc) -> bool;
  static auto load_from_file(const QString& filename) -> QJsonDocument;
};

} // namespace Engine::Core
