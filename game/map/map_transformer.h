#pragma once

#include <QString>

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "map_definition.h"

namespace Engine::Core {
class World;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace Game::Units {
class UnitFactoryRegistry;
}

namespace Game::Map {

struct MapRuntime {
  std::vector<Engine::Core::EntityID> unit_ids;
};

struct MapTransformOptions {
  std::unordered_map<int, int> player_team_overrides;
  std::unordered_map<int, QString> base_assignments;
  std::vector<int> human_owners{1};
};

class MapTransformer {
public:
  static auto apply_to_world(const MapDefinition& def,
                             Engine::Core::World& world,
                             const MapTransformOptions& options = {}) -> MapRuntime;

  static void setFactoryRegistry(std::shared_ptr<Game::Units::UnitFactoryRegistry> reg);
  static auto
  get_factory_registry() -> std::shared_ptr<Game::Units::UnitFactoryRegistry>;
};

} // namespace Game::Map
