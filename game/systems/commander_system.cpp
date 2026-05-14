#include "commander_system.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include "../core/component.h"
#include "../core/world.h"
#include "troop_profile_service.h"
#include "units/spawn_type.h"

namespace Game::Systems {
namespace {

[[nodiscard]] auto distance_sq(const Engine::Core::TransformComponent& a,
                               const Engine::Core::TransformComponent& b) -> float {
  const float dx = a.position.x - b.position.x;
  const float dz = a.position.z - b.position.z;
  return dx * dx + dz * dz;
}

auto morale_for(Engine::Core::Entity* entity) -> Engine::Core::MoraleComponent* {
  auto* morale = entity->get_component<Engine::Core::MoraleComponent>();
  if (morale == nullptr) {
    morale = entity->add_component<Engine::Core::MoraleComponent>();
  }
  return morale;
}

void refresh_morale_state(Engine::Core::MoraleComponent& morale) {
  morale.morale = std::clamp(morale.morale, 0.0F, 100.0F);
  morale.routing = morale.morale < 20.0F;
  morale.wavering = !morale.routing && morale.morale < 40.0F;
}

auto resolve_troop_type(const Engine::Core::UnitComponent* unit)
    -> std::optional<Game::Units::TroopType> {
  if (unit == nullptr) {
    return std::nullopt;
  }
  return Game::Units::spawn_typeToTroopType(unit->spawn_type);
}

auto is_living_troop(const Engine::Core::UnitComponent* unit) -> bool {
  return unit != nullptr && unit->health > 0 &&
         Game::Units::is_troop_spawn(unit->spawn_type);
}

auto try_trigger_rally(Engine::Core::World* world,
                       Engine::Core::Entity* commander_entity,
                       Engine::Core::CommanderComponent& commander,
                       const Engine::Core::UnitComponent& unit,
                       const Engine::Core::TransformComponent& origin) -> bool {
  if (world == nullptr || commander_entity == nullptr ||
      commander.rally_cooldown_remaining > 0.0F) {
    return false;
  }

  const float rally_radius_sq = commander.rally_range * commander.rally_range;
  for (auto* candidate : world->get_entities_with<Engine::Core::UnitComponent>()) {
    if (candidate == commander_entity) {
      continue;
    }

    auto* candidate_unit = candidate->get_component<Engine::Core::UnitComponent>();
    auto* candidate_transform =
        candidate->get_component<Engine::Core::TransformComponent>();
    if (!is_living_troop(candidate_unit) || candidate_transform == nullptr ||
        candidate_unit->owner_id != unit.owner_id ||
        distance_sq(origin, *candidate_transform) > rally_radius_sq) {
      continue;
    }

    auto* morale = candidate->get_component<Engine::Core::MoraleComponent>();
    if (morale == nullptr || (!morale->wavering && !morale->routing)) {
      continue;
    }

    morale->morale += commander.rally_morale_restore;
    morale->shock_timer = 0.0F;
    refresh_morale_state(*morale);
    commander.rally_cooldown_remaining = commander.rally_cooldown;
    commander.rally_feedback_time = 1.5F;
    return true;
  }

  return false;
}

void reset_commander_modified_stats(Engine::Core::World* world) {
  for (auto* entity : world->get_entities_with<Engine::Core::UnitComponent>()) {
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (!is_living_troop(unit)) {
      continue;
    }
    const auto troop_type = resolve_troop_type(unit);
    if (!troop_type.has_value()) {
      continue;
    }

    const auto profile =
        TroopProfileService::instance().get_profile(unit->nation_id, *troop_type);
    unit->speed = profile.combat.speed;

    if (auto* attack = entity->get_component<Engine::Core::AttackComponent>()) {
      attack->damage = profile.combat.ranged_damage;
      attack->melee_damage = profile.combat.melee_damage;
    }
  }
}

void apply_commander_death_shock(Engine::Core::World* world,
                                 Engine::Core::Entity* commander_entity,
                                 Engine::Core::CommanderComponent& commander,
                                 const Engine::Core::UnitComponent& unit,
                                 const Engine::Core::TransformComponent& origin) {
  const float shock_radius_sq =
      commander.death_shock_radius * commander.death_shock_radius;
  for (auto* candidate : world->get_entities_with<Engine::Core::UnitComponent>()) {
    if (candidate == commander_entity) {
      continue;
    }
    auto* candidate_unit = candidate->get_component<Engine::Core::UnitComponent>();
    auto* candidate_transform =
        candidate->get_component<Engine::Core::TransformComponent>();
    if (!is_living_troop(candidate_unit) || (candidate_transform == nullptr) ||
        candidate_unit->owner_id != unit.owner_id) {
      continue;
    }
    if (distance_sq(origin, *candidate_transform) > shock_radius_sq) {
      continue;
    }
    if (auto* morale = morale_for(candidate)) {
      morale->morale -= commander.death_morale_shock;
      morale->shock_timer = std::max(morale->shock_timer, 4.0F);
      refresh_morale_state(*morale);
    }
  }
}

} // namespace

void CommanderSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  reset_commander_modified_stats(world);

  for (auto* entity : world->get_entities_with<Engine::Core::MoraleComponent>()) {
    auto* morale = entity->get_component<Engine::Core::MoraleComponent>();
    if (morale == nullptr) {
      continue;
    }
    morale->commander_aura_bonus = 0.0F;
    morale->shock_timer = std::max(0.0F, morale->shock_timer - delta_time);
  }

  for (auto* commander_entity :
       world->get_entities_with<Engine::Core::CommanderComponent>()) {
    auto* commander =
        commander_entity->get_component<Engine::Core::CommanderComponent>();
    auto* unit = commander_entity->get_component<Engine::Core::UnitComponent>();
    auto* transform =
        commander_entity->get_component<Engine::Core::TransformComponent>();
    if ((commander == nullptr) || (unit == nullptr) || (transform == nullptr)) {
      continue;
    }

    commander->rally_cooldown_remaining =
        std::max(0.0F, commander->rally_cooldown_remaining - delta_time);
    commander->rally_feedback_time =
        std::max(0.0F, commander->rally_feedback_time - delta_time);

    if (unit->health <= 0) {
      if (!commander->wounded) {
        commander->wounded = true;
        commander->aura_active = false;
        apply_commander_death_shock(
            world, commander_entity, *commander, *unit, *transform);
      }
      continue;
    }

    if (commander->wounded || !commander->aura_active) {
      continue;
    }

    if (commander->rally_requested) {
      commander->rally_requested = false;
      (void)try_trigger_rally(world, commander_entity, *commander, *unit, *transform);
    }

    const float aura_radius_sq = commander->aura_radius * commander->aura_radius;
    const float rally_radius_sq = commander->rally_range * commander->rally_range;
    bool rallied_this_tick = false;
    for (auto* candidate : world->get_entities_with<Engine::Core::UnitComponent>()) {
      if (candidate == commander_entity) {
        continue;
      }
      auto* candidate_unit = candidate->get_component<Engine::Core::UnitComponent>();
      auto* candidate_transform =
          candidate->get_component<Engine::Core::TransformComponent>();
      if ((candidate_unit == nullptr) || (candidate_transform == nullptr) ||
          candidate_unit->owner_id != unit->owner_id) {
        continue;
      }

      const float dist_sq = distance_sq(*transform, *candidate_transform);
      const bool candidate_is_troop = is_living_troop(candidate_unit);
      if (dist_sq <= aura_radius_sq) {
        if (candidate_is_troop) {
          auto* morale = morale_for(candidate);
          if (morale != nullptr) {
            morale->commander_aura_bonus =
                std::max(morale->commander_aura_bonus, commander->aura_morale_bonus);
            morale->morale += commander->aura_morale_bonus * delta_time * 0.05F;
            refresh_morale_state(*morale);
          }
        }

        if (candidate_is_troop && commander->bonus_type == "health_regen") {
          candidate_unit->health = std::min(
              candidate_unit->max_health,
              candidate_unit->health + static_cast<int>(std::round(
                                           commander->aura_bonus_value * delta_time)));
        } else if (candidate_is_troop && commander->bonus_type == "attack_boost") {
          if (auto* attack =
                  candidate->get_component<Engine::Core::AttackComponent>()) {
            const float factor = 1.0F + commander->aura_bonus_value;
            attack->damage =
                std::max(1, static_cast<int>(std::round(attack->damage * factor)));
            attack->melee_damage = std::max(
                1, static_cast<int>(std::round(attack->melee_damage * factor)));
          }
        } else if (candidate_is_troop && commander->bonus_type == "speed_boost") {
          candidate_unit->speed *= (1.0F + commander->aura_bonus_value);
        } else if (commander->bonus_type == "production_haste") {
          if (auto* production =
                  candidate->get_component<Engine::Core::ProductionComponent>()) {
            if (production->in_progress) {
              const float haste = std::max(0.0F, commander->aura_bonus_value);
              production->time_remaining =
                  std::max(0.0F, production->time_remaining - delta_time * haste);
            }
          }
        }
      }

      if (!commander->rally_requires_manual_trigger && !rallied_this_tick &&
          commander->rally_cooldown_remaining <= 0.0F && candidate_is_troop &&
          dist_sq <= rally_radius_sq) {
        if (auto* morale = candidate->get_component<Engine::Core::MoraleComponent>();
            morale != nullptr && (morale->wavering || morale->routing)) {
          morale->morale += commander->rally_morale_restore;
          morale->shock_timer = 0.0F;
          refresh_morale_state(*morale);
          commander->rally_cooldown_remaining = commander->rally_cooldown;
          commander->rally_feedback_time = 1.5F;
          rallied_this_tick = true;
        }
      }
    }
  }
}

} // namespace Game::Systems
