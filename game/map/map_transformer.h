#pragma once

#include <QString>

#include <cstdint>
#include <memory>
#include <unordered_map>

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
  bool spectator_mode = false;
};

class MapTransformer {
public:
  static auto apply_to_world(const MapDefinition& def,
                             Engine::Core::World& world,
                             const MapTransformOptions& options = {}) -> MapRuntime;

  static void setFactoryRegistry(std::shared_ptr<Game::Units::UnitFactoryRegistry> reg);
  static auto
  get_factory_registry() -> std::shared_ptr<Game::Units::UnitFactoryRegistry>;

  static void set_local_owner_id(int owner_id);
  static auto local_owner_id() -> int;
};

} // namespace Game::Map
