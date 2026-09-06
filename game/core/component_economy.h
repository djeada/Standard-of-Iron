#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../../animation/combat_manifest.h"
#include "../systems/nation_id.h"
#include "../systems/projectile_kind.h"
#include "../systems/resource_types.h"
#include "../systems/unit_activity.h"
#include "../units/spawn_type.h"
#include "../units/troop_type.h"
#include "../wildlife/wildlife_species.h"
#include "entity.h"
#include "melee_intent.h"
#include "movement_facts.h"

namespace Game::Systems {
class MovementSystem;
class RouteFollowSystem;
} // namespace Game::Systems
#include "component_gameplay.h"

namespace Engine::Core {

class HomeComponent {
public:
  HomeComponent() = default;

  int population_contribution{18};
  Engine::Core::EntityID nearest_barracks_id{0};
  float update_cooldown{0.0F};
  float family_generation_cooldown{0.0F};
  float family_generation_interval{12.0F};
  int family_manpower_value{8};
};

class CivilianDeliveryComponent {
public:
  CivilianDeliveryComponent() = default;

  EntityID target_barracks_id{0};
};

enum class CarriedFoodForm : std::uint8_t {

  Grain = 0,

  Meat = 1,
};

class ResourceCarryComponent {
public:
  ResourceCarryComponent() = default;

  [[nodiscard]] auto empty() const -> bool { return amounts.empty(); }

  [[nodiscard]] auto total() const -> int {
    int sum = 0;
    for (int const value : amounts.values) {
      sum += value;
    }
    return sum;
  }

  Game::Systems::ResourceAmounts amounts{};

  CarriedFoodForm food_form{CarriedFoodForm::Grain};

  EntityID depot_entity_id{0};
  float depot_x{0.0F};
  float depot_z{0.0F};
  bool has_depot{false};

  float haul_repath_cooldown{0.0F};
  float haul_seconds{0.0F};
};

class StockpileComponent {
public:
  StockpileComponent() = default;

  float wood_fill{0.0F};
  float stone_fill{0.0F};
  float iron_fill{0.0F};
  float food_fill{0.0F};
  float deposit_flash{0.0F};
};

inline constexpr int k_farm_growth_stage_count = 5;

class FarmComponent {
public:
  FarmComponent() = default;

  float growth{0.0F};
  float cycle_seconds{60.0F};
  int harvests{0};

  [[nodiscard]] auto ripe() const noexcept -> bool { return growth >= 1.0F; }

  [[nodiscard]] auto growth_stage() const noexcept -> int {
    if (growth >= 1.0F) {
      return k_farm_growth_stage_count - 1;
    }
    float const clamped = growth < 0.0F ? 0.0F : growth;
    int const stage =
        static_cast<int>(clamped * static_cast<float>(k_farm_growth_stage_count - 1));
    return stage < 0
               ? 0
               : (stage > k_farm_growth_stage_count - 2 ? k_farm_growth_stage_count - 2
                                                        : stage);
  }

  void reset_after_harvest() {
    growth = 0.0F;
    ++harvests;
  }
};

enum class SettlementErrand : std::uint8_t {
  Settling = 0,
  WalkingTo = 1,
  Working = 2,
  Fleeing = 3,
};

enum class SettlementErrandRole : std::uint8_t {
  Loiter = 0,
  Labour = 1,
};

inline constexpr float k_settlement_labour_cycles_per_second = 1.15F;

class SettlementResidentComponent {
public:
  SettlementResidentComponent() = default;

  float hearth_x{0.0F};
  float hearth_z{0.0F};
  float roam_radius{16.0F};
  bool hearth_assigned{false};

  bool released{false};

  SettlementErrand errand{SettlementErrand::Settling};
  SettlementErrandRole role{SettlementErrandRole::Loiter};
  EntityID focus_id{0};
  float errand_x{0.0F};
  float errand_z{0.0F};
  float focus_x{0.0F};
  float focus_z{0.0F};
  float errand_remaining{0.0F};
  float planned_dwell{3.0F};
  float think_cooldown{0.0F};
  float work_elapsed{0.0F};
  std::uint32_t rng_state{0U};

  [[nodiscard]] auto is_labouring() const -> bool {
    return errand == SettlementErrand::Working && role == SettlementErrandRole::Labour;
  }
};

class WildlifeComponent {
public:
  WildlifeComponent() = default;

  Game::Wildlife::Species species{Game::Wildlife::Species::Sheep};
  Game::Wildlife::Behavior behavior{Game::Wildlife::Behavior::Graze};

  std::uint16_t group_id{0U};
  float home_x{0.0F};
  float home_z{0.0F};
  float roam_radius{14.0F};
  bool anchor_assigned{false};

  float target_x{0.0F};
  float target_z{0.0F};
  float think_cooldown{0.0F};
  float state_timer{0.0F};
  float alarm_timer{0.0F};
  float hostile_timer{0.0F};

  static constexpr float k_bite_animation_seconds = 1.08F;
  static constexpr float k_bite_impact_phase = 0.27F;
  static constexpr float k_flinch_animation_seconds = 0.55F;
  float bite_timer{0.0F};
  float flinch_timer{0.0F};
  float held_timer{0.0F};
  int watched_health{-1};

  static constexpr float k_stall_release_seconds = 1.5F;
  float stall_timer{0.0F};
  float last_x{0.0F};
  float last_z{0.0F};
  bool stalled{false};
  EntityID bite_target_id{0};
  bool bite_impact_pending{false};
  EntityID focus_id{0};
  EntityID aggressor_id{0};
  float orbit{0.0F};
  std::uint32_t rng_state{0U};

  [[nodiscard]] auto is_hostile() const noexcept -> bool {
    return species == Game::Wildlife::Species::Wolf && hostile_timer > 0.0F;
  }
};

} // namespace Engine::Core
