#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../units/spawn_type.h"
#include "../units/troop_type.h"
#include "nation_id.h"

namespace Engine::Core {
class World;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace Game::Systems {

enum class ProductionResult {
  Success,
  NoBarracks,
  InsufficientManpower,
  InsufficientResources,
  PerBarracksLimitReached,
  WrongBuilding,
  GlobalTroopLimitReached,
  CommanderNotRecruitable,
  AlreadyInProgress,
  QueueFull
};

struct ProductionState {
  bool has_barracks = false;
  bool has_home = false;
  bool has_temple = false;
  bool in_progress = false;
  NationID nation_id = NationID::RomanRepublic;
  Game::Units::TroopType product_type = Game::Units::TroopType::Archer;
  float time_remaining = 0.0F;
  float build_time = 0.0F;
  int produced_count = 0;
  int max_units = 0;
  int villager_cost = 1;
  int manpower_available = 0;
  int queue_size = 0;
  std::vector<Game::Units::TroopType> production_queue;
};

[[nodiscard]] auto
recruiting_building_for(Game::Units::TroopType unit_type) -> Game::Units::SpawnType;

class ProductionService {
public:
  static auto
  can_start_production(Engine::Core::World& world,
                       Engine::Core::EntityID building_id,
                       Game::Units::TroopType unit_type) -> ProductionResult;

  static auto start_production(Engine::Core::World& world,
                               Engine::Core::EntityID building_id,
                               Game::Units::TroopType unit_type) -> ProductionResult;

  static auto set_rally_point(Engine::Core::World& world,
                              Engine::Core::EntityID building_id,
                              float x,
                              float z) -> bool;

  static auto
  find_selected_barracks(Engine::Core::World& world,
                         const std::vector<Engine::Core::EntityID>& selected,
                         int owner_id) -> Engine::Core::EntityID;

  static auto find_selected_home(Engine::Core::World& world,
                                 const std::vector<Engine::Core::EntityID>& selected,
                                 int owner_id) -> Engine::Core::EntityID;

  static auto find_selected_temple(Engine::Core::World& world,
                                   const std::vector<Engine::Core::EntityID>& selected,
                                   int owner_id) -> Engine::Core::EntityID;

  static auto
  get_selected_barracks_state(Engine::Core::World& world,
                              const std::vector<Engine::Core::EntityID>& selected,
                              int owner_id,
                              ProductionState& out_state) -> bool;

  static auto
  get_selected_home_state(Engine::Core::World& world,
                          const std::vector<Engine::Core::EntityID>& selected,
                          int owner_id,
                          ProductionState& out_state) -> bool;

  static auto
  get_selected_temple_state(Engine::Core::World& world,
                            const std::vector<Engine::Core::EntityID>& selected,
                            int owner_id,
                            ProductionState& out_state) -> bool;
};

} // namespace Game::Systems
