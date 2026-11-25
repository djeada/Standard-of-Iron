#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "systems/nation_id.h"
#include "systems/recruitment_rules.h"
#include "units/spawn_type.h"
#include "units/troop_type.h"

namespace Engine::Core {
class World;
class ProductionComponent;
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
  QueueFull,
  // NATION_ASYMMETRY.md: not on the owner's nation's roster, missing the
  // building it needs (a Fabrica for triarii), or at its squad cap.
  NotInRoster,
  NeedsBuilding,
  TypeLimitReached
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
  bool repeat_active = false;
  Game::Units::TroopType repeat_product = Game::Units::TroopType::Archer;
  bool reserve_short = false;
  int cheapest_recruit_cost = 0;
  std::vector<Game::Units::TroopType> production_queue;
};

class ProductionService {
public:
  // `band` names the nation of a mercenary band hired at a Mercenary Camp;
  // left empty, the owner's own troop (or its default band) is meant.
  static auto
  can_start_production(Engine::Core::World& world,
                       Engine::Core::EntityID building_id,
                       Game::Units::TroopType unit_type,
                       std::optional<NationID> band = std::nullopt) -> ProductionResult;

  static auto
  start_production(Engine::Core::World& world,
                   Engine::Core::EntityID building_id,
                   Game::Units::TroopType unit_type,
                   std::optional<NationID> band = std::nullopt) -> ProductionResult;

  static constexpr int k_barracks_queue_limit = 10;
  static constexpr int k_other_queue_limit = 5;
  static constexpr int k_home_queue_limit = 3;
  static constexpr int k_recruit_batch = 5;
  static constexpr int k_repeat_lookahead = 3;

  struct BatchPlan {
    int count = 0;
    ProductionResult limit = ProductionResult::Success;
  };

  static auto plan_batch(Engine::Core::World& world,
                         Engine::Core::EntityID building_id,
                         Game::Units::TroopType unit_type,
                         int wanted) -> BatchPlan;

  static auto start_production_batch(Engine::Core::World& world,
                                     Engine::Core::EntityID building_id,
                                     Game::Units::TroopType unit_type,
                                     int count) -> int;

  static auto toggle_repeat(Engine::Core::World& world,
                            Engine::Core::EntityID building_id,
                            Game::Units::TroopType unit_type) -> bool;

  static void top_up_repeats(Engine::Core::World& world);

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

  [[nodiscard]] static auto
  cheapest_recruit_cost(Game::Units::SpawnType building) -> int;

  [[nodiscard]] static auto
  reserve_is_short(const Engine::Core::ProductionComponent& production,
                   Game::Units::SpawnType building) -> bool;

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
