#include "duel_spacing.h"

#include <algorithm>

#include "../core/component_combat.h"
#include "../core/component_commander.h"
#include "../core/component_core.h"
#include "../core/component_economy.h"
#include "../core/component_gameplay.h"
#include "../core/component_structures.h"
#include "../core/entity.h"
#include "../units/spawn_type.h"
#include "formation_combat_geometry.h"

namespace Game::Systems::DuelSpacing {

namespace {

constexpr float k_min_separation = 0.55F;
constexpr float k_base_separation_fraction = 0.70F;
constexpr float k_base_separation_min = 0.85F;
constexpr float k_base_separation_max = 1.70F;
constexpr float k_body_clearance_per_scale = 0.90F;
constexpr float k_body_min_per_scale = 0.70F;
constexpr float k_fallback_reach = 1.5F;

auto reach_of(const Engine::Core::Entity& entity) -> float {
  auto const* attack = entity.get_component<Engine::Core::AttackComponent>();
  return attack != nullptr ? std::max(0.0F, attack->melee_range) : k_fallback_reach;
}

auto scale_of(const Engine::Core::Entity& entity) -> float {
  auto const* transform = entity.get_component<Engine::Core::TransformComponent>();
  return transform != nullptr ? std::max(0.0F, transform->scale.x) : 1.0F;
}

} // namespace

auto is_duel_body(const Engine::Core::Entity& entity) -> bool {
  if (entity.has_component<Engine::Core::BuildingComponent>() ||
      entity.has_component<Engine::Core::ElephantComponent>() ||
      entity.has_component<Engine::Core::WildlifeComponent>()) {
    return false;
  }
  auto const* unit = entity.get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr || Game::Units::is_cavalry(unit->spawn_type)) {
    return false;
  }
  return !FormationCombat::has_formation_slots(entity);
}

auto standoff_between(const Engine::Core::Entity& self,
                      const Engine::Core::Entity& opponent) -> Standoff {
  float const own_reach = reach_of(self);
  float const opponent_reach = reach_of(opponent);
  float const body_scales = scale_of(self) + scale_of(opponent);
  float const weapon_span = std::min(own_reach, opponent_reach);

  Standoff result;
  result.preferred = std::max(
      std::clamp(0.5F * (own_reach + opponent_reach) * k_base_separation_fraction,
                 k_base_separation_min,
                 k_base_separation_max),
      std::min(body_scales * k_body_clearance_per_scale, weapon_span));
  result.minimum = std::max(k_min_separation, body_scales * k_body_min_per_scale);

  if (self.has_component<Engine::Core::CommanderComponent>() ||
      opponent.has_component<Engine::Core::CommanderComponent>()) {
    result.minimum = std::max(result.minimum, k_commander_min_separation);
    result.preferred = std::max(result.preferred, k_commander_preferred_separation);
  }
  result.preferred = std::max(result.preferred, result.minimum);
  return result;
}

} // namespace Game::Systems::DuelSpacing
