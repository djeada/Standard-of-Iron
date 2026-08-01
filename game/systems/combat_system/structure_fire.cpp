#include "structure_fire.h"

#include <algorithm>
#include <cmath>

#include "../../core/component.h"
#include "../../core/world.h"
#include "damage_application.h"

namespace Game::Systems::Combat {

namespace {

constexpr float k_fire_ramp_seconds = 0.6F;
constexpr float k_fire_fade_seconds = 1.2F;

[[nodiscard]] auto live_structure_unit(const Engine::Core::Entity& entity)
    -> const Engine::Core::UnitComponent* {
  if (!entity.has_component<Engine::Core::BuildingComponent>()) {
    return nullptr;
  }
  auto const* unit = entity.get_component<Engine::Core::UnitComponent>();
  return (unit != nullptr && unit->health > 0) ? unit : nullptr;
}

[[nodiscard]] auto
ignition_threshold_for(const Engine::Core::UnitComponent& unit) -> float {
  return std::max(1.0F,
                  static_cast<float>(std::max(1, unit.max_health)) *
                      k_structure_ignition_damage_fraction);
}

[[nodiscard]] auto damage_per_tick_for(const Engine::Core::UnitComponent& unit) -> int {
  return std::max(
      1,
      static_cast<int>(std::lround(static_cast<float>(std::max(1, unit.max_health)) *
                                   k_structure_fire_damage_fraction_per_tick)));
}

} // namespace

auto is_structure(const Engine::Core::Entity& entity) -> bool {
  return entity.has_component<Engine::Core::BuildingComponent>();
}

auto can_ignite_structure(const Engine::Core::Entity& entity) -> bool {
  return !entity.has_component<Engine::Core::PendingRemovalComponent>() &&
         live_structure_unit(entity) != nullptr;
}

auto structure_is_burning(const Engine::Core::Entity& entity) -> bool {
  auto const* fire = entity.get_component<Engine::Core::StructureFireComponent>();
  return fire != nullptr && fire->is_burning() &&
         live_structure_unit(entity) != nullptr;
}

auto structure_fire_intensity(const Engine::Core::Entity& entity) -> float {
  auto const* fire = entity.get_component<Engine::Core::StructureFireComponent>();
  if (fire == nullptr || !fire->is_burning() ||
      live_structure_unit(entity) == nullptr) {
    return 0.0F;
  }
  float const ramp_in =
      std::clamp(fire->ignition_elapsed / k_fire_ramp_seconds, 0.0F, 1.0F);
  float const fade_out =
      std::clamp(fire->remaining_duration / k_fire_fade_seconds, 0.0F, 1.0F);
  return ramp_in * fade_out;
}

auto apply_structure_incendiary_damage(Engine::Core::Entity& structure,
                                       int applied_damage,
                                       Engine::Core::EntityID attacker_id) -> bool {
  if (applied_damage <= 0 || !can_ignite_structure(structure)) {
    return false;
  }

  auto const* unit = structure.get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return false;
  }

  auto* fire = structure.get_component<Engine::Core::StructureFireComponent>();
  if (fire == nullptr) {
    fire = structure.add_component<Engine::Core::StructureFireComponent>();
    if (fire == nullptr) {
      return false;
    }
    fire->ignition_threshold = ignition_threshold_for(*unit);
    fire->tick_interval = k_structure_fire_tick_interval;
  }

  fire->attacker_id = attacker_id;
  fire->ignition_progress += static_cast<float>(applied_damage);
  if (fire->ignition_progress < fire->ignition_threshold) {
    return false;
  }

  fire->ignition_progress = fire->ignition_threshold;
  bool const already_burning = fire->is_burning();
  fire->duration = k_structure_fire_duration;
  fire->remaining_duration = k_structure_fire_duration;
  fire->damage_per_tick = std::max(fire->damage_per_tick, damage_per_tick_for(*unit));
  if (!already_burning) {
    fire->ignition_elapsed = 0.0F;
    fire->tick_accumulator = 0.0F;
  }
  return true;
}

void extinguish_structure_fire(Engine::Core::Entity& structure) {
  structure.remove_component<Engine::Core::StructureFireComponent>();
}

auto process_structure_fires(Engine::Core::World* world,
                             float delta_time) -> StructureFireUpdateResult {
  StructureFireUpdateResult result;
  if (world == nullptr) {
    return result;
  }

  float const step = std::max(0.0F, delta_time);
  for (auto* entity :
       world->get_entities_with<Engine::Core::StructureFireComponent>()) {
    if (entity == nullptr) {
      continue;
    }

    auto* fire = entity->get_component<Engine::Core::StructureFireComponent>();
    if (fire == nullptr) {
      continue;
    }

    if (entity->has_component<Engine::Core::PendingRemovalComponent>() ||
        live_structure_unit(*entity) == nullptr) {
      bool const was_burning = fire->is_burning();
      extinguish_structure_fire(*entity);
      result.extinguished_fires += was_burning ? 1 : 0;
      continue;
    }

    if (!fire->is_burning()) {
      fire->ignition_progress =
          std::max(0.0F,
                   fire->ignition_progress - fire->ignition_threshold * step /
                                                 k_structure_ignition_decay_seconds);
      if (fire->ignition_progress <= 0.0F) {
        extinguish_structure_fire(*entity);
      }
      continue;
    }

    ++result.burning_structures;
    fire->remaining_duration = std::max(0.0F, fire->remaining_duration - step);
    fire->ignition_elapsed += step;
    fire->tick_accumulator += step;

    float const tick_interval = std::max(0.05F, fire->tick_interval);
    while (fire->damage_per_tick > 0 && fire->tick_accumulator >= tick_interval) {
      fire->tick_accumulator -= tick_interval;
      auto const application =
          apply_unit_damage(world, entity, fire->damage_per_tick, fire->attacker_id);
      if (application.applied_damage > 0) {
        ++result.fire_ticks;
      }
      if (live_structure_unit(*entity) == nullptr) {
        extinguish_structure_fire(*entity);
        ++result.extinguished_fires;
        break;
      }
    }

    fire = entity->get_component<Engine::Core::StructureFireComponent>();
    if (fire != nullptr && fire->remaining_duration <= 0.0F) {
      extinguish_structure_fire(*entity);
      ++result.extinguished_fires;
    }
  }

  return result;
}

} // namespace Game::Systems::Combat
