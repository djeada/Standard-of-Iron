#include "commander_system.h"

#include "../core/component.h"
#include "../core/world.h"
#include <algorithm>
#include <cmath>

namespace Game::Systems {
namespace {

[[nodiscard]] auto distance_sq(const Engine::Core::TransformComponent &a,
                               const Engine::Core::TransformComponent &b)
    -> float {
  const float dx = a.position.x - b.position.x;
  const float dz = a.position.z - b.position.z;
  return dx * dx + dz * dz;
}

auto morale_for(Engine::Core::Entity *entity) -> Engine::Core::MoraleComponent * {
  auto *morale = entity->get_component<Engine::Core::MoraleComponent>();
  if (morale == nullptr) {
    morale = entity->add_component<Engine::Core::MoraleComponent>();
  }
  return morale;
}

void refresh_morale_state(Engine::Core::MoraleComponent &morale) {
  morale.morale = std::clamp(morale.morale, 0.0F, 100.0F);
  morale.routing = morale.morale < 20.0F;
  morale.wavering = !morale.routing && morale.morale < 40.0F;
}

void apply_commander_death_shock(Engine::Core::World *world,
                                 Engine::Core::Entity *commander_entity,
                                 Engine::Core::CommanderComponent &commander,
                                 const Engine::Core::UnitComponent &unit,
                                 const Engine::Core::TransformComponent &origin) {
  const float shock_radius_sq =
      commander.death_shock_radius * commander.death_shock_radius;
  for (auto *candidate : world->get_entities_with<Engine::Core::UnitComponent>()) {
    if (candidate == commander_entity) {
      continue;
    }
    auto *candidate_unit =
        candidate->get_component<Engine::Core::UnitComponent>();
    auto *candidate_transform =
        candidate->get_component<Engine::Core::TransformComponent>();
    if ((candidate_unit == nullptr) || (candidate_transform == nullptr) ||
        candidate_unit->owner_id != unit.owner_id || candidate_unit->health <= 0) {
      continue;
    }
    if (distance_sq(origin, *candidate_transform) > shock_radius_sq) {
      continue;
    }
    if (auto *morale = morale_for(candidate)) {
      morale->morale -= commander.death_morale_shock;
      morale->shock_timer = std::max(morale->shock_timer, 4.0F);
      refresh_morale_state(*morale);
    }
  }
}

} // namespace

void CommanderSystem::update(Engine::Core::World *world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  for (auto *entity : world->get_entities_with<Engine::Core::MoraleComponent>()) {
    auto *morale = entity->get_component<Engine::Core::MoraleComponent>();
    if (morale == nullptr) {
      continue;
    }
    morale->commander_aura_bonus = 0.0F;
    morale->shock_timer = std::max(0.0F, morale->shock_timer - delta_time);
  }

  for (auto *commander_entity :
       world->get_entities_with<Engine::Core::CommanderComponent>()) {
    auto *commander =
        commander_entity->get_component<Engine::Core::CommanderComponent>();
    auto *unit = commander_entity->get_component<Engine::Core::UnitComponent>();
    auto *transform =
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
        apply_commander_death_shock(world, commander_entity, *commander, *unit,
                                    *transform);
      }
      continue;
    }

    if (commander->wounded || !commander->aura_active) {
      continue;
    }

    const float aura_radius_sq = commander->aura_radius * commander->aura_radius;
    const float rally_radius_sq =
        commander->rally_range * commander->rally_range;
    bool rallied_this_tick = false;
    for (auto *candidate : world->get_entities_with<Engine::Core::UnitComponent>()) {
      if (candidate == commander_entity) {
        continue;
      }
      auto *candidate_unit =
          candidate->get_component<Engine::Core::UnitComponent>();
      auto *candidate_transform =
          candidate->get_component<Engine::Core::TransformComponent>();
      if ((candidate_unit == nullptr) || (candidate_transform == nullptr) ||
          candidate_unit->owner_id != unit->owner_id ||
          candidate_unit->health <= 0) {
        continue;
      }

      const float dist_sq = distance_sq(*transform, *candidate_transform);
      if (dist_sq <= aura_radius_sq) {
        if (auto *morale = morale_for(candidate)) {
          morale->commander_aura_bonus =
              std::max(morale->commander_aura_bonus,
                       commander->aura_morale_bonus);
          morale->morale += commander->aura_morale_bonus * delta_time * 0.05F;
          refresh_morale_state(*morale);
        }
      }

      if (!rallied_this_tick && commander->rally_cooldown_remaining <= 0.0F &&
          dist_sq <= rally_radius_sq) {
        if (auto *morale = candidate->get_component<Engine::Core::MoraleComponent>();
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
