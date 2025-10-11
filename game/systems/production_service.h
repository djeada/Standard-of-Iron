#pragma once

#include <string>
#include <vector>

namespace Engine {
namespace Core {
class World;
using EntityID = unsigned int;
} // namespace Core
} // namespace Engine

namespace Game {
namespace Systems {

struct ProductionState {
  bool hasBarracks = false;
  bool inProgress = false;
  float timeRemaining = 0.0f;
  float buildTime = 0.0f;
  int producedCount = 0;
  int maxUnits = 0;
};

class ProductionService {
public:
  static bool startProductionForFirstSelectedBarracks(
      Engine::Core::World &world,
      const std::vector<Engine::Core::EntityID> &selected, int ownerId,
      const std::string &unitType);

  static bool setRallyForFirstSelectedBarracks(
      Engine::Core::World &world,
      const std::vector<Engine::Core::EntityID> &selected, int ownerId, float x,
      float z);

  static bool
  getSelectedBarracksState(Engine::Core::World &world,
                           const std::vector<Engine::Core::EntityID> &selected,
                           int ownerId, ProductionState &outState);
};

} // namespace Systems
} // namespace Game
