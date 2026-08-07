#include "defensive_unit_layout_service.h"

#include <cmath>
#include <numbers>
#include <optional>

#include "../core/component.h"
#include "../core/entity.h"
#include "../formation/unit_layout.h"
#include "../units/spawn_type.h"

namespace Game::Systems {

namespace {

[[nodiscard]] auto degrees_from_direction(float dx, float dz) -> float {
  return std::atan2(dx, dz) * 180.0F / std::numbers::pi_v<float>;
}

[[nodiscard]] auto signed_angle_delta(float from_degrees, float to_degrees) -> float {
  return std::fmod((to_degrees - from_degrees + 540.0F), 360.0F) - 180.0F;
}

[[nodiscard]] auto troop_type_of(const Engine::Core::Entity& entity)
    -> std::optional<Game::Units::TroopType> {
  const auto* unit = entity.get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return std::nullopt;
  }
  return Game::Units::spawn_typeToTroopType(unit->spawn_type);
}

[[nodiscard]] auto layout_state_is(const Engine::Core::Entity& entity,
                                   Game::Formation::UnitLayoutState state) -> bool {
  const auto* layout = entity.get_component<Engine::Core::UnitLayoutStateComponent>();
  return layout != nullptr && layout->state == static_cast<std::uint8_t>(state);
}

[[nodiscard]] auto facing_degrees_of(const Engine::Core::Entity& entity) -> float {
  const auto* transform = entity.get_component<Engine::Core::TransformComponent>();
  return transform != nullptr ? transform->rotation.y : 0.0F;
}

} // namespace

auto DefensiveUnitLayoutService::profile_for(const Engine::Core::Entity& entity)
    -> const DefensiveUnitLayoutProfile* {
  const auto* unit = entity.get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return nullptr;
  }
  const auto* nation = NationRegistry::instance().get_nation(unit->nation_id);
  if (nation == nullptr || !nation->defensive_unit_layout.has_value()) {
    return nullptr;
  }
  auto const troop = troop_type_of(entity);
  if (!troop.has_value() || !nation->defensive_unit_layout->is_eligible_troop(*troop)) {
    return nullptr;
  }
  return &nation->defensive_unit_layout.value();
}

auto DefensiveUnitLayoutService::is_active(const Engine::Core::Entity& entity) -> bool {
  return profile_for(entity) != nullptr &&
         layout_state_is(entity, Game::Formation::UnitLayoutState::Defensive);
}

auto DefensiveUnitLayoutService::is_formed(const Engine::Core::Entity& entity) -> bool {
  if (!is_active(entity)) {
    return false;
  }
  const auto* layout = entity.get_component<Engine::Core::UnitLayoutStateComponent>();
  return layout != nullptr && layout->is_formed();
}

auto DefensiveUnitLayoutService::damage_multiplier(
    const Engine::Core::Entity& target,
    const DefensiveUnitLayoutDamageContext& context) -> float {
  if (!is_formed(target)) {
    return 1.0F;
  }
  const auto* profile = profile_for(target);
  const auto* transform = target.get_component<Engine::Core::TransformComponent>();
  if (profile == nullptr || transform == nullptr) {
    return 1.0F;
  }

  float const dx = context.attack_origin.x() - transform->position.x;
  float const dz = context.attack_origin.z() - transform->position.z;
  if ((dx * dx) + (dz * dz) <= 1.0e-6F) {
    return 1.0F;
  }

  float const incoming_degrees = degrees_from_direction(dx, dz);
  float const delta =
      std::fabs(signed_angle_delta(facing_degrees_of(target), incoming_degrees));

  float multiplier = 1.0F;
  if (delta <= profile->frontal_arc_degrees * 0.5F) {
    multiplier = context.is_missile ? profile->frontal_missile_multiplier
                                    : profile->frontal_melee_multiplier;
  } else if (delta >= 180.0F - (profile->frontal_arc_degrees * 0.5F)) {
    multiplier = profile->rear_multiplier;
  } else {
    multiplier = profile->flank_multiplier;
  }
  if (context.is_cavalry_impact) {
    multiplier *= profile->cavalry_impact_multiplier;
  }
  return multiplier;
}

auto DefensiveUnitLayoutService::attack_output_multiplier(
    const Engine::Core::Entity& entity) -> float {
  const auto* profile = profile_for(entity);
  return profile != nullptr && is_formed(entity) ? profile->attack_output_multiplier
                                                 : 1.0F;
}

auto DefensiveUnitLayoutService::move_speed_multiplier(
    const Engine::Core::Entity& entity) -> float {
  const auto* profile = profile_for(entity);
  return profile != nullptr && is_formed(entity) ? profile->move_speed_multiplier
                                                 : 1.0F;
}

auto DefensiveUnitLayoutService::turn_speed_multiplier(
    const Engine::Core::Entity& entity) -> float {
  const auto* profile = profile_for(entity);
  return profile != nullptr && is_active(entity) ? profile->turn_speed_multiplier
                                                 : 1.0F;
}

auto DefensiveUnitLayoutService::blocks_charge(const Engine::Core::Entity& entity)
    -> bool {
  const auto* profile = profile_for(entity);
  return profile != nullptr && is_active(entity) && !profile->allows_charge;
}

auto DefensiveUnitLayoutService::holds_position(const Engine::Core::Entity& entity)
    -> bool {
  return is_active(entity);
}

} // namespace Game::Systems
