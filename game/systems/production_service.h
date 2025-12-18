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
  bool in_progress = false;
  NationID nation_id = NationID::RomanRepublic;
  Game::Units::TroopType product_type = Game::Units::TroopType::Archer;
  float time_remaining = 0.0F;
  float build_time = 0.0F;
  int produced_count = 0;
  int max_units = 0;
  int villager_cost = 1;
  int queue_size = 0;
  std::vector<Game::Units::TroopType> production_queue;
};

class ProductionService {
public:
  static auto start_production_for_first_selected_barracks(
      Engine::Core::World &world,
      const std::vector<Engine::Core::EntityID> &selected, int owner_id,
      Game::Units::TroopType unit_type) -> ProductionResult;

  static auto start_production_for_first_selected_barracks(
      Engine::Core::World &world,
      const std::vector<Engine::Core::EntityID> &selected, int owner_id,
      const std::string &unit_type) -> ProductionResult {
    return start_production_for_first_selected_barracks(
        world, selected, owner_id,
        Game::Units::troop_typeFromString(unit_type));
  }

  static auto set_rally_for_first_selected_barracks(
      Engine::Core::World &world,
      const std::vector<Engine::Core::EntityID> &selected, int owner_id,
      float x, float z) -> bool;

  static auto get_selected_barracks_state(
      Engine::Core::World &world,
      const std::vector<Engine::Core::EntityID> &selected, int owner_id,
      ProductionState &outState) -> bool;
};

} // namespace Game::Systems
