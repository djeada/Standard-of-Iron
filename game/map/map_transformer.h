#pragma once

#include "../visuals/visual_catalog.h"
#include "map_definition.h"
#include <memory>

namespace Engine {
namespace Core {
class World;
using EntityID = unsigned int;
} // namespace Core
} // namespace Engine
namespace Game {
namespace Units {
class UnitFactoryRegistry;
}
} // namespace Game

namespace Game::Map {

struct MapRuntime {
  std::vector<Engine::Core::EntityID> unitIds;
};

class MapTransformer {
public:
  static MapRuntime
  applyToWorld(const MapDefinition &def, Engine::Core::World &world,
               const Game::Visuals::VisualCatalog *visuals = nullptr);

  static void
  setFactoryRegistry(std::shared_ptr<Game::Units::UnitFactoryRegistry> reg);
  static std::shared_ptr<Game::Units::UnitFactoryRegistry> getFactoryRegistry();

  static void setLocalOwnerId(int ownerId);
  static int localOwnerId();

  static void
  setPlayerTeamOverrides(const std::unordered_map<int, int> &overrides);
  static void clearPlayerTeamOverrides();
};

} // namespace Game::Map
