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
#include "component_combat.h"

namespace Engine::Core {

class BuildingComponent {
public:
  BuildingComponent() = default;

  Game::Systems::NationID original_nation_id{Game::Systems::NationID::RomanRepublic};
};

class ProductionComponent {
public:
  ProductionComponent()
      : build_time(Defaults::k_production_default_build_time)
      ,

      max_units(Defaults::k_production_max_units) {}

  bool in_progress{false};
  float build_time;
  float time_remaining{0.0F};
  int produced_count{0};
  int max_units;
  Game::Units::TroopType product_type{Game::Units::TroopType::Archer};
  float rally_x{0.0F}, rally_z{0.0F};
  bool rally_set{false};
  int villager_cost{1};
  int manpower_available{0};

  int manpower_ceiling{0};
  std::vector<Game::Units::TroopType> production_queue;

  [[nodiscard]] auto manpower_limit() const -> int {

    return manpower_ceiling > 0 ? manpower_ceiling : max_units;
  }
};

class MoraleComponent {
public:
  MoraleComponent() = default;

  float morale{70.0F};
  float commander_aura_bonus{0.0F};
  float shock_timer{0.0F};
  bool wavering{false};
  bool routing{false};
};

class CommanderAuraBuffComponent {
public:
  CommanderAuraBuffComponent() = default;

  EntityID source_commander_id{0};
  bool active{false};
  float strength{0.0F};
  float health_regen_accumulator{0.0F};
};

inline void refresh_morale_state(MoraleComponent& morale) noexcept {
  morale.morale = std::clamp(morale.morale, 0.0F, 100.0F);
  morale.routing = morale.morale < 20.0F;
  morale.wavering = !morale.routing && morale.morale < 40.0F;
}

class UndeadComponent {
public:
  UndeadComponent() = default;

  bool morale_immune{true};
  float fire_damage_multiplier{1.5F};
  float priest_damage_multiplier{1.4F};
  float cavalry_charge_damage_multiplier{1.25F};
  bool counts_for_economy{false};
};

class CursedStatusComponent {
public:
  CursedStatusComponent() = default;

  float morale_penalty_per_hit{8.0F};
  float duration{6.0F};
  float remaining_duration{6.0F};
  int stacks{1};
};

class BurningStatusComponent {
public:
  BurningStatusComponent() = default;

  float duration{2.5F};
  float remaining_duration{2.5F};
  float ignition_elapsed{0.0F};
  float tick_interval{0.5F};
  float tick_accumulator{0.0F};
  int damage_per_tick{2};
  EntityID attacker_id{0};
  float fire_bonus_multiplier{1.0F};
};

} // namespace Engine::Core
