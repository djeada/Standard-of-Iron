#pragma once

#include "../units/troop_type.h"
#include "nation_id.h"
#include <string>
#include <vector>

namespace Engine::Core {
class World;
using EntityID = unsigned int;
} // namespace Engine::Core

namespace Game::Systems {

enum class ProductionResult {
  Success,
  NoBarracks,
  PerBarracksLimitReached,
  GlobalTroopLimitReached,
  AlreadyInProgress,
  QueueFull
};

struct ProductionState {
  bool has_barracks = false;
  bool inProgress = false;
  NationID nation_id = NationID::RomanRepublic;
  Game::Units::TroopType product_type = Game::Units::TroopType::Archer;
  float timeRemaining = 0.0F;
  float buildTime = 0.0F;
  int producedCount = 0;
  int maxUnits = 0;
  int villagerCost = 1;
  int queueSize = 0;
  std::vector<Game::Units::TroopType> productionQueue;
};

class ProductionService {
public:
  static auto startProductionForFirstSelectedBarracks(
      Engine::Core::World &world,
      const std::vector<Engine::Core::EntityID> &selected, int owner_id,
      Game::Units::TroopType unit_type) -> ProductionResult;

  static auto startProductionForFirstSelectedBarracks(
      Engine::Core::World &world,
      const std::vector<Engine::Core::EntityID> &selected, int owner_id,
      const std::string &unit_type) -> ProductionResult {
    return startProductionForFirstSelectedBarracks(
        world, selected, owner_id,
        Game::Units::troop_typeFromString(unit_type));
  }

  static auto setRallyForFirstSelectedBarracks(
      Engine::Core::World &world,
      const std::vector<Engine::Core::EntityID> &selected, int owner_id,
      float x, float z) -> bool;

  static auto
  getSelectedBarracksState(Engine::Core::World &world,
                           const std::vector<Engine::Core::EntityID> &selected,
                           int owner_id, ProductionState &outState) -> bool;
};

} // namespace Game::Systems
